// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/quantum_keystore.h>
#include <key_io.h>

namespace wallet {

// Global quantum key store instance
std::unique_ptr<QuantumKeyStore> g_quantum_keystore = std::make_unique<QuantumKeyStore>();

bool QuantumKeyStore::AddQuantumKey(const CKeyID& keyid, std::unique_ptr<CQuantumKey> key)
{
    LOCK(cs_quantum_keys);
    
    if (!key || !key->IsValid()) return false;
    
    // Store the public key
    CQuantumPubKey pubkey = key->GetPubKey();
    m_quantum_pubkeys[keyid] = pubkey;
    
    // Store the private key
    m_quantum_keys[keyid] = std::move(key);
    
    return true;
}

bool QuantumKeyStore::GetQuantumKey(const CKeyID& keyid, const CQuantumKey** key) const
{
    LOCK(cs_quantum_keys);
    
    auto it = m_quantum_keys.find(keyid);
    if (it != m_quantum_keys.end() && it->second) {
        *key = it->second.get();
        return true;
    }
    return false;
}

bool QuantumKeyStore::GetQuantumPubKey(const CKeyID& keyid, CQuantumPubKey& pubkey) const
{
    LOCK(cs_quantum_keys);
    
    auto it = m_quantum_pubkeys.find(keyid);
    if (it != m_quantum_pubkeys.end()) {
        pubkey = it->second;
        return true;
    }
    return false;
}

bool QuantumKeyStore::HaveQuantumKey(const CKeyID& keyid) const
{
    LOCK(cs_quantum_keys);
    return m_quantum_keys.count(keyid) > 0;
}

int QuantumKeyStore::GetQuantumTypeForAddress(const CTxDestination& dest) const
{
    LOCK(cs_quantum_keys);
    
    // Check if it's a PKHash address
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
    
    return 0; // Not a quantum address
}

size_t QuantumKeyStore::GetKeyCount() const
{
    LOCK(cs_quantum_keys);
    return m_quantum_keys.size();
}

} // namespace wallet