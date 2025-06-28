// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_QUANTUM_WITNESS_H
#define BITCOIN_SCRIPT_QUANTUM_WITNESS_H

#include <script/script.h>
#include <script/quantum_signature.h>
#include <crypto/quantum_key.h>
#include <uint256.h>

namespace quantum {

/**
 * Create a quantum witness script for P2WSH
 * 
 * The witness script contains the actual quantum signature verification logic.
 * This is the script that gets hashed to create the P2WSH scriptPubKey.
 * 
 * Format: <pubkey> OP_CHECKSIG_[ML_DSA/SLH_DSA]
 * 
 * Note: We use a simple witness script format (just pubkey + checksig) rather than
 * the complex P2PKH-style script. This is more efficient and cleaner for witness.
 * 
 * @param pubkey The quantum public key
 * @return The witness script
 */
CScript CreateQuantumWitnessScript(const CQuantumPubKey& pubkey);

/**
 * Create a P2WSH scriptPubKey for a quantum public key
 * 
 * This creates the actual scriptPubKey that goes in the transaction output.
 * Format: OP_0 <32-byte-hash-of-witness-script>
 * 
 * @param pubkey The quantum public key
 * @return The P2WSH scriptPubKey
 */
CScript CreateQuantumP2WSH(const CQuantumPubKey& pubkey);

/**
 * Extract quantum public key from a witness script
 * 
 * @param witnessScript The witness script to analyze
 * @param pubkey Output quantum public key
 * @return true if this is a quantum witness script
 */
bool ExtractQuantumPubKeyFromWitnessScript(const CScript& witnessScript, 
                                           CQuantumPubKey& pubkey);

/**
 * Check if a script is a quantum P2WSH script
 * 
 * Note: This can only make a best guess since P2WSH scripts all look the same
 * (OP_0 <32-byte-hash>). True verification requires the witness script.
 * 
 * @param script The script to check
 * @return true if this is a P2WSH script
 */
bool IsQuantumP2WSH(const CScript& script);

/**
 * Create witness stack for spending a quantum P2WSH output
 * 
 * The witness stack for P2WSH contains:
 * 1. Empty placeholder for OP_0 (required by OP_CHECKMULTISIG bug)
 * 2. The signature
 * 3. The witness script
 * 
 * For quantum signatures, element 2 is the serialized QuantumSignature
 * 
 * @param qsig The quantum signature
 * @param witnessScript The witness script
 * @return The witness stack
 */
std::vector<std::vector<unsigned char>> CreateQuantumWitnessStack(
    const QuantumSignature& qsig,
    const CScript& witnessScript);

/**
 * Parse quantum signature from witness stack
 * 
 * @param stack The witness stack
 * @param qsig_out Output quantum signature
 * @param witnessScript_out Output witness script
 * @return true if successfully parsed
 */
bool ParseQuantumWitnessStack(const std::vector<std::vector<unsigned char>>& stack, 
                             QuantumSignature& qsig_out,
                             CScript& witnessScript_out);

/**
 * Get the signature scheme from a quantum witness script
 * 
 * @param witnessScript The witness script
 * @return The signature scheme ID, or SCHEME_ECDSA if not quantum
 */
SignatureSchemeID GetQuantumSchemeFromWitnessScript(const CScript& witnessScript);

} // namespace quantum

#endif // BITCOIN_SCRIPT_QUANTUM_WITNESS_H