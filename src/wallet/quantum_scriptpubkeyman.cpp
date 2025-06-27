// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/quantum_scriptpubkeyman.h>
#include <wallet/walletdb.h>
#include <key_io.h>
#include <logging.h>
#include <script/script.h>
#include <script/solver.h>
#include <script/quantum_signature.h>
#include <util/bip32.h>
#include <util/translation.h>
#include <common/signmessage.h>
#include <hash.h>
#include <wallet/crypter.h>
#include <streams.h>

namespace wallet {

util::Result<CTxDestination> QuantumScriptPubKeyMan::GetNewDestination(const OutputType type)
{
    LOCK(cs_key_man);
    
    // Top up keypool if needed
    TopUp();
    
    // Check if we have keys in the pool
    if (m_keypool.empty()) {
        return util::Error{strprintf(_("Keypool ran out, please call keypoolrefill"))};
    }
    
    // Find a key from the keypool that matches the desired type
    CKeyID keyid;
    CQuantumPubKey pubkey;
    bool found = false;
    
    // Determine the desired key type based on address type
    ::quantum::KeyType desired_key_type = (m_address_type == QuantumAddressType::P2QPKH_ML_DSA) ? 
        ::quantum::KeyType::ML_DSA_65 : ::quantum::KeyType::SLH_DSA_192F;
    
    // First, try to find a key of the desired type in the pool
    for (auto it = m_keypool.begin(); it != m_keypool.end(); ++it) {
        if (GetQuantumPubKey(*it, pubkey)) {
            if (pubkey.GetType() == desired_key_type) {
                keyid = *it;
                m_keypool.erase(it);
                found = true;
                break;
            }
        }
    }
    
    // If no key of the desired type found, generate one directly
    if (!found) {
        // Generate new quantum key of the specific type
        auto key = std::make_unique<CQuantumKey>();
        key->MakeNewKey(desired_key_type);
        
        if (!key->IsValid()) {
            return util::Error{strprintf(_("Failed to generate quantum key"))};
        }
        
        pubkey = key->GetPubKey();
        keyid = pubkey.GetID();
        
        // Add to internal storage
        m_quantum_pubkeys[keyid] = pubkey;
        
        // Get a batch for database operations
        WalletBatch batch(m_storage.GetDatabase());
        
        // Write public key to database
        if (!WriteQuantumPubKey(keyid, pubkey, batch)) {
            return util::Error{strprintf(_("Failed to write quantum public key"))};
        }
        
        // Store the key
        if (m_encrypted && !m_master_key.empty()) {
            // Encrypt the key
            std::vector<unsigned char> encrypted_key;
            if (!::wallet::EncryptQuantumKey(m_master_key, *key, pubkey, encrypted_key)) {
                return util::Error{strprintf(_("Failed to encrypt quantum key"))};
            }
            m_encrypted_keys[keyid] = encrypted_key;
            
            // Write encrypted key to database
            CKeyMetadata meta(GetTime());
            if (!batch.WriteCryptedQuantumKey(keyid, encrypted_key, meta)) {
                return util::Error{strprintf(_("Failed to write encrypted quantum key"))};
            }
        } else {
            // Write unencrypted key to database
            if (!WriteQuantumKey(keyid, *key, batch)) {
                return util::Error{strprintf(_("Failed to write quantum key"))};
            }
            
            // Store unencrypted in memory
            m_quantum_keys[keyid] = std::move(key);
        }
    }
    
    // Mark as used
    m_used_keys.insert(keyid);
    
    // Get a batch for database operations
    WalletBatch batch(m_storage.GetDatabase());
    
    // Return the destination
    if (m_address_type == QuantumAddressType::P2QSH) {
        // Create P2QSH script
        ::quantum::KeyType keyType = (m_address_type == QuantumAddressType::P2QPKH_ML_DSA) ? 
            ::quantum::KeyType::ML_DSA_65 : ::quantum::KeyType::SLH_DSA_192F;
        uint256 hash = ::quantum::QuantumHash256(pubkey.GetKeyData());
        CScript script = ::quantum::CreateP2QPKHScript(hash, keyType);
        CScriptID scriptid(script);
        m_quantum_scripts[scriptid] = m_address_type;
        
        // Persist script to database
        if (!WriteQuantumScript(scriptid, m_address_type, batch)) {
            return util::Error{strprintf(_("Failed to write quantum script"))};
        }
        
        return util::Result<CTxDestination>(ScriptHash(scriptid));
    } else {
        // P2QPKH - return PKHash directly
        return util::Result<CTxDestination>(PKHash(keyid));
    }
}

isminetype QuantumScriptPubKeyMan::IsMine(const CScript& script) const
{
    LOCK(cs_key_man);
    
    std::vector<std::vector<unsigned char>> solutions;
    TxoutType type = Solver(script, solutions);
    
    switch (type) {
        case TxoutType::PUBKEYHASH:
        {
            if (solutions.size() == 1 && solutions[0].size() == 20) {
                CKeyID keyid = CKeyID(uint160(solutions[0]));
                if (HaveQuantumKey(keyid)) {
                    return ISMINE_SPENDABLE;
                }
            }
            break;
        }
        case TxoutType::SCRIPTHASH:
        {
            if (solutions.size() == 1 && solutions[0].size() == 20) {
                CScriptID scriptid = CScriptID(uint160(solutions[0]));
                if (m_quantum_scripts.count(scriptid) > 0) {
                    return ISMINE_SPENDABLE;
                }
            }
            break;
        }
        default:
            break;
    }
    
    return ISMINE_NO;
}

bool QuantumScriptPubKeyMan::CheckDecryptionKey(const CKeyingMaterial& master_key)
{
    LOCK(cs_key_man);
    
    if (!m_encrypted) {
        return false;
    }
    
    // Try to decrypt a key to verify the master key
    for (const auto& [keyid, encrypted_key] : m_encrypted_keys) {
        CQuantumPubKey pubkey;
        if (GetQuantumPubKey(keyid, pubkey)) {
            CQuantumKey decrypted_key;
            if (::wallet::DecryptQuantumKey(master_key, encrypted_key, pubkey, decrypted_key)) {
                // Successfully decrypted - master key is correct
                return true;
            }
        }
    }
    
    return false;
}

bool QuantumScriptPubKeyMan::Encrypt(const CKeyingMaterial& master_key, WalletBatch* batch)
{
    LOCK(cs_key_man);
    
    if (m_encrypted) {
        return false;
    }
    
    // Encrypt all quantum keys
    for (const auto& [keyid, key] : m_quantum_keys) {
        if (!key) continue;
        
        std::vector<unsigned char> encrypted_key;
        CQuantumPubKey pubkey = key->GetPubKey();
        if (!::wallet::EncryptQuantumKey(master_key, *key, pubkey, encrypted_key)) {
            return false;
        }
        
        m_encrypted_keys[keyid] = encrypted_key;
        
        if (batch) {
            // Write encrypted key to database
            CKeyMetadata meta(GetTime());
            if (!batch->WriteCryptedQuantumKey(keyid, encrypted_key, meta)) {
                return false;
            }
        }
    }
    
    // Clear unencrypted keys
    m_quantum_keys.clear();
    m_encrypted = true;
    m_master_key = master_key;
    
    return true;
}

util::Result<CTxDestination> QuantumScriptPubKeyMan::GetReservedDestination(const OutputType type, bool internal, int64_t& index)
{
    LOCK(cs_key_man);
    
    // Top up keypool if needed
    TopUp();
    
    if (m_keypool.empty()) {
        return util::Error{strprintf(_("Keypool ran out, please call keypoolrefill"))};
    }
    
    // Get a key from the pool
    auto it = m_keypool.begin();
    CKeyID keyid = *it;
    m_keypool.erase(it);
    
    // Use the keypool size as index
    index = m_keypool.size();
    
    // Mark as used
    m_used_keys.insert(keyid);
    
    // Get a batch for database operations
    WalletBatch batch(m_storage.GetDatabase());
    
    // Return appropriate destination
    if (m_address_type == QuantumAddressType::P2QSH) {
        CQuantumPubKey pubkey;
        if (!GetQuantumPubKey(keyid, pubkey)) {
            return util::Error{strprintf(_("Public key not found"))};
        }
        ::quantum::KeyType keyType = (m_address_type == QuantumAddressType::P2QPKH_ML_DSA) ? 
            ::quantum::KeyType::ML_DSA_65 : ::quantum::KeyType::SLH_DSA_192F;
        uint256 hash = ::quantum::QuantumHash256(pubkey.GetKeyData());
        CScript script = ::quantum::CreateP2QPKHScript(hash, keyType);
        CScriptID scriptid(script);
        
        // Store and persist script
        m_quantum_scripts[scriptid] = m_address_type;
        if (!WriteQuantumScript(scriptid, m_address_type, batch)) {
            return util::Error{strprintf(_("Failed to write quantum script"))};
        }
        
        return util::Result<CTxDestination>(ScriptHash(scriptid));
    } else {
        return util::Result<CTxDestination>(PKHash(keyid));
    }
}

void QuantumScriptPubKeyMan::KeepDestination(int64_t index, const OutputType& type)
{
    // Key has been used, nothing special to do
}

void QuantumScriptPubKeyMan::ReturnDestination(int64_t index, bool internal, const CTxDestination& addr)
{
    LOCK(cs_key_man);
    
    // Try to return key to pool
    const PKHash* pkhash = std::get_if<PKHash>(&addr);
    if (pkhash) {
        CKeyID keyid(static_cast<uint160>(*pkhash));
        if (m_used_keys.count(keyid) > 0) {
            m_used_keys.erase(keyid);
            m_keypool.insert(keyid);
        }
    }
}

bool QuantumScriptPubKeyMan::TopUp(unsigned int size)
{
    LOCK(cs_key_man);
    
    unsigned int target_size = size > 0 ? size : 100; // Default keypool size
    
    LogPrintf("QuantumScriptPubKeyMan::TopUp: Starting keypool top-up, current size=%u, target size=%u, address_type=%d\n", 
              m_keypool.size(), target_size, static_cast<int>(m_address_type));
    
    while (m_keypool.size() < target_size) {
        LogPrintf("QuantumScriptPubKeyMan::TopUp: Generating key %u/%u\n", m_keypool.size() + 1, target_size);
        
        // Generate new quantum key
        auto key = std::make_unique<CQuantumKey>();
        
        ::quantum::KeyType key_type;
        switch (m_address_type) {
            case QuantumAddressType::P2QPKH_ML_DSA:
                key_type = ::quantum::KeyType::ML_DSA_65;
                break;
            case QuantumAddressType::P2QPKH_SLH_DSA:
                key_type = ::quantum::KeyType::SLH_DSA_192F;
                break;
            default:
                key_type = ::quantum::KeyType::ML_DSA_65;
                break;
        }
        
        key->MakeNewKey(key_type);
        
        LogPrintf("QuantumScriptPubKeyMan::TopUp: Key generated, checking validity\n");
        
        if (!key->IsValid()) {
            LogPrintf("QuantumScriptPubKeyMan::TopUp: Key generation failed - invalid key\n");
            return false;
        }
        
        LogPrintf("QuantumScriptPubKeyMan::TopUp: Getting public key\n");
        CQuantumPubKey pubkey = key->GetPubKey();
        LogPrintf("QuantumScriptPubKeyMan::TopUp: Getting key ID\n");
        CKeyID keyid = pubkey.GetID();
        LogPrintf("QuantumScriptPubKeyMan::TopUp: Key ID obtained: %s\n", keyid.ToString());
        
        // Add to internal storage
        LogPrintf("QuantumScriptPubKeyMan::TopUp: Adding to internal storage\n");
        m_quantum_pubkeys[keyid] = pubkey;
        
        // Get a batch for database operations
        LogPrintf("QuantumScriptPubKeyMan::TopUp: Getting wallet batch\n");
        WalletBatch batch(m_storage.GetDatabase());
        LogPrintf("QuantumScriptPubKeyMan::TopUp: Wallet batch obtained\n");
        
        // Write public key to database
        if (!WriteQuantumPubKey(keyid, pubkey, batch)) {
            return false;
        }
        
        if (m_encrypted && !m_master_key.empty()) {
            // Encrypt the key
            std::vector<unsigned char> encrypted_key;
            if (!::wallet::EncryptQuantumKey(m_master_key, *key, pubkey, encrypted_key)) {
                return false;
            }
            m_encrypted_keys[keyid] = encrypted_key;
            
            // Write encrypted key to database
            CKeyMetadata meta(GetTime());
            if (!batch.WriteCryptedQuantumKey(keyid, encrypted_key, meta)) {
                return false;
            }
        } else {
            // Write unencrypted key to database
            if (!WriteQuantumKey(keyid, *key, batch)) {
                return false;
            }
            
            // Store unencrypted in memory
            m_quantum_keys[keyid] = std::move(key);
        }
        
        // Add to keypool
        m_keypool.insert(keyid);
    }
    
    return true;
}

unsigned int QuantumScriptPubKeyMan::GetKeyPoolSize() const
{
    LOCK(cs_key_man);
    return m_keypool.size();
}

bool QuantumScriptPubKeyMan::SignTransaction(CMutableTransaction& tx, const std::map<COutPoint, Coin>& coins, int sighash, std::map<int, bilingual_str>& input_errors) const
{
    LOCK(cs_key_man);
    
    // For each input
    for (unsigned int i = 0; i < tx.vin.size(); ++i) {
        const CTxIn& txin = tx.vin[i];
        
        auto it = coins.find(txin.prevout);
        if (it == coins.end()) {
            continue;
        }
        
        const Coin& coin = it->second;
        const CTxOut& txout = coin.out;
        
        // Check if this is ours
        if (IsMine(txout.scriptPubKey) == ISMINE_NO) {
            continue;
        }
        
        // Extract the public key hash
        std::vector<std::vector<unsigned char>> solutions;
        TxoutType type = Solver(txout.scriptPubKey, solutions);
        
        if (type == TxoutType::PUBKEYHASH && solutions.size() == 1) {
            CKeyID keyid = CKeyID(uint160(solutions[0]));
            
            // Get the quantum key
            const CQuantumKey* key = nullptr;
            if (!GetQuantumKey(keyid, &key) || !key) {
                input_errors[i] = _("Private key not available");
                continue;
            }
            
            // Create signature
            uint256 hash = SignatureHash(txout.scriptPubKey, tx, i, sighash, txout.nValue, SigVersion::BASE);
            std::vector<unsigned char> sig;
            
            if (!key->Sign(hash, sig)) {
                input_errors[i] = _("Signing failed");
                continue;
            }
            
            // Add sighash type
            sig.push_back(static_cast<unsigned char>(sighash));
            
            // Get public key
            CQuantumPubKey pubkey;
            if (!GetQuantumPubKey(keyid, pubkey)) {
                input_errors[i] = _("Public key not found");
                continue;
            }
            
            // Create signature script
            tx.vin[i].scriptSig = CScript() << sig << pubkey.GetKeyData();
        }
    }
    
    return true;
}

SigningResult QuantumScriptPubKeyMan::SignMessage(const std::string& message, const PKHash& pkhash, std::string& str_sig) const
{
    LOCK(cs_key_man);
    
    CKeyID keyid(static_cast<uint160>(pkhash));
    
    const CQuantumKey* key = nullptr;
    if (!GetQuantumKey(keyid, &key) || !key) {
        return SigningResult::PRIVATE_KEY_NOT_AVAILABLE;
    }
    
    HashWriter ss{};
    ss << MESSAGE_MAGIC << message;
    uint256 hash = ss.GetHash();
    
    std::vector<unsigned char> sig;
    if (!key->Sign(hash, sig)) {
        return SigningResult::SIGNING_FAILED;
    }
    
    // Add recovery information and encode
    CQuantumPubKey pubkey;
    if (!GetQuantumPubKey(keyid, pubkey)) {
        return SigningResult::SIGNING_FAILED;
    }
    
    // Encode signature with public key for verification
    DataStream ds{};
    uint8_t scheme_id = (pubkey.GetType() == ::quantum::KeyType::ML_DSA_65) ? 
        ::quantum::SCHEME_ML_DSA_65 : ::quantum::SCHEME_SLH_DSA_192F;
    ds << scheme_id << sig << pubkey.GetKeyData();
    
    str_sig = EncodeBase64(ds);
    
    return SigningResult::OK;
}

std::optional<common::PSBTError> QuantumScriptPubKeyMan::FillPSBT(PartiallySignedTransaction& psbt, const PrecomputedTransactionData& txdata, std::optional<int> sighash_type, bool sign, bool bip32derivs, int* n_signed, bool finalize) const
{
    // TODO: Implement PSBT support for quantum signatures
    return common::PSBTError::UNSUPPORTED;
}

bool QuantumScriptPubKeyMan::CanProvideImpl(const CScript& script, SignatureData& sigdata) const
{
    LOCK(cs_key_man);
    
    std::vector<std::vector<unsigned char>> solutions;
    TxoutType type = Solver(script, solutions);
    
    if (type == TxoutType::PUBKEYHASH && solutions.size() == 1) {
        CKeyID keyid = CKeyID(uint160(solutions[0]));
        return HaveQuantumKey(keyid);
    } else if (type == TxoutType::SCRIPTHASH && solutions.size() == 1) {
        CScriptID scriptid = CScriptID(uint160(solutions[0]));
        return m_quantum_scripts.count(scriptid) > 0;
    }
    
    return false;
}

bool QuantumScriptPubKeyMan::HavePrivateKeys() const
{
    LOCK(cs_key_man);
    return !m_quantum_keys.empty() || !m_encrypted_keys.empty();
}

int64_t QuantumScriptPubKeyMan::GetTimeFirstKey() const
{
    // Quantum keys don't have timestamps in our implementation
    return 0;
}

std::unique_ptr<SigningProvider> QuantumScriptPubKeyMan::GetSolvingProvider(const CScript& script) const
{
    // TODO: Implement signing provider for quantum signatures
    return nullptr;
}

bool QuantumScriptPubKeyMan::AddQuantumKey(std::unique_ptr<CQuantumKey> key, const CQuantumPubKey& pubkey)
{
    LOCK(cs_key_man);
    
    CKeyID keyid = pubkey.GetID();
    
    // Store public key
    m_quantum_pubkeys[keyid] = pubkey;
    
    // Get a batch for database operations
    WalletBatch batch(m_storage.GetDatabase());
    
    // Write public key to database
    if (!WriteQuantumPubKey(keyid, pubkey, batch)) {
        return false;
    }
    
    // Store private key (encrypted or not)
    if (m_encrypted && !m_master_key.empty()) {
        std::vector<unsigned char> encrypted_key;
        if (!::wallet::EncryptQuantumKey(m_master_key, *key, pubkey, encrypted_key)) {
            return false;
        }
        m_encrypted_keys[keyid] = encrypted_key;
        
        // Write encrypted key to database
        CKeyMetadata meta(GetTime());
        if (!batch.WriteCryptedQuantumKey(keyid, encrypted_key, meta)) {
            return false;
        }
    } else {
        // Write unencrypted key to database
        if (!WriteQuantumKey(keyid, *key, batch)) {
            return false;
        }
        
        m_quantum_keys[keyid] = std::move(key);
    }
    
    return true;
}

bool QuantumScriptPubKeyMan::GetQuantumKey(const CKeyID& keyid, const CQuantumKey** key) const
{
    LOCK(cs_key_man);
    
    // If not encrypted, return from unencrypted storage
    if (!m_encrypted) {
        auto it = m_quantum_keys.find(keyid);
        if (it != m_quantum_keys.end() && it->second) {
            *key = it->second.get();
            return true;
        }
        return false;
    }
    
    // If encrypted, we need to decrypt on demand
    // For now, we don't support on-demand decryption in const context
    // This would require mutable decrypted key cache
    return false;
}

bool QuantumScriptPubKeyMan::GetQuantumPubKey(const CKeyID& keyid, CQuantumPubKey& pubkey) const
{
    LOCK(cs_key_man);
    
    auto it = m_quantum_pubkeys.find(keyid);
    if (it != m_quantum_pubkeys.end()) {
        pubkey = it->second;
        return true;
    }
    
    return false;
}

bool QuantumScriptPubKeyMan::HaveQuantumKey(const CKeyID& keyid) const
{
    LOCK(cs_key_man);
    return m_quantum_keys.count(keyid) > 0 || m_encrypted_keys.count(keyid) > 0;
}

std::set<CKeyID> QuantumScriptPubKeyMan::GetQuantumKeys() const
{
    LOCK(cs_key_man);
    std::set<CKeyID> keys;
    
    for (const auto& [keyid, key] : m_quantum_keys) {
        keys.insert(keyid);
    }
    
    for (const auto& [keyid, encrypted] : m_encrypted_keys) {
        keys.insert(keyid);
    }
    
    return keys;
}

void QuantumScriptPubKeyMan::SetQuantumAddressType(QuantumAddressType type)
{
    LOCK(cs_key_man);
    m_address_type = type;
}

QuantumAddressType QuantumScriptPubKeyMan::GetQuantumAddressType() const
{
    LOCK(cs_key_man);
    return m_address_type;
}

int QuantumScriptPubKeyMan::GetQuantumTypeForAddress(const CTxDestination& dest) const
{
    LOCK(cs_key_man);
    
    // Check if it's a PKHash (P2QPKH address)
    const PKHash* pkhash = std::get_if<PKHash>(&dest);
    if (pkhash) {
        CKeyID keyid(static_cast<uint160>(*pkhash));
        // Check if we have this quantum key
        if (HaveQuantumKey(keyid)) {
            // Check the public key to determine the type
            CQuantumPubKey pubkey;
            if (GetQuantumPubKey(keyid, pubkey)) {
                if (pubkey.GetType() == ::quantum::KeyType::ML_DSA_65) {
                    return 1; // Q1 prefix
                } else if (pubkey.GetType() == ::quantum::KeyType::SLH_DSA_192F) {
                    return 2; // Q2 prefix
                }
            }
        }
    }
    
    // Check if it's a ScriptHash (P2QSH address)
    const ScriptHash* scripthash = std::get_if<ScriptHash>(&dest);
    if (scripthash) {
        CScriptID scriptid(static_cast<uint160>(*scripthash));
        auto it = m_quantum_scripts.find(scriptid);
        if (it != m_quantum_scripts.end()) {
            return 3; // Q3 prefix for P2QSH
        }
    }
    
    return 0; // Not a quantum address
}

bool QuantumScriptPubKeyMan::LoadKey(const CKeyID& keyid, std::unique_ptr<CQuantumKey> key)
{
    LOCK(cs_key_man);
    m_quantum_keys[keyid] = std::move(key);
    return true;
}

bool QuantumScriptPubKeyMan::LoadPubKey(const CKeyID& keyid, const CQuantumPubKey& pubkey)
{
    LOCK(cs_key_man);
    m_quantum_pubkeys[keyid] = pubkey;
    return true;
}

bool QuantumScriptPubKeyMan::LoadScript(const CScriptID& scriptid, QuantumAddressType type)
{
    LOCK(cs_key_man);
    m_quantum_scripts[scriptid] = type;
    return true;
}

bool QuantumScriptPubKeyMan::WriteQuantumKey(const CKeyID& keyid, const CQuantumKey& key, WalletBatch& batch)
{
    // Get the private key data
    ::quantum::secure_vector privKeyData = key.GetPrivKeyData();
    
    // Convert secure_vector to regular vector for storage
    std::vector<unsigned char> keyData(privKeyData.begin(), privKeyData.end());
    
    // Create metadata
    CKeyMetadata meta(GetTime());
    
    // Write to database
    return batch.WriteQuantumKey(keyid, keyData, meta);
}

bool QuantumScriptPubKeyMan::WriteQuantumPubKey(const CKeyID& keyid, const CQuantumPubKey& pubkey, WalletBatch& batch)
{
    // Get the raw public key data
    std::vector<unsigned char> pubkeyData = pubkey.GetKeyData();
    
    // Write to database
    return batch.WriteQuantumPubKey(keyid, pubkeyData);
}

bool QuantumScriptPubKeyMan::WriteQuantumScript(const CScriptID& scriptid, QuantumAddressType type, WalletBatch& batch)
{
    // Convert address type to uint8_t
    uint8_t typeValue = static_cast<uint8_t>(type);
    
    // Write to database
    return batch.WriteQuantumScript(scriptid, typeValue);
}

} // namespace wallet