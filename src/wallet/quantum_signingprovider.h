// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_QUANTUM_SIGNINGPROVIDER_H
#define BITCOIN_WALLET_QUANTUM_SIGNINGPROVIDER_H

#include <script/signingprovider.h>
#include <crypto/quantum_key.h>
#include <wallet/quantum_keystore.h>

namespace wallet {

/**
 * SigningProvider that can handle quantum keys
 * This extends FlatSigningProvider to support quantum signature schemes
 */
class QuantumSigningProvider : public FlatSigningProvider
{
private:
    //! Map from CKeyID to quantum keys (non-owning pointers)
    std::map<CKeyID, const quantum::CQuantumKey*> quantum_keys;
    
    //! Map from CKeyID to quantum public keys
    std::map<CKeyID, quantum::CQuantumPubKey> quantum_pubkeys;

public:
    //! Add a quantum key to this provider
    void AddQuantumKey(const CKeyID& keyid, const quantum::CQuantumKey* key);
    
    //! Add a quantum public key to this provider
    void AddQuantumPubKey(const CKeyID& keyid, const quantum::CQuantumPubKey& pubkey);
    
    //! Get quantum key if available
    bool GetQuantumKey(const CKeyID& keyid, const quantum::CQuantumKey** key) const;
    
    //! Get quantum public key if available
    bool GetQuantumPubKey(const CKeyID& keyid, quantum::CQuantumPubKey& pubkey) const;
    
    //! Check if we have the quantum key
    bool HaveQuantumKey(const CKeyID& keyid) const;
    
    //! Merge quantum keys from another provider
    void MergeQuantum(const QuantumSigningProvider& other);
};

/**
 * Create a quantum signing provider from the global keystore for a specific script
 */
std::unique_ptr<QuantumSigningProvider> GetQuantumSigningProvider(const CScript& script, bool include_private = false);

} // namespace wallet

#endif // BITCOIN_WALLET_QUANTUM_SIGNINGPROVIDER_H