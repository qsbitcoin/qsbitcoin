// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/quantum_key.h>
#include <crypto/signature_scheme.h>
#include <crypto/oqs_wrapper.h>
#include <hash.h>
#include <random.h>
#include <script/quantum_signature.h>
#include <support/cleanse.h>
#include <util/strencodings.h>

#include <algorithm>

namespace quantum {

//
// CQuantumPubKey implementation
//

CQuantumPubKey::CQuantumPubKey(const CPubKey& pubkey)
    : m_type(KeyType::ECDSA)
{
    m_vchPubKey.assign(pubkey.begin(), pubkey.end());
}

bool CQuantumPubKey::IsValid() const
{
    switch (m_type) {
        case KeyType::ECDSA:
            if (m_vchPubKey.size() == 33 || m_vchPubKey.size() == 65) {
                CPubKey pubkey(m_vchPubKey);
                return pubkey.IsValid();
            }
            return false;
            
        case KeyType::ML_DSA_65:
            return m_vchPubKey.size() == quantum::ML_DSA_65_PUBKEY_SIZE;
            
        case KeyType::SLH_DSA_192F:
            return m_vchPubKey.size() == quantum::SLH_DSA_192F_PUBKEY_SIZE;
            
        default:
            return false;
    }
}

CKeyID CQuantumPubKey::GetID() const
{
    if (!IsValid()) {
        return CKeyID();
    }
    
    // For quantum keys, we include the key type in the hash
    if (m_type != KeyType::ECDSA) {
        std::vector<unsigned char> vch;
        vch.push_back(static_cast<unsigned char>(m_type));
        vch.insert(vch.end(), m_vchPubKey.begin(), m_vchPubKey.end());
        return CKeyID(Hash160(vch));
    }
    
    // For ECDSA, use standard hash
    return CKeyID(Hash160(m_vchPubKey));
}

bool CQuantumPubKey::GetCPubKey(CPubKey& pubkey) const
{
    if (m_type != KeyType::ECDSA) {
        return false;
    }
    
    pubkey = CPubKey(m_vchPubKey);
    return pubkey.IsValid();
}

//
// CQuantumKey implementation
//

CQuantumKey::~CQuantumKey()
{
    Clear();
}

CQuantumKey::CQuantumKey(const CKey& key)
    : m_type(KeyType::ECDSA), m_fValid(key.IsValid())
{
    if (m_fValid) {
        m_ecdsaKey = std::make_unique<CKey>(key);
    }
}

void CQuantumKey::Clear()
{
    if (!m_vchPrivKey.empty()) {
        memory_cleanse(m_vchPrivKey.data(), m_vchPrivKey.size());
        m_vchPrivKey.clear();
    }
    if (!m_vchPubKey.empty()) {
        m_vchPubKey.clear();
    }
    m_ecdsaKey.reset();
    m_fValid = false;
}

void CQuantumKey::MakeNewKey(KeyType type)
{
    Clear();
    m_type = type;
    
    switch (type) {
        case KeyType::ECDSA: {
            m_ecdsaKey = std::make_unique<CKey>();
            m_ecdsaKey->MakeNewKey(true); // Always use compressed keys
            m_fValid = m_ecdsaKey->IsValid();
            break;
        }
        
        case KeyType::ML_DSA_65: {
            OQSContext ctx("ML-DSA-65");
            std::vector<unsigned char> public_key;
            secure_vector secret_key;
            
            if (ctx.GenerateKeypair(public_key, secret_key)) {
                m_vchPrivKey = std::move(secret_key);
                m_vchPubKey = std::move(public_key);
                m_fValid = true;
            }
            break;
        }
        
        case KeyType::SLH_DSA_192F: {
            OQSContext ctx("SPHINCS+-SHA2-192f-simple");
            std::vector<unsigned char> public_key;
            secure_vector secret_key;
            
            if (ctx.GenerateKeypair(public_key, secret_key)) {
                m_vchPrivKey = std::move(secret_key);
                m_vchPubKey = std::move(public_key);
                m_fValid = true;
            }
            break;
        }
        
        default:
            m_fValid = false;
            break;
    }
}

CQuantumPubKey CQuantumKey::GetPubKey() const
{
    if (!IsValid()) {
        return CQuantumPubKey();
    }
    
    switch (m_type) {
        case KeyType::ECDSA: {
            if (m_ecdsaKey) {
                CPubKey pubkey = m_ecdsaKey->GetPubKey();
                return CQuantumPubKey(pubkey);
            }
            break;
        }
        
        case KeyType::ML_DSA_65: {
            // Return the stored public key if available
            if (!m_vchPubKey.empty()) {
                return CQuantumPubKey(KeyType::ML_DSA_65, m_vchPubKey);
            }
            // For ML-DSA-65, we cannot extract the public key from the secret key
            // The public key must be stored separately
            return CQuantumPubKey();
        }
        
        case KeyType::SLH_DSA_192F: {
            // Return the stored public key if available
            if (!m_vchPubKey.empty()) {
                return CQuantumPubKey(KeyType::SLH_DSA_192F, m_vchPubKey);
            }
            // Otherwise extract public key from secret key
            // SPHINCS+: public key is the last 48 bytes of the 96-byte secret key
            if (m_vchPrivKey.size() == quantum::SLH_DSA_192F_PRIVKEY_SIZE) {
                std::vector<unsigned char> pubkey(m_vchPrivKey.end() - quantum::SLH_DSA_192F_PUBKEY_SIZE, m_vchPrivKey.end());
                return CQuantumPubKey(KeyType::SLH_DSA_192F, pubkey);
            }
            break;
        }
    }
    
    return CQuantumPubKey();
}

bool CQuantumKey::Sign(const uint256& hash, std::vector<unsigned char>& vchSig) const
{
    if (!IsValid()) {
        return false;
    }
    
    switch (m_type) {
        case KeyType::ECDSA: {
            if (m_ecdsaKey) {
                return m_ecdsaKey->Sign(hash, vchSig);
            }
            break;
        }
        
        case KeyType::ML_DSA_65: {
            OQSContext ctx("ML-DSA-65");
            size_t sig_len = 0;
            return ctx.Sign(vchSig, sig_len, hash.begin(), 32, m_vchPrivKey);
        }
        break;
        
        case KeyType::SLH_DSA_192F: {
            OQSContext ctx("SPHINCS+-SHA2-192f-simple");
            size_t sig_len = 0;
            return ctx.Sign(vchSig, sig_len, hash.begin(), 32, m_vchPrivKey);
        }
    }
    
    return false;
}

bool CQuantumKey::Verify(const uint256& hash, const std::vector<unsigned char>& vchSig, 
                        const CQuantumPubKey& pubkey)
{
    if (!pubkey.IsValid() || vchSig.empty()) {
        return false;
    }
    
    switch (pubkey.GetType()) {
        case KeyType::ECDSA: {
            CPubKey cpubkey;
            if (pubkey.GetCPubKey(cpubkey)) {
                return cpubkey.Verify(hash, vchSig);
            }
            break;
        }
        
        case KeyType::ML_DSA_65: {
            OQSContext ctx("ML-DSA-65");
            return ctx.Verify(hash.begin(), 32, vchSig.data(), vchSig.size(), 
                            pubkey.GetKeyData());
        }
        break;
        
        case KeyType::SLH_DSA_192F: {
            OQSContext ctx("SPHINCS+-SHA2-192f-simple");
            return ctx.Verify(hash.begin(), 32, vchSig.data(), vchSig.size(), 
                            pubkey.GetKeyData());
        }
        break;
    }
    
    return false;
}

const CKey* CQuantumKey::GetECDSAKey() const
{
    if (m_type == KeyType::ECDSA && m_ecdsaKey) {
        return m_ecdsaKey.get();
    }
    return nullptr;
}

bool CQuantumKey::Derive(CQuantumKey& keyChild, ChainCode& ccChild, unsigned int nChild, 
                        const ChainCode& cc) const
{
    if (!IsValid()) {
        return false;
    }
    
    // For ECDSA keys, use standard BIP32 derivation
    if (m_type == KeyType::ECDSA && m_ecdsaKey) {
        CKey childKey;
        if (m_ecdsaKey->Derive(childKey, ccChild, nChild, cc)) {
            keyChild = CQuantumKey(childKey);
            return true;
        }
    }
    
    // For quantum keys, BIP32 derivation is not supported
    // Quantum cryptographic keys cannot be derived hierarchically like ECDSA keys
    if (m_type == KeyType::ML_DSA_65 || m_type == KeyType::SLH_DSA_192F) {
        return false;
    }
    
    return false;
}

bool CQuantumKey::Load(const secure_vector& privkey, const CQuantumPubKey& pubkey)
{
    Clear();
    
    if (pubkey.GetType() == KeyType::ECDSA) {
        // For ECDSA, use the traditional CKey loading
        CPubKey cpubkey;
        if (pubkey.GetCPubKey(cpubkey)) {
            m_ecdsaKey = std::make_unique<CKey>();
            CPrivKey privKey(privkey.begin(), privkey.end());
            if (m_ecdsaKey->Load(privKey, cpubkey, true)) {
                m_type = KeyType::ECDSA;
                m_fValid = true;
                return true;
            }
        }
    } else {
        // For quantum keys, verify the key matches the expected size
        size_t expected_size = 0;
        switch (pubkey.GetType()) {
            case KeyType::ML_DSA_65:
                expected_size = quantum::ML_DSA_65_PRIVKEY_SIZE;
                break;
            case KeyType::SLH_DSA_192F:
                expected_size = quantum::SLH_DSA_192F_PRIVKEY_SIZE;
                break;
            default:
                return false;
        }
        
        if (privkey.size() == expected_size) {
            m_type = pubkey.GetType();
            m_vchPrivKey = privkey;
            
            // Store the provided public key
            m_vchPubKey = pubkey.GetKeyData();
            
            // For quantum keys, validate the key pair by testing sign/verify
            if (m_type == KeyType::ML_DSA_65 || m_type == KeyType::SLH_DSA_192F) {
                // Test that we can sign and verify with this key pair
                // Use a fixed test hash for validation
                uint256 test_hash = uint256::FromHex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef").value();
                
                std::vector<unsigned char> test_sig;
                m_fValid = true; // Temporarily set valid to allow signing
                
                if (Sign(test_hash, test_sig)) {
                    // Verify the signature matches
                    if (CQuantumKey::Verify(test_hash, test_sig, pubkey)) {
                        return true;
                    }
                }
                
                // Key pair validation failed
                m_fValid = false;
                return false;
            }
            
            m_fValid = true;
            return true;
        }
    }
    
    Clear();
    return false;
}

secure_vector CQuantumKey::GetPrivKeyData() const
{
    if (!IsValid()) {
        return secure_vector();
    }
    
    if (m_type == KeyType::ECDSA && m_ecdsaKey) {
        CPrivKey privkey = m_ecdsaKey->GetPrivKey();
        return secure_vector(privkey.begin(), privkey.end());
    }
    
    return m_vchPrivKey;
}

//
// CExtQuantumKey implementation
//

void CExtQuantumKey::Encode(std::vector<unsigned char>& code) const
{
    code.resize(BIP32_EXTKEY_WITH_VERSION_SIZE + 1); // +1 for key type
    code[0] = 0x04; // Version byte for private key
    code[1] = 0x88;
    code[2] = 0xAD;
    code[3] = 0xE4;
    code[4] = nDepth;
    memcpy(code.data() + 5, vchFingerprint, 4);
    code[9] = (nChild >> 24) & 0xFF;
    code[10] = (nChild >> 16) & 0xFF;
    code[11] = (nChild >> 8) & 0xFF;
    code[12] = (nChild >> 0) & 0xFF;
    memcpy(code.data() + 13, chaincode.begin(), 32);
    code[45] = static_cast<unsigned char>(key.GetType());
    
    // For quantum keys, we need to extend the encoding format
    // This is a simplified version - in production, we'd need a more sophisticated format
    secure_vector keyData = key.GetPrivKeyData();
    code.insert(code.end(), keyData.begin(), keyData.end());
}

void CExtQuantumKey::Decode(const std::vector<unsigned char>& code)
{
    if (code.size() < BIP32_EXTKEY_WITH_VERSION_SIZE + 1) {
        throw std::runtime_error("Invalid extended key size");
    }
    
    nDepth = code[4];
    memcpy(vchFingerprint, code.data() + 5, 4);
    nChild = (code[9] << 24) | (code[10] << 16) | (code[11] << 8) | code[12];
    memcpy(chaincode.begin(), code.data() + 13, 32);
    
    KeyType keyType = static_cast<KeyType>(code[45]);
    
    // Extract the private key data based on type
    secure_vector keyData(code.begin() + 46, code.end());
    
    if (keyType == KeyType::ECDSA) {
        // For ECDSA, reconstruct CKey
        CKey ecdsaKey;
        // This is simplified - in production we'd need proper ECDSA key reconstruction
        key = CQuantumKey(ecdsaKey);
    } else {
        // For quantum keys, load the key data
        CQuantumPubKey dummy_pubkey(keyType, std::vector<unsigned char>());
        key.Load(keyData, dummy_pubkey);
    }
}

bool CExtQuantumKey::Derive(CExtQuantumKey& out, unsigned int _nChild) const
{
    out.nDepth = nDepth + 1;
    CKeyID id = key.GetPubKey().GetID();
    memcpy(out.vchFingerprint, &id, 4);
    out.nChild = _nChild;
    return key.Derive(out.key, out.chaincode, _nChild, chaincode);
}

} // namespace quantum