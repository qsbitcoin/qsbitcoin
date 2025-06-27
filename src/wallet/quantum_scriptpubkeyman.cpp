// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/quantum_scriptpubkeyman.h>
#include <common/signmessage.h>
#include <key_io.h>
#include <logging.h>
#include <script/script.h>
#include <script/sign.h>
#include <script/signingprovider.h>
#include <script/solver.h>
#include <util/strencodings.h>
#include <util/time.h>
#include <util/translation.h>
#include <wallet/walletdb.h>

namespace wallet {

// Import quantum types
using quantum::CQuantumKey;
using quantum::CQuantumPubKey;
using quantum::QuantumAddressType;

// Helper functions for quantum addresses
static bool ExtractQuantumDestination(const CScript& script, CTxDestination& dest, QuantumAddressType& addr_type)
{
    // Simple implementation - just check for P2PKH pattern
    std::vector<std::vector<unsigned char>> vSolutions;
    TxoutType whichType = Solver(script, vSolutions);
    
    if (whichType == TxoutType::PUBKEYHASH && vSolutions.size() > 0) {
        PKHash hash;
        std::copy(vSolutions[0].begin(), vSolutions[0].end(), hash.begin());
        dest = hash;
        addr_type = QuantumAddressType::P2QPKH_ML_DSA; // Default
        return true;
    }
    return false;
}

static CScript GetScriptForQuantumDestination(const CQuantumPubKey& pubkey, QuantumAddressType addr_type)
{
    // Simple P2PKH-style script
    CKeyID keyid = pubkey.GetID();
    return CScript() << OP_DUP << OP_HASH160 << ToByteVector(keyid) << OP_EQUALVERIFY << OP_CHECKSIG;
}

static CScript CreateQuantumScriptSig(const std::vector<unsigned char>& sig, const CQuantumPubKey& pubkey, QuantumAddressType addr_type)
{
    // Create script sig with quantum signature format
    // For now, just use the raw key data
    const std::vector<unsigned char>& pubkey_data = pubkey.GetKeyData();
    return CScript() << sig << pubkey_data;
}

util::Result<CTxDestination> QuantumScriptPubKeyMan::GetNewDestination(const OutputType type)
{
    LOCK(cs_key_man);
    
    // Top up keypool if needed
    if (!TopUp()) {
        return util::Error{Untranslated("Failed to top up keypool")};
    }
    
    // Get a key from the keypool
    if (m_keypool.empty()) {
        return util::Error{Untranslated("Keypool is empty")};
    }
    
    auto it = m_keypool.begin();
    CKeyID keyid = *it;
    m_keypool.erase(it);
    m_used_keys.insert(keyid);
    
    // Get the public key
    CQuantumPubKey pubkey;
    if (!GetQuantumPubKey(keyid, pubkey)) {
        return util::Error{Untranslated("Failed to get quantum public key")};
    }
    
    // Create destination based on address type
    CTxDestination dest;
    switch (m_address_type) {
        case QuantumAddressType::P2QPKH_ML_DSA:
        case QuantumAddressType::P2QPKH_SLH_DSA:
            dest = PKHash(keyid);
            break;
        case QuantumAddressType::P2QSH:
            // For P2QSH, we create a script containing the public key
            CScript script = GetScriptForQuantumDestination(pubkey, m_address_type);
            CScriptID scriptid(script);
            WalletBatch batch(m_storage.GetDatabase());
            WriteQuantumScript(scriptid, m_address_type, batch);
            dest = ScriptHash(scriptid);
            break;
    }
    
    return dest;
}

isminetype QuantumScriptPubKeyMan::IsMine(const CScript& script) const
{
    LOCK(cs_key_man);
    
    // Check if this is a quantum script
    QuantumAddressType addr_type;
    CTxDestination dest;
    if (!ExtractQuantumDestination(script, dest, addr_type)) {
        return ISMINE_NO;
    }
    
    // Check if we have the key
    if (auto* pkhash = std::get_if<PKHash>(&dest)) {
        CKeyID keyid(ToKeyID(*pkhash));
        if (HaveQuantumKey(keyid)) {
            return ISMINE_SPENDABLE;
        }
    } else if (auto* scripthash = std::get_if<ScriptHash>(&dest)) {
        CScriptID scriptid{uint160(*scripthash)};
        auto it = m_quantum_scripts.find(scriptid);
        if (it != m_quantum_scripts.end()) {
            return ISMINE_SPENDABLE;
        }
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
    for (const auto& [keyid, encrypted_key] : m_quantum_keys) {
        CQuantumKey key;
        // TODO: Implement decryption once we have encrypted key storage
        // For now, assume keys are not encrypted
        return true;
    }
    
    return false;
}

bool QuantumScriptPubKeyMan::Encrypt(const CKeyingMaterial& master_key, WalletBatch* batch)
{
    LOCK(cs_key_man);
    
    if (m_encrypted) {
        return false;
    }
    
    m_master_key = master_key;
    m_encrypted = true;
    
    // TODO: Implement key encryption
    // For now, we'll just mark as encrypted without actually encrypting
    
    return true;
}

bool QuantumScriptPubKeyMan::TopUp(unsigned int size)
{
    LOCK(cs_key_man);
    
    unsigned int target_size = size > 0 ? size : DEFAULT_KEYPOOL_SIZE;
    unsigned int current_size = m_keypool.size();
    
    if (current_size >= target_size) {
        return true;
    }
    
    WalletBatch batch(m_storage.GetDatabase());
    
    // Generate new keys
    for (unsigned int i = current_size; i < target_size; ++i) {
        // Generate new quantum key
        auto key = std::make_unique<CQuantumKey>();
        quantum::KeyType keyType = (m_address_type == QuantumAddressType::P2QPKH_SLH_DSA) ? 
                                   quantum::KeyType::SLH_DSA_192F : quantum::KeyType::ML_DSA_65;
        key->MakeNewKey(keyType);
        if (!key->IsValid()) {
            LogPrintf("QuantumScriptPubKeyMan::TopUp: Failed to generate quantum key\n");
            return false;
        }
        
        CQuantumPubKey pubkey = key->GetPubKey();
        CKeyID keyid = pubkey.GetID();
        
        // Write to database first
        if (!WriteQuantumKey(keyid, *key, batch) || !WriteQuantumPubKey(keyid, pubkey, batch)) {
            LogPrintf("QuantumScriptPubKeyMan::TopUp: Failed to write quantum key to database\n");
            return false;
        }
        
        // Store the key
        if (!AddQuantumKey(std::move(key), pubkey)) {
            LogPrintf("QuantumScriptPubKeyMan::TopUp: Failed to add quantum key\n");
            return false;
        }
        
        // Add to keypool
        m_keypool.insert(keyid);
    }
    
    LogPrintf("QuantumScriptPubKeyMan::TopUp: Added %u keys to keypool\n", target_size - current_size);
    return true;
}

bool QuantumScriptPubKeyMan::SignTransaction(CMutableTransaction& tx, const std::map<COutPoint, Coin>& coins, int sighash, std::map<int, bilingual_str>& input_errors) const
{
    LOCK(cs_key_man);
    
    bool any_signed = false;
    
    for (unsigned int i = 0; i < tx.vin.size(); ++i) {
        auto it = coins.find(tx.vin[i].prevout);
        if (it == coins.end() || it->second.IsSpent()) {
            continue;
        }
        
        const CTxOut& utxo = it->second.out;
        SignatureData sigdata;
        
        // Check if we can sign this input
        if (!CanProvideImpl(utxo.scriptPubKey, sigdata)) {
            continue;
        }
        
        // Get the quantum key
        QuantumAddressType addr_type;
        CTxDestination dest;
        if (!ExtractQuantumDestination(utxo.scriptPubKey, dest, addr_type)) {
            continue;
        }
        
        CKeyID keyid;
        if (auto* pkhash = std::get_if<PKHash>(&dest)) {
            keyid = ToKeyID(*pkhash);
        } else {
            continue; // P2QSH not yet implemented for signing
        }
        
        const CQuantumKey* key;
        if (!GetQuantumKey(keyid, &key)) {
            input_errors[i] = Untranslated("Quantum key not found");
            continue;
        }
        
        // Create signature
        uint256 hash = SignatureHash(utxo.scriptPubKey, tx, i, sighash, utxo.nValue, SigVersion::BASE);
        std::vector<unsigned char> sig;
        if (!key->Sign(hash, sig)) {
            input_errors[i] = Untranslated("Failed to create quantum signature");
            continue;
        }
        
        // Add signature to scriptSig
        CQuantumPubKey pubkey = key->GetPubKey();
        tx.vin[i].scriptSig = CreateQuantumScriptSig(sig, pubkey, addr_type);
        any_signed = true;
    }
    
    return any_signed;
}

SigningResult QuantumScriptPubKeyMan::SignMessage(const std::string& message, const PKHash& pkhash, std::string& str_sig) const
{
    LOCK(cs_key_man);
    
    CKeyID keyid(ToKeyID(pkhash));
    const CQuantumKey* key;
    if (!GetQuantumKey(keyid, &key)) {
        return SigningResult::PRIVATE_KEY_NOT_AVAILABLE;
    }
    
    // Create message hash
    uint256 hash = MessageHash(message);
    
    // Sign with quantum key
    std::vector<unsigned char> sig;
    if (!key->Sign(hash, sig)) {
        return SigningResult::SIGNING_FAILED;
    }
    
    // Encode signature
    str_sig = EncodeBase64(sig);
    return SigningResult::OK;
}

std::optional<common::PSBTError> QuantumScriptPubKeyMan::FillPSBT(PartiallySignedTransaction& psbt, const PrecomputedTransactionData& txdata, std::optional<int> sighash_type, bool sign, bool bip32derivs, int* n_signed, bool finalize) const
{
    // TODO: Implement PSBT support for quantum signatures
    return std::nullopt;
}

bool QuantumScriptPubKeyMan::CanProvideImpl(const CScript& script, SignatureData& sigdata) const
{
    LOCK(cs_key_man);
    
    QuantumAddressType addr_type;
    CTxDestination dest;
    if (!ExtractQuantumDestination(script, dest, addr_type)) {
        return false;
    }
    
    if (auto* pkhash = std::get_if<PKHash>(&dest)) {
        CKeyID keyid(ToKeyID(*pkhash));
        return HaveQuantumKey(keyid);
    } else if (auto* scripthash = std::get_if<ScriptHash>(&dest)) {
        CScriptID scriptid{uint160(*scripthash)};
        return m_quantum_scripts.find(scriptid) != m_quantum_scripts.end();
    }
    
    return false;
}

bool QuantumScriptPubKeyMan::HavePrivateKeys() const
{
    LOCK(cs_key_man);
    return !m_quantum_keys.empty();
}

int64_t QuantumScriptPubKeyMan::GetTimeFirstKey() const
{
    LOCK(cs_key_man);
    // Quantum keys don't have timestamps in our current implementation
    return GetTime();
}

std::unique_ptr<SigningProvider> QuantumScriptPubKeyMan::GetSolvingProvider(const CScript& script) const
{
    LOCK(cs_key_man);
    
    auto provider = std::make_unique<FlatSigningProvider>();
    
    QuantumAddressType addr_type;
    CTxDestination dest;
    if (!ExtractQuantumDestination(script, dest, addr_type)) {
        return provider;
    }
    
    if (auto* pkhash = std::get_if<PKHash>(&dest)) {
        CKeyID keyid(ToKeyID(*pkhash));
        const CQuantumKey* key;
        CQuantumPubKey pubkey;
        if (GetQuantumKey(keyid, &key) && GetQuantumPubKey(keyid, pubkey)) {
            // Note: FlatSigningProvider doesn't support quantum keys yet
            // This would need to be extended to support quantum signing
        }
    }
    
    return provider;
}

bool QuantumScriptPubKeyMan::AddQuantumKey(std::unique_ptr<CQuantumKey> key, const CQuantumPubKey& pubkey)
{
    LOCK(cs_key_man);
    
    CKeyID keyid = pubkey.GetID();
    m_quantum_keys[keyid] = std::move(key);
    m_quantum_pubkeys[keyid] = pubkey;
    
    return true;
}

bool QuantumScriptPubKeyMan::GetQuantumKey(const CKeyID& keyid, const CQuantumKey** key) const
{
    LOCK(cs_key_man);
    
    auto it = m_quantum_keys.find(keyid);
    if (it != m_quantum_keys.end()) {
        *key = it->second.get();
        return true;
    }
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
    return m_quantum_keys.find(keyid) != m_quantum_keys.end();
}

std::set<CKeyID> QuantumScriptPubKeyMan::GetQuantumKeys() const
{
    LOCK(cs_key_man);
    std::set<CKeyID> keys;
    for (const auto& [keyid, _] : m_quantum_keys) {
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
    // TODO: Implement database writing for quantum keys
    // For now, just return true
    return true;
}

bool QuantumScriptPubKeyMan::WriteQuantumPubKey(const CKeyID& keyid, const CQuantumPubKey& pubkey, WalletBatch& batch)
{
    // TODO: Implement database writing for quantum public keys
    // For now, just return true
    return true;
}

bool QuantumScriptPubKeyMan::WriteQuantumScript(const CScriptID& scriptid, QuantumAddressType type, WalletBatch& batch)
{
    // TODO: Implement database writing for quantum scripts
    // For now, just return true
    return true;
}

util::Result<CTxDestination> QuantumScriptPubKeyMan::GetReservedDestination(const OutputType type, bool internal, int64_t& index)
{
    // For now, just get a new destination
    // TODO: Implement proper reservation system
    return GetNewDestination(type);
}

void QuantumScriptPubKeyMan::KeepDestination(int64_t index, const OutputType& type)
{
    // TODO: Implement destination keeping
}

void QuantumScriptPubKeyMan::ReturnDestination(int64_t index, bool internal, const CTxDestination& addr)
{
    // TODO: Implement destination return
}

} // namespace wallet