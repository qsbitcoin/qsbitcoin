// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/quantum_descriptor_util.h>
#include <wallet/quantum_keystore.h>
#include <script/solver.h>
#include <quantum_address.h>

namespace wallet {

void PopulateQuantumSigningProvider(const CScript& script, FlatSigningProvider& provider, bool include_private)
{
    // Check if this is a quantum script (OP_CHECKSIG_ML_DSA or OP_CHECKSIG_SLH_DSA + 20-byte hash)
    if (script.size() == 22 && 
        (script[0] == OP_CHECKSIG_ML_DSA || script[0] == OP_CHECKSIG_SLH_DSA) &&
        script[1] == 20) {
        
        // Extract the key hash from the quantum script
        std::vector<unsigned char> hash_bytes(script.begin() + 2, script.begin() + 22);
        uint160 hash(hash_bytes);
        CKeyID keyid(hash);
        
        // Check if this is a quantum address in the global keystore
        if (g_quantum_keystore) {
            quantum::CQuantumPubKey pubkey;
            if (g_quantum_keystore->GetQuantumPubKey(keyid, pubkey)) {
                // Add quantum public key to the provider
                provider.quantum_pubkeys[keyid] = pubkey;
                
                if (include_private) {
                    const quantum::CQuantumKey* key = nullptr;
                    if (g_quantum_keystore->GetQuantumKey(keyid, &key) && key) {
                        // Store the pointer to the quantum key
                        // This is safe as long as g_quantum_keystore outlives the provider
                        provider.quantum_keys[keyid] = key;
                    }
                }
            }
        }
        return;
    }
    
    // Also handle standard P2PKH scripts in case they are quantum addresses
    std::vector<std::vector<unsigned char>> solutions;
    TxoutType which_type = Solver(script, solutions);
    if (which_type == TxoutType::PUBKEYHASH && solutions.size() == 1 && solutions[0].size() == 20) {
        uint160 hash(solutions[0]);
        CKeyID keyid(hash);
        
        // Check if this is a quantum address in the global keystore
        if (g_quantum_keystore) {
            quantum::CQuantumPubKey pubkey;
            if (g_quantum_keystore->GetQuantumPubKey(keyid, pubkey)) {
                // Add quantum public key to the provider
                provider.quantum_pubkeys[keyid] = pubkey;
                
                if (include_private) {
                    const quantum::CQuantumKey* key = nullptr;
                    if (g_quantum_keystore->GetQuantumKey(keyid, &key) && key) {
                        // Store the pointer to the quantum key
                        // This is safe as long as g_quantum_keystore outlives the provider
                        provider.quantum_keys[keyid] = key;
                    }
                }
            }
        }
    }
}

} // namespace wallet