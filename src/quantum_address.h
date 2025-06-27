// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QUANTUM_ADDRESS_H
#define BITCOIN_QUANTUM_ADDRESS_H

#include <crypto/quantum_key.h>
#include <script/script.h>
#include <uint256.h>
#include <string>
#include <vector>

namespace quantum {

/**
 * Quantum-safe Bitcoin address format
 * 
 * This module defines address formats for quantum-safe signatures.
 * Due to the large size of quantum-safe public keys, we use hash-based
 * addresses similar to P2PKH/P2SH but with quantum-safe algorithms.
 * 
 * Address format:
 * - Version byte(s) to identify quantum address type
 * - Hash of public key (using SHA3-256 for quantum resistance)
 * - Checksum (4 bytes)
 * 
 * Example address types:
 * - Q1... : Pay to Quantum Public Key Hash (P2QPKH) using ML-DSA-65
 * - Q2... : Pay to Quantum Public Key Hash (P2QPKH) using SLH-DSA-192F
 * - Q3... : Pay to Quantum Script Hash (P2QSH) for multisig
 */

/**
 * Quantum address types
 */
enum class QuantumAddressType : uint8_t {
    P2QPKH_ML_DSA = 0x51,      // 'Q' + 1 for ML-DSA-65
    P2QPKH_SLH_DSA = 0x52,     // 'Q' + 2 for SLH-DSA-192F  
    P2QSH = 0x53,              // 'Q' + 3 for quantum script hash
    P2QWPKH_ML_DSA = 0x71,     // 'q' + 1 for witness ML-DSA
    P2QWPKH_SLH_DSA = 0x72,    // 'q' + 2 for witness SLH-DSA
    P2QWSH = 0x73              // 'q' + 3 for witness quantum script
};

/**
 * Generate a quantum-safe address from a public key
 * 
 * Creates a P2QPKH (Pay to Quantum Public Key Hash) address
 * 
 * @param pubkey Quantum public key
 * @param addrType Address type (defaults based on key type)
 * @return Base58Check encoded address
 */
std::string EncodeQuantumAddress(const CQuantumPubKey& pubkey,
                                QuantumAddressType addrType = QuantumAddressType::P2QPKH_ML_DSA);

/**
 * Decode a quantum-safe address
 * 
 * @param address Base58Check encoded address
 * @param addrType Output address type
 * @param hash Output hash (public key hash or script hash)
 * @return true if decoding succeeded
 */
bool DecodeQuantumAddress(const std::string& address,
                         QuantumAddressType& addrType,
                         uint256& hash);

/**
 * Create a P2QPKH (Pay to Quantum Public Key Hash) script
 * 
 * Script format:
 * OP_DUP OP_HASH256 <pubkeyhash> OP_EQUALVERIFY OP_CHECKSIGQ
 * 
 * Where OP_CHECKSIGQ is a new opcode for quantum signature verification
 * 
 * @param pubkeyHash SHA3-256 hash of the public key
 * @param keyType Type of quantum key
 * @return Script for P2QPKH
 */
CScript CreateP2QPKHScript(const uint256& pubkeyHash, KeyType keyType);

/**
 * Create a P2QSH (Pay to Quantum Script Hash) script
 * 
 * Similar to P2SH but for quantum-safe scripts
 * 
 * @param scriptHash SHA3-256 hash of the redeem script
 * @return Script for P2QSH
 */
CScript CreateP2QSHScript(const uint256& scriptHash);

/**
 * Extract quantum address type from a script
 * 
 * @param script Script to analyze
 * @param addrType Output address type
 * @param hash Output hash (pubkey hash or script hash)
 * @return true if script is a quantum address type
 */
bool ExtractQuantumAddress(const CScript& script,
                          QuantumAddressType& addrType,
                          uint256& hash);

/**
 * Quantum-safe hash function for addresses
 * 
 * Uses SHA3-256 which is quantum-resistant
 * 
 * @param data Data to hash
 * @return SHA3-256 hash
 */
uint256 QuantumHash256(const std::vector<unsigned char>& data);

/**
 * Convert quantum address type to human-readable string
 */
std::string GetQuantumAddressTypeString(QuantumAddressType type);

/**
 * Get the expected public key type for an address type
 */
KeyType GetKeyTypeForAddress(QuantumAddressType addrType);

/**
 * Validate a quantum address string
 * 
 * @param address Address to validate
 * @return true if address is valid
 */
bool IsValidQuantumAddress(const std::string& address);

/**
 * Quantum witness program for SegWit-style quantum addresses
 * 
 * @param witversion Witness version (0 for current)
 * @param keyType Type of quantum key
 * @param pubkeyHash Hash of public key
 * @return Witness program script
 */
CScript CreateQuantumWitnessProgram(unsigned char witversion,
                                   KeyType keyType,
                                   const uint256& pubkeyHash);

} // namespace quantum

#endif // BITCOIN_QUANTUM_ADDRESS_H