// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/oqs_wrapper.h>
#include <support/cleanse.h>
#include <crypto/common.h>
#include <random.h>
#include <oqs/oqs.h>
#include <oqs/rand.h>

#include <cstring>
#include <stdexcept>

namespace quantum {

// Forward declaration of Bitcoin RNG callback
static void BitcoinRandBytes(uint8_t* buffer, size_t size);

// Static initialization helper
static struct OQSRNGInitializer {
    OQSRNGInitializer() {
        // Set Bitcoin's cryptographically secure RNG as the source for liboqs
        OQS_randombytes_custom_algorithm(&BitcoinRandBytes);
    }
} g_oqs_rng_initializer;

// Bitcoin RNG callback for liboqs
static void BitcoinRandBytes(uint8_t* buffer, size_t size)
{
    // Use Bitcoin Core's cryptographically secure random number generator
    // This provides the same entropy sources as used for ECDSA keys
    
    // GetStrongRandBytes has a limit of 32 bytes per call, so we need to
    // call it multiple times for larger requests (e.g., ML-DSA-65 needs 4032 bytes)
    constexpr size_t MAX_BYTES_PER_CALL = 32;
    size_t offset = 0;
    
    while (offset < size) {
        size_t bytes_to_get = std::min(size - offset, MAX_BYTES_PER_CALL);
        GetStrongRandBytes(std::span<unsigned char>(buffer + offset, bytes_to_get));
        offset += bytes_to_get;
    }
}

OQSContext::OQSContext(const std::string& algorithm_name)
    : m_sig(nullptr), m_algorithm_name(algorithm_name)
{
    m_sig = OQS_SIG_new(algorithm_name.c_str());
    if (!m_sig) {
        throw std::runtime_error("Failed to create OQS_SIG for algorithm: " + algorithm_name);
    }
}

OQSContext::~OQSContext()
{
    if (m_sig) {
        OQS_SIG_free(m_sig);
        m_sig = nullptr;
    }
}

OQSContext::OQSContext(OQSContext&& other) noexcept
    : m_sig(other.m_sig), m_algorithm_name(std::move(other.m_algorithm_name))
{
    other.m_sig = nullptr;
}

OQSContext& OQSContext::operator=(OQSContext&& other) noexcept
{
    if (this != &other) {
        if (m_sig) {
            OQS_SIG_free(m_sig);
        }
        m_sig = other.m_sig;
        m_algorithm_name = std::move(other.m_algorithm_name);
        other.m_sig = nullptr;
    }
    return *this;
}

bool OQSContext::GenerateKeypair(std::vector<unsigned char>& public_key,
                                 std::vector<unsigned char>& secret_key)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_sig) {
        return false;
    }
    
    // Resize buffers to correct size
    public_key.resize(m_sig->length_public_key);
    secret_key.resize(m_sig->length_secret_key);
    
    // Generate keypair using Bitcoin Core's random number generator
    OQS_STATUS status = OQS_SIG_keypair(m_sig, public_key.data(), secret_key.data());
    
    return (status == OQS_SUCCESS);
}

bool OQSContext::Sign(std::vector<unsigned char>& signature,
                     size_t& signature_len,
                     const unsigned char* message,
                     size_t message_len,
                     const std::vector<unsigned char>& secret_key)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_sig || !message || secret_key.size() != m_sig->length_secret_key) {
        return false;
    }
    
    // Resize signature buffer to maximum size
    signature.resize(m_sig->length_signature);
    
    OQS_STATUS status = OQS_SIG_sign(m_sig, signature.data(), &signature_len,
                                     message, message_len, secret_key.data());
    
    if (status == OQS_SUCCESS) {
        // Resize to actual signature length
        signature.resize(signature_len);
        return true;
    }
    
    return false;
}

bool OQSContext::Verify(const unsigned char* message,
                       size_t message_len,
                       const unsigned char* signature,
                       size_t signature_len,
                       const std::vector<unsigned char>& public_key)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    if (!m_sig || !message || !signature || 
        public_key.size() != m_sig->length_public_key) {
        return false;
    }
    
    OQS_STATUS status = OQS_SIG_verify(m_sig, message, message_len,
                                       signature, signature_len, public_key.data());
    
    return (status == OQS_SUCCESS);
}

size_t OQSContext::GetPublicKeySize() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sig ? m_sig->length_public_key : 0;
}

size_t OQSContext::GetSecretKeySize() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sig ? m_sig->length_secret_key : 0;
}

size_t OQSContext::GetMaxSignatureSize() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sig ? m_sig->length_signature : 0;
}

std::string OQSContext::GetAlgorithmName() const
{
    return m_algorithm_name;
}

bool OQSContext::IsAlgorithmAvailable(const std::string& algorithm_name)
{
    return OQS_SIG_alg_is_enabled(algorithm_name.c_str()) == 1;
}

// SecureQuantumKey implementation
SecureQuantumKey::SecureQuantumKey(size_t size)
    : m_data(size)
{
    // Initialize with cryptographically secure random data
    GetStrongRandBytes(m_data);
}

SecureQuantumKey::~SecureQuantumKey()
{
    // Securely wipe the key data
    memory_cleanse(m_data.data(), m_data.size());
}

std::vector<unsigned char> SecureQuantumKey::ToVector() const
{
    return m_data;
}

} // namespace quantum