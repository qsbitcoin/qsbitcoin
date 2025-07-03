// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_QUANTUM_SIGNATURE_H
#define BITCOIN_SCRIPT_QUANTUM_SIGNATURE_H

#include <serialize.h>
#include <span.h>
#include <uint256.h>

#include <cstdint>
#include <vector>

namespace quantum {

/** Signature scheme identifiers for extensibility */
enum SignatureSchemeID : uint8_t {
    SCHEME_ECDSA = 0x01,         // Legacy ECDSA signatures
    SCHEME_ML_DSA_65 = 0x02,     // ML-DSA-65 (Dilithium3)
    SCHEME_SLH_DSA_192F = 0x03,  // SLH-DSA-192f (SPHINCS+)
    // Reserved for future algorithms
    SCHEME_ML_DSA_87 = 0x04,     // Future: ML-DSA-87
    SCHEME_FALCON_512 = 0x05,    // Future: Falcon-512
    SCHEME_FALCON_1024 = 0x06,   // Future: Falcon-1024
    // ... extensible up to 0xFF
};

/** Exact sizes for ML-DSA-65 (NIST standard - verified from liboqs) */
static constexpr size_t ML_DSA_65_PRIVKEY_SIZE = 4032;     // ML-DSA-65 private key size
static constexpr size_t ML_DSA_65_PUBKEY_SIZE = 1952;      // ML-DSA-65 public key size
static constexpr size_t ML_DSA_65_SIG_SIZE = 3309;         // ML-DSA-65 signature size

/** Exact sizes for SLH-DSA-192f (NIST standard - verified from liboqs) */
static constexpr size_t SLH_DSA_192F_PRIVKEY_SIZE = 96;    // SLH-DSA-192f private key size
static constexpr size_t SLH_DSA_192F_PUBKEY_SIZE = 48;     // SLH-DSA-192f public key size
static constexpr size_t SLH_DSA_192F_SIG_SIZE = 35664;     // SLH-DSA-192f signature size

/** Maximum signature sizes for validation */
static constexpr size_t MAX_ECDSA_SIG_SIZE = 73;           // DER encoded ECDSA
static constexpr size_t MAX_ML_DSA_65_SIG_SIZE = 3366;     // ML-DSA-65 signature (with margin)
static constexpr size_t MAX_SLH_DSA_192F_SIG_SIZE = SLH_DSA_192F_SIG_SIZE; // SLH-DSA-192f signature
static constexpr size_t MAX_QUANTUM_SIG_SIZE = 65535;      // Maximum for any quantum signature (varint limit)

/** Maximum public key sizes */
static constexpr size_t MAX_ECDSA_PUBKEY_SIZE = 65;        // Uncompressed ECDSA public key
static constexpr size_t MAX_ML_DSA_65_PUBKEY_SIZE = ML_DSA_65_PUBKEY_SIZE;  // ML-DSA-65 public key
static constexpr size_t MAX_SLH_DSA_192F_PUBKEY_SIZE = SLH_DSA_192F_PUBKEY_SIZE; // SLH-DSA-192f public key
static constexpr size_t MAX_QUANTUM_PUBKEY_SIZE_DYNAMIC = 65535;   // Maximum for any quantum pubkey (varint limit)

/** Minimum size threshold to detect quantum signatures */
static constexpr size_t MIN_QUANTUM_SIG_SIZE_THRESHOLD = 100;  // Quantum signatures are always larger than this

/**
 * Dynamic signature format:
 * [scheme_id:1][sig_len:varint][signature][pubkey_len:varint][pubkey]
 * 
 * This format allows for:
 * - Future algorithm additions without consensus changes
 * - Efficient encoding of variable-length signatures
 * - Clear separation between signature and public key data
 */
struct QuantumSignature {
    SignatureSchemeID scheme_id;
    std::vector<unsigned char> signature;
    std::vector<unsigned char> pubkey;

    QuantumSignature() : scheme_id(SCHEME_ECDSA) {}
    
    QuantumSignature(SignatureSchemeID id, 
                     const std::vector<unsigned char>& sig,
                     const std::vector<unsigned char>& pk)
        : scheme_id(id), signature(sig), pubkey(pk) {}

    /** Serialize to script format */
    void Serialize(std::vector<unsigned char>& vch) const;
    
    /** Deserialize from script format */
    bool Deserialize(const std::vector<unsigned char>& vch);
    
    /** Get maximum expected signature size for a scheme */
    static size_t GetMaxSignatureSize(SignatureSchemeID scheme_id);
    
    /** Get maximum expected public key size for a scheme */
    static size_t GetMaxPubKeySize(SignatureSchemeID scheme_id);
    
    /** Validate signature and pubkey sizes for the scheme */
    bool IsValidSize() const;
    
    /** Calculate serialized size */
    size_t GetSerializedSize() const;
};

/**
 * Parse a quantum signature from script data
 * Returns true if parsing successful, false otherwise
 */
bool ParseQuantumSignature(const std::vector<unsigned char>& data, QuantumSignature& sig_out);

/**
 * Encode a quantum signature for inclusion in script
 */
std::vector<unsigned char> EncodeQuantumSignature(const QuantumSignature& sig);

/**
 * Calculate transaction weight adjustment for quantum signatures
 * Quantum signatures are much larger than ECDSA, so we need special weight calculations
 */
int64_t GetQuantumSignatureWeight(const QuantumSignature& sig);

} // namespace quantum

#endif // BITCOIN_SCRIPT_QUANTUM_SIGNATURE_H