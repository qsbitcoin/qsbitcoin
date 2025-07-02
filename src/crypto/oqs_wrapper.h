// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CRYPTO_OQS_WRAPPER_H
#define BITCOIN_CRYPTO_OQS_WRAPPER_H

#include <memory>
#include <string>
#include <vector>
#include <mutex>

#include <support/cleanse.h>

// Forward declare OQS types to avoid exposing liboqs headers
extern "C" {
    typedef struct OQS_SIG OQS_SIG;
}

namespace quantum {

/**
 * RAII wrapper for OQS_SIG structure
 * Ensures proper cleanup and provides thread-safe access
 * 
 * Note: This wrapper automatically configures liboqs to use Bitcoin Core's
 * cryptographically secure RNG (GetStrongRandBytes) instead of the default
 * system RNG. This ensures quantum keys benefit from the same entropy
 * sources as ECDSA keys.
 */
class OQSContext {
public:
    /**
     * Create a new OQS signature context
     * @param[in] algorithm_name The OQS algorithm name (e.g., "ML-DSA-65")
     * @throws std::runtime_error if algorithm is not available
     */
    explicit OQSContext(const std::string& algorithm_name);
    
    /**
     * Destructor - ensures OQS_SIG is properly freed
     */
    ~OQSContext();
    
    // Delete copy operations to prevent double-free
    OQSContext(const OQSContext&) = delete;
    OQSContext& operator=(const OQSContext&) = delete;
    
    // Allow move operations
    OQSContext(OQSContext&& other) noexcept;
    OQSContext& operator=(OQSContext&& other) noexcept;
    
    /**
     * Generate a new keypair
     * @param[out] public_key Buffer for public key (must be GetPublicKeySize() bytes)
     * @param[out] secret_key Buffer for secret key (must be GetSecretKeySize() bytes)
     * @return true on success, false on failure
     */
    bool GenerateKeypair(std::vector<unsigned char>& public_key,
                        std::vector<unsigned char>& secret_key);
    
    // Overload for secure_vector
    template<typename Allocator>
    bool GenerateKeypair(std::vector<unsigned char>& public_key,
                        std::vector<unsigned char, Allocator>& secret_key) {
        std::vector<unsigned char> temp_secret;
        if (GenerateKeypair(public_key, temp_secret)) {
            secret_key.assign(temp_secret.begin(), temp_secret.end());
            memory_cleanse(temp_secret.data(), temp_secret.size());
            return true;
        }
        return false;
    }
    
    /**
     * Sign a message
     * @param[out] signature Buffer for signature (will be resized as needed)
     * @param[out] signature_len Actual signature length
     * @param[in] message The message to sign
     * @param[in] message_len Length of the message
     * @param[in] secret_key The secret key
     * @return true on success, false on failure
     */
    bool Sign(std::vector<unsigned char>& signature,
             size_t& signature_len,
             const unsigned char* message,
             size_t message_len,
             const std::vector<unsigned char>& secret_key);
    
    // Overload for secure_vector
    template<typename Allocator>
    bool Sign(std::vector<unsigned char>& signature,
              size_t& signature_len,
              const unsigned char* message,
              size_t message_len,
              const std::vector<unsigned char, Allocator>& secret_key) {
        return Sign(signature, signature_len, message, message_len,
                   std::vector<unsigned char>(secret_key.begin(), secret_key.end()));
    }
    
    /**
     * Verify a signature
     * @param[in] message The signed message
     * @param[in] message_len Length of the message
     * @param[in] signature The signature to verify
     * @param[in] signature_len Length of the signature
     * @param[in] public_key The public key
     * @return true if signature is valid, false otherwise
     */
    bool Verify(const unsigned char* message,
               size_t message_len,
               const unsigned char* signature,
               size_t signature_len,
               const std::vector<unsigned char>& public_key);
    
    /**
     * Get the public key size for this algorithm
     * @return Public key size in bytes
     */
    size_t GetPublicKeySize() const;
    
    /**
     * Get the secret key size for this algorithm
     * @return Secret key size in bytes
     */
    size_t GetSecretKeySize() const;
    
    /**
     * Get the maximum signature size for this algorithm
     * @return Maximum signature size in bytes
     */
    size_t GetMaxSignatureSize() const;
    
    /**
     * Get the algorithm name
     * @return Algorithm name string
     */
    std::string GetAlgorithmName() const;
    
    /**
     * Check if the algorithm is available
     * @param[in] algorithm_name The algorithm to check
     * @return true if available, false otherwise
     */
    static bool IsAlgorithmAvailable(const std::string& algorithm_name);
    
private:
    OQS_SIG* m_sig;
    mutable std::mutex m_mutex;  // Protect OQS_SIG access
    std::string m_algorithm_name;
};

/**
 * Secure memory management for quantum-safe keys
 */
class SecureQuantumKey {
public:
    /**
     * Create a secure key container
     * @param[in] size Key size in bytes
     */
    explicit SecureQuantumKey(size_t size);
    
    /**
     * Destructor - securely wipes memory
     */
    ~SecureQuantumKey();
    
    // Delete copy operations
    SecureQuantumKey(const SecureQuantumKey&) = delete;
    SecureQuantumKey& operator=(const SecureQuantumKey&) = delete;
    
    /**
     * Get mutable data pointer
     * @return Pointer to key data
     */
    unsigned char* data() { return m_data.data(); }
    
    /**
     * Get const data pointer
     * @return Const pointer to key data
     */
    const unsigned char* data() const { return m_data.data(); }
    
    /**
     * Get key size
     * @return Size in bytes
     */
    size_t size() const { return m_data.size(); }
    
    /**
     * Convert to vector (copies data)
     * @return Vector containing key data
     */
    std::vector<unsigned char> ToVector() const;
    
private:
    std::vector<unsigned char> m_data;
};

} // namespace quantum

#endif // BITCOIN_CRYPTO_OQS_WRAPPER_H