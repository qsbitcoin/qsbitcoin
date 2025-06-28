// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QUANTUM_ADDRESS_H
#define BITCOIN_QUANTUM_ADDRESS_H

#include <crypto/quantum_key.h>
#include <script/script.h>
#include <uint256.h>
#include <vector>

namespace quantum {

/**
 * Quantum-safe Bitcoin address format
 * 
 * This module provides quantum-safe functionality for Bitcoin.
 * 
 * Current implementation uses P2WSH (bech32 addresses) exclusively for
 * quantum signatures to handle large signature sizes without script limits.
 * 
 * Signature sizes:
 * - ML-DSA-65: ~3.3KB signatures
 * - SLH-DSA-192f: ~35KB signatures
 */

/**
 * Quantum-safe hash function for addresses
 * 
 * Uses SHA3-256 which is quantum-resistant
 * 
 * @param data Data to hash
 * @return SHA3-256 hash
 */
uint256 QuantumHash256(const std::vector<unsigned char>& data);

} // namespace quantum

#endif // BITCOIN_QUANTUM_ADDRESS_H