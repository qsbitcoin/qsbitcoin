// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CRYPTO_QUANTUM_KEY_IO_H
#define BITCOIN_CRYPTO_QUANTUM_KEY_IO_H

#include <crypto/quantum_key.h>
#include <string>
#include <vector>

namespace quantum {

/**
 * Quantum key import/export functionality
 * 
 * This module provides functions to import and export quantum keys in various
 * formats. Due to the large size of quantum keys, we use specialized formats
 * rather than the traditional WIF (Wallet Import Format).
 */

/**
 * Export formats for quantum keys
 */
enum class ExportFormat {
    RAW,        // Raw binary format
    BASE64,     // Base64 encoded
    HEX,        // Hexadecimal string
    ARMORED     // ASCII-armored format with checksums
};

/**
 * Export a quantum private key
 * 
 * @param key The key to export
 * @param format Export format
 * @return Exported key data (empty on failure)
 */
std::string ExportQuantumKey(const CQuantumKey& key, ExportFormat format = ExportFormat::ARMORED);

/**
 * Import a quantum private key
 * 
 * @param key Output key object
 * @param data Imported key data
 * @param format Import format (auto-detect if not specified)
 * @return true if import succeeded
 */
bool ImportQuantumKey(CQuantumKey& key, const std::string& data, 
                     ExportFormat format = ExportFormat::ARMORED);

/**
 * Export a quantum public key
 * 
 * @param pubkey The public key to export
 * @param format Export format
 * @return Exported public key data
 */
std::string ExportQuantumPubKey(const CQuantumPubKey& pubkey, 
                               ExportFormat format = ExportFormat::HEX);

/**
 * Import a quantum public key
 * 
 * @param pubkey Output public key object
 * @param data Imported public key data
 * @param format Import format
 * @return true if import succeeded
 */
bool ImportQuantumPubKey(CQuantumPubKey& pubkey, const std::string& data,
                        ExportFormat format = ExportFormat::HEX);

/**
 * Create an ASCII-armored format for quantum keys
 * 
 * This format includes:
 * - Header/footer lines
 * - Key type identification
 * - Base64 encoded key data
 * - CRC32 checksum
 * 
 * @param keyData Raw key data
 * @param keyType Type of quantum key
 * @param isPrivate Whether this is a private key
 * @return ASCII-armored string
 */
std::string CreateArmoredFormat(const std::vector<unsigned char>& keyData,
                               KeyType keyType, bool isPrivate,
                               const std::vector<unsigned char>& pubKeyData = {});

/**
 * Parse ASCII-armored format
 * 
 * @param armored Armored string
 * @param keyData Output key data
 * @param keyType Output key type
 * @param isPrivate Output whether this is a private key
 * @param pubKeyData Output public key data (if present in armored format)
 * @return true if parsing succeeded
 */
bool ParseArmoredFormat(const std::string& armored,
                       std::vector<unsigned char>& keyData,
                       KeyType& keyType,
                       bool& isPrivate,
                       std::vector<unsigned char>& pubKeyData);

/**
 * Generate a key fingerprint for display
 * 
 * Creates a human-readable fingerprint of a quantum key for verification
 * 
 * @param pubkey Public key to fingerprint
 * @return Fingerprint string (e.g., "ML-DSA-65:1A2B:3C4D:5E6F:...")
 */
std::string GetQuantumKeyFingerprint(const CQuantumPubKey& pubkey);

} // namespace quantum

#endif // BITCOIN_CRYPTO_QUANTUM_KEY_IO_H