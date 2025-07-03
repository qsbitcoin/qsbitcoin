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
    LogPrintf("[QUANTUM] PopulateQuantumSigningProvider called, script size=%d, include_private=%d, desc_spkm=%p\n", script.size(), include_private, desc_spkm);
    
    // Check if this is a P2WSH script (OP_0 + 32-byte hash)
    LogPrintf("[QUANTUM] Checking script: size=%d, first_byte=%d, second_byte=%d\n", script.size(), script.size() > 0 ? script[0] : -1, script.size() > 1 ? script[1] : -1);
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
        
        LogPrintf("[QUANTUM] Looking for witness script with hash %s\n", target_hash.ToString());
        LogPrintf("[QUANTUM] Provider has %d scripts\n", provider.scripts.size());
        
        // Check all scripts in the provider
        for (const auto& [id, scr] : provider.scripts) {
            // Calculate the SHA256 of this script
            uint256 script_hash;
            CSHA256().Write(scr.data(), scr.size()).Finalize(script_hash.begin());
            
            LogPrintf("[QUANTUM] Checking script with CScriptID %s, hash=%s\n", id.ToString(), script_hash.ToString());
            
            if (script_hash == target_hash) {
                witness_script = scr;
                found = true;
                LogPrintf("[QUANTUM] Found matching witness script!\n");
                break;
            }
        }
        
        // Witness scripts should be in the provider from the descriptor
        
        if (found) {
            LogPrintf("[QUANTUM] Found witness script in provider\n");
            
            // Check if the witness script is a quantum script
            LogPrintf("[QUANTUM] Parsing witness script, size=%d\n", witness_script.size());
            if (witness_script.size() >= 5) {
                // The witness script format is: <OP_PUSHDATA2><pubkey><OP_CHECKSIG_EX>
                // Algorithm ID is inferred from pubkey size
                
                // Check if script ends with OP_CHECKSIG_EX
                if (witness_script[witness_script.size() - 1] == OP_CHECKSIG_EX) {
                    LogPrintf("[QUANTUM] Script ends with OP_CHECKSIG_EX\n");
                    
                    // Parse the script to extract pubkey
                    CScript::const_iterator pc = witness_script.begin();
                    std::vector<unsigned char> vchPubKey;
                    opcodetype opcode;
                    
                    // Get the pubkey (should be OP_PUSHDATA2)
                    bool got_pubkey = witness_script.GetOp(pc, opcode, vchPubKey);
                    LogPrintf("[QUANTUM] Got pubkey: %d, size=%d, opcode=%d\n", got_pubkey, vchPubKey.size(), opcode);
                    
                    if (got_pubkey && !vchPubKey.empty()) {
                        // Check for OP_CHECKSIG_EX at the end
                        bool got_opcode = witness_script.GetOp(pc, opcode);
                        bool at_end = (pc == witness_script.end());
                        LogPrintf("[QUANTUM] Got final opcode: %d, opcode=%d, at_end=%d\n", got_opcode, opcode, at_end);
                        
                        if (got_opcode && (opcode == OP_CHECKSIG_EX) && at_end) {
                            // Determine algorithm from pubkey size
                            quantum::KeyType keyType;
                            
                            if (vchPubKey.size() == 1952) {
                                keyType = quantum::KeyType::ML_DSA_65;
                                LogPrintf("[QUANTUM] Detected ML-DSA from pubkey size 1952\n");
                            } else if (vchPubKey.size() > 30000) {
                                keyType = quantum::KeyType::SLH_DSA_192F;
                                LogPrintf("[QUANTUM] Detected SLH-DSA from large pubkey size\n");
                            } else {
                                LogPrintf("[QUANTUM] Unknown pubkey size: %d\n", vchPubKey.size());
                                return;
                            }
                            
                            quantum::CQuantumPubKey pubkey(keyType, vchPubKey);
                        
                            if (pubkey.IsValid()) {
                                CKeyID keyid = pubkey.GetID();
                                LogPrintf("[QUANTUM] Found quantum pubkey in witness script, keyid=%s, pubkey_size=%d\n", keyid.ToString(), vchPubKey.size());
                                
                                // Add pubkey to provider
                                provider.quantum_pubkeys[keyid] = pubkey;
                                LogPrintf("[QUANTUM] Added quantum pubkey to provider\n");
                            
                                // Try to get private key
                                if (include_private) {
                                    const quantum::CQuantumKey* qkey = nullptr;
                                    bool have_key = false;
                                    
                                    LogPrintf("[QUANTUM] Looking for private key for keyid=%s\n", keyid.ToString());
                                    
                                    // Try descriptor SPKM first
                                    if (desc_spkm) {
                                        LogPrintf("[QUANTUM] desc_spkm is available, calling GetQuantumKey\n");
                                        have_key = desc_spkm->GetQuantumKey(keyid, &qkey);
                                        LogPrintf("[QUANTUM] GetQuantumKey result: %d, qkey=%p\n", have_key, qkey);
                                    } else {
                                        LogPrintf("[QUANTUM] desc_spkm is NULL!\n");
                                    }
                                    
                                    // Only use descriptor SPKM keys
                                    
                                    if (have_key && qkey) {
                                        // Store pointer to the quantum key in provider
                                        provider.quantum_keys[keyid] = qkey;
                                        LogPrintf("[QUANTUM] Added quantum private key to provider for keyid=%s\n", keyid.ToString());
                                    } else {
                                        LogPrintf("[QUANTUM] No quantum private key found for keyid=%s (have_key=%d, qkey=%p)\n", keyid.ToString(), have_key, qkey);
                                    }
                                }
                            }
                            return;
                        }
                    }
                }
            }
        }
    }
    
    // Legacy support removed - all quantum scripts should use the new unified format
    
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