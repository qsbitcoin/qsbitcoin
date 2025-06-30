// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/quantum_descriptor_util.h>
#include <wallet/scriptpubkeyman.h>
#include <script/solver.h>
#include <script/quantum_witness.h>
#include <quantum_address.h>
#include <logging.h>
#include <hash.h>
#include <crypto/sha256.h>

namespace wallet {

void PopulateQuantumSigningProvider(const CScript& script, FlatSigningProvider& provider, bool include_private, const DescriptorScriptPubKeyMan* desc_spkm)
{
    LogPrintf("[QUANTUM] PopulateQuantumSigningProvider called, script size=%d, include_private=%d\n", script.size(), include_private);
    
    // Check if this is a P2WSH script (OP_0 + 32-byte hash)
    if (script.size() == 34 && script[0] == OP_0 && script[1] == 32) {
        LogPrintf("[QUANTUM] Detected P2WSH script\n");
        // This is a P2WSH script - we need to check if it's for a quantum key
        // by looking up the witness script in the provider
        std::vector<unsigned char> hash_bytes(script.begin() + 2, script.begin() + 34);
        uint256 scripthash(hash_bytes);
        
        // Look through all scripts in the provider to find a witness script that hashes to this value
        uint256 target_hash(hash_bytes);
        CScript witness_script;
        bool found = false;
        
        // Check all scripts in the provider
        for (const auto& [id, scr] : provider.scripts) {
            // Calculate the SHA256 of this script
            uint256 script_hash;
            CSHA256().Write(scr.data(), scr.size()).Finalize(script_hash.begin());
            
            if (script_hash == target_hash) {
                witness_script = scr;
                found = true;
                break;
            }
        }
        
        // Witness scripts should be in the provider from the descriptor
        
        if (found) {
            LogPrintf("[QUANTUM] Found witness script in provider\n");
            
            // Check if the witness script is a quantum script
            if (witness_script.size() >= 2) {
                CScript::const_iterator pc = witness_script.begin();
                opcodetype opcode;
                std::vector<unsigned char> vch;
                
                // Get pubkey
                if (witness_script.GetOp(pc, opcode, vch) && !vch.empty()) {
                    // Get opcode
                    if (witness_script.GetOp(pc, opcode) && 
                        (opcode == OP_CHECKSIG_ML_DSA || opcode == OP_CHECKSIG_SLH_DSA) &&
                        pc == witness_script.end()) {
                        
                        // This is a quantum witness script
                        quantum::KeyType keyType = (opcode == OP_CHECKSIG_ML_DSA) ? 
                            quantum::KeyType::ML_DSA_65 : quantum::KeyType::SLH_DSA_192F;
                        quantum::CQuantumPubKey pubkey(keyType, vch);
                        
                        if (pubkey.IsValid()) {
                            CKeyID keyid = pubkey.GetID();
                            LogPrintf("[QUANTUM] Found quantum pubkey in witness script, keyid=%s\n", keyid.ToString());
                            
                            // Add pubkey to provider
                            provider.quantum_pubkeys[keyid] = pubkey;
                            LogPrintf("[QUANTUM] Added quantum pubkey to provider\n");
                            
                            // Try to get private key
                            if (include_private) {
                                const quantum::CQuantumKey* qkey = nullptr;
                                bool have_key = false;
                                
                                // Try descriptor SPKM first
                                if (desc_spkm) {
                                    have_key = desc_spkm->GetQuantumKey(keyid, &qkey);
                                    LogPrintf("[QUANTUM] GetQuantumKey result: %d, qkey=%p\n", have_key, qkey);
                                }
                                
                                // Only use descriptor SPKM keys
                                
                                if (have_key && qkey) {
                                    // Store pointer to the quantum key in provider
                                    provider.quantum_keys[keyid] = qkey;
                                    LogPrintf("[QUANTUM] Added quantum private key to provider for keyid=%s\n", keyid.ToString());
                                } else {
                                    LogPrintf("[QUANTUM] No quantum private key found for keyid=%s\n", keyid.ToString());
                                }
                            }
                        }
                        return;
                    }
                }
            }
        }
    }
    
    // Legacy support: Check if this is an old-style quantum script (OP_CHECKSIG_ML_DSA or OP_CHECKSIG_SLH_DSA + 20-byte hash)
    if (script.size() == 22 && 
        (script[0] == OP_CHECKSIG_ML_DSA || script[0] == OP_CHECKSIG_SLH_DSA) &&
        script[1] == 20) {
        
        // Extract the key hash from the quantum script
        std::vector<unsigned char> hash_bytes(script.begin() + 2, script.begin() + 22);
        uint160 hash(hash_bytes);
        CKeyID keyid(hash);
        
        // Try descriptor SPKM first, then fall back to global keystore
        quantum::CQuantumPubKey pubkey;
        bool have_pubkey = false;
        
        if (desc_spkm) {
            have_pubkey = desc_spkm->GetQuantumPubKey(keyid, pubkey);
        }
        
        if (have_pubkey) {
            // Add quantum public key to the provider
            provider.quantum_pubkeys[keyid] = pubkey;
            
            if (include_private) {
                const quantum::CQuantumKey* key = nullptr;
                bool have_key = false;
                
                if (desc_spkm) {
                    have_key = desc_spkm->GetQuantumKey(keyid, &key);
                }
                
                if (have_key && key) {
                    // Store the pointer to the quantum key
                    provider.quantum_keys[keyid] = key;
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
        
        // Try descriptor SPKM first, then fall back to global keystore
        quantum::CQuantumPubKey pubkey;
        bool have_pubkey = false;
        
        if (desc_spkm) {
            have_pubkey = desc_spkm->GetQuantumPubKey(keyid, pubkey);
        }
        
        if (have_pubkey) {
            // Add quantum public key to the provider
            provider.quantum_pubkeys[keyid] = pubkey;
            
            if (include_private) {
                const quantum::CQuantumKey* key = nullptr;
                bool have_key = false;
                
                if (desc_spkm) {
                    have_key = desc_spkm->GetQuantumKey(keyid, &key);
                }
                
                if (have_key && key) {
                    // Store the pointer to the quantum key
                    provider.quantum_keys[keyid] = key;
                }
            }
        }
    }
}

} // namespace wallet