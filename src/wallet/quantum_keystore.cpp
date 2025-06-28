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


size_t QuantumKeyStore::GetKeyCount() const
{
    LOCK(cs_quantum_keys);
    return m_quantum_keys.size();
}

std::vector<CKeyID> QuantumKeyStore::GetAllKeyIDs() const
{
    LOCK(cs_quantum_keys);
    std::vector<CKeyID> keyids;
    keyids.reserve(m_quantum_keys.size());
    
    for (const auto& [keyid, key] : m_quantum_keys) {
        keyids.push_back(keyid);
    }
    
    return keyids;
}

bool QuantumKeyStore::AddWitnessScript(const CScriptID& scriptID, const CScript& witnessScript)
{
    LOCK(cs_quantum_keys);
    m_witness_scripts[scriptID] = witnessScript;
    return true;
}

bool QuantumKeyStore::GetWitnessScript(const CScriptID& scriptID, CScript& witnessScript) const
{
    LOCK(cs_quantum_keys);
    
    auto it = m_witness_scripts.find(scriptID);
    if (it != m_witness_scripts.end()) {
        witnessScript = it->second;
        return true;
    }
    return false;
}

} // namespace wallet