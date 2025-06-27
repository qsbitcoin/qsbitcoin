// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CRYPTO_QUANTUM_HD_H
#define BITCOIN_CRYPTO_QUANTUM_HD_H

#include <crypto/quantum_key.h>
#include <crypto/hmac_sha512.h>
#include <hash.h>
#include <span>
#include <cstddef>

namespace quantum {

/**
 * Quantum-safe HD derivation implementation
 * 
 * This implements hierarchical deterministic key derivation for quantum-safe
 * signatures. Unlike BIP32 which uses elliptic curve point addition, this
 * uses a hash-based approach suitable for quantum-safe algorithms.
 * 
 * Key differences from BIP32:
 * 1. No public key derivation (not possible with hash-based signatures)
 * 2. Uses hash-based key generation instead of EC point addition
 * 3. Supports both ML-DSA and SLH-DSA algorithms
 * 
 * The derivation function still uses HMAC-SHA512 which is quantum-resistant.
 */

/**
 * Derive a quantum-safe child key from parent key
 * 
 * This function implements the core HD derivation logic for quantum keys.
 * It uses HMAC-SHA512 to derive new key material from the parent key.
 * 
 * @param keyChild Output child key
 * @param ccChild Output child chain code
 * @param nChild Child key index (hardened if >= 0x80000000)
 * @param cc Parent chain code
 * @param keyParent Parent private key data
 * @param keyType Type of quantum key to derive
 * @return true if derivation succeeded
 */
bool DeriveQuantumChild(CQuantumKey& keyChild, ChainCode& ccChild, 
                       unsigned int nChild, const ChainCode& cc,
                       const secure_vector& keyParent, KeyType keyType);

/**
 * Generate master quantum key from seed
 * 
 * This generates a master HD key from a seed value, similar to BIP32
 * master key generation but adapted for quantum algorithms.
 * 
 * @param keyMaster Output master key
 * @param ccMaster Output master chain code
 * @param seed Seed data (should be at least 32 bytes)
 * @param keyType Type of quantum key to generate
 * @return true if generation succeeded
 */
bool GenerateQuantumMaster(CQuantumKey& keyMaster, ChainCode& ccMaster,
                          std::span<const std::byte> seed, KeyType keyType);

/**
 * Quantum HD key fingerprint calculation
 * 
 * Calculates a 4-byte fingerprint of a quantum public key for use in
 * extended key serialization.
 * 
 * @param pubkey The public key to fingerprint
 * @param fingerprint Output 4-byte fingerprint
 */
void GetQuantumKeyFingerprint(const CQuantumPubKey& pubkey, 
                             unsigned char fingerprint[4]);

} // namespace quantum

#endif // BITCOIN_CRYPTO_QUANTUM_HD_H