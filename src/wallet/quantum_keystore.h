// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_QUANTUM_KEYSTORE_H
#define BITCOIN_WALLET_QUANTUM_KEYSTORE_H

#include <crypto/quantum_key.h>
#include <sync.h>
#include <addresstype.h>
#include <map>
#include <memory>

namespace wallet {

using quantum::CQuantumKey;
using quantum::CQuantumPubKey;

/**
 * Simple quantum key store for wallet
 * This is a temporary solution until full descriptor support is implemented
 */
class QuantumKeyStore
{
private:
    mutable RecursiveMutex cs_quantum_keys;
    
    //! Map from CKeyID to quantum keys
    std::map<CKeyID, std::unique_ptr<CQuantumKey>> m_quantum_keys GUARDED_BY(cs_quantum_keys);
    
    //! Map from CKeyID to quantum public keys  
    std::map<CKeyID, CQuantumPubKey> m_quantum_pubkeys GUARDED_BY(cs_quantum_keys);
    
    //! Map from CScriptID to witness scripts for quantum P2WSH addresses
    std::map<CScriptID, CScript> m_witness_scripts GUARDED_BY(cs_quantum_keys);

public:
    //! Add a quantum key to the store
    bool AddQuantumKey(const CKeyID& keyid, std::unique_ptr<CQuantumKey> key);
    
    //! Get a quantum key from the store
    bool GetQuantumKey(const CKeyID& keyid, const CQuantumKey** key) const;
    
    //! Get a quantum public key from the store
    bool GetQuantumPubKey(const CKeyID& keyid, CQuantumPubKey& pubkey) const;
    
    //! Check if we have a quantum key
    bool HaveQuantumKey(const CKeyID& keyid) const;
    
    //! Get the number of quantum keys in the store
    size_t GetKeyCount() const;
    
    //! Get all key IDs in the store
    std::vector<CKeyID> GetAllKeyIDs() const;
    
    //! Add a witness script for a quantum P2WSH address
    bool AddWitnessScript(const CScriptID& scriptID, const CScript& witnessScript);
    
    //! Get a witness script by its ID
    bool GetWitnessScript(const CScriptID& scriptID, CScript& witnessScript) const;
};

//! Global quantum key store (temporary solution)
extern std::unique_ptr<QuantumKeyStore> g_quantum_keystore;

} // namespace wallet

#endif // BITCOIN_WALLET_QUANTUM_KEYSTORE_H