// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CRYPTO_SIGNATURE_SCHEME_H
#define BITCOIN_CRYPTO_SIGNATURE_SCHEME_H

#include <uint256.h>
#include <pubkey.h>
#include <key.h>
#include <vector>
#include <memory>

namespace quantum {

/**
 * Signature scheme identifiers
 * These match the scheme_flags in the address format
 */
enum class SignatureSchemeId : uint8_t {
    ECDSA   = 0x01,  // Legacy ECDSA
    ML_DSA  = 0x02,  // ML-DSA-65 (Dilithium)
    SLH_DSA = 0x03,  // SLH-DSA-192f (SPHINCS+)
};

/**
 * Abstract interface for signature schemes
 * This allows Bitcoin Core to support multiple signature algorithms
 * including both classical (ECDSA) and quantum-safe algorithms
 */
class ISignatureScheme {
public:
    virtual ~ISignatureScheme() = default;
    
    /**
     * Sign a message hash
     * @param[in] hash The 256-bit message hash to sign
     * @param[in] key The private key
     * @param[out] sig The resulting signature
     * @return true if signing succeeded, false otherwise
     */
    virtual bool Sign(const uint256& hash, const CKey& key, 
                     std::vector<unsigned char>& sig) const = 0;
    
    /**
     * Verify a signature
     * @param[in] hash The 256-bit message hash
     * @param[in] pubkey The public key
     * @param[in] sig The signature to verify
     * @return true if the signature is valid, false otherwise
     */
    virtual bool Verify(const uint256& hash, const CPubKey& pubkey,
                       const std::vector<unsigned char>& sig) const = 0;
    
    /**
     * Get the maximum signature size for this scheme
     * @return Maximum signature size in bytes
     */
    virtual size_t GetMaxSignatureSize() const = 0;
    
    /**
     * Get the public key size for this scheme
     * @return Public key size in bytes
     */
    virtual size_t GetPublicKeySize() const = 0;
    
    /**
     * Get the private key size for this scheme
     * @return Private key size in bytes
     */
    virtual size_t GetPrivateKeySize() const = 0;
    
    /**
     * Get the scheme identifier
     * @return The SignatureSchemeId for this scheme
     */
    virtual SignatureSchemeId GetSchemeId() const = 0;
    
    /**
     * Get a human-readable name for this scheme
     * @return Scheme name (e.g., "ECDSA", "ML-DSA-65")
     */
    virtual std::string GetName() const = 0;
    
    /**
     * Check if this scheme is quantum-safe
     * @return true if quantum-safe, false otherwise
     */
    virtual bool IsQuantumSafe() const = 0;
};

/**
 * Registry for signature schemes
 * Manages the available signature schemes and provides factory methods
 */
class SignatureSchemeRegistry {
private:
    std::map<SignatureSchemeId, std::unique_ptr<ISignatureScheme>> m_schemes;
    static SignatureSchemeRegistry* s_instance;
    
    SignatureSchemeRegistry();
    
public:
    /**
     * Get the singleton instance
     */
    static SignatureSchemeRegistry& GetInstance();
    
    /**
     * Register a signature scheme
     * @param[in] scheme The scheme to register
     */
    void RegisterScheme(std::unique_ptr<ISignatureScheme> scheme);
    
    /**
     * Get a signature scheme by ID
     * @param[in] id The scheme identifier
     * @return Pointer to the scheme, or nullptr if not found
     */
    const ISignatureScheme* GetScheme(SignatureSchemeId id) const;
    
    /**
     * Get all registered schemes
     * @return Vector of scheme IDs
     */
    std::vector<SignatureSchemeId> GetRegisteredSchemes() const;
    
    /**
     * Check if a scheme is registered
     * @param[in] id The scheme identifier
     * @return true if registered, false otherwise
     */
    bool IsSchemeRegistered(SignatureSchemeId id) const;
};

} // namespace quantum

#endif // BITCOIN_CRYPTO_SIGNATURE_SCHEME_H