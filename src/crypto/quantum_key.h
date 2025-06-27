// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CRYPTO_QUANTUM_KEY_H
#define BITCOIN_CRYPTO_QUANTUM_KEY_H

#include <key.h>
#include <pubkey.h>
#include <crypto/signature_scheme.h>
#include <support/allocators/secure.h>
#include <serialize.h>
#include <uint256.h>

#include <memory>
#include <vector>

namespace quantum {

// Type aliases for secure data storage
using secure_vector = std::vector<unsigned char, secure_allocator<unsigned char>>;

/**
 * Key type identifiers for quantum-safe algorithms
 */
enum class KeyType : uint8_t {
    ECDSA = 0x00,      // Traditional ECDSA (for compatibility)
    ML_DSA_65 = 0x01,  // ML-DSA-65 (Dilithium3)
    SLH_DSA_192F = 0x02 // SLH-DSA-SHA2-192f-simple (SPHINCS+)
};

/**
 * Maximum sizes for quantum keys and signatures
 * Based on liboqs v0.12.0 measurements
 */
static constexpr size_t MAX_QUANTUM_PUBKEY_SIZE = 1952;  // ML-DSA-65 public key
static constexpr size_t MAX_QUANTUM_PRIVKEY_SIZE = 4032; // ML-DSA-65 private key
static constexpr size_t MAX_QUANTUM_SIGNATURE_SIZE = 35664; // SLH-DSA signature

/**
 * A quantum-safe public key that can handle multiple algorithm types
 */
class CQuantumPubKey
{
private:
    KeyType m_type{KeyType::ECDSA};
    std::vector<unsigned char> m_vchPubKey;
    
public:
    CQuantumPubKey() = default;
    
    CQuantumPubKey(KeyType type, const std::vector<unsigned char>& vchPubKey)
        : m_type(type), m_vchPubKey(vchPubKey) {}
    
    // Convert from traditional CPubKey
    explicit CQuantumPubKey(const CPubKey& pubkey);
    
    // Get the key type
    KeyType GetType() const { return m_type; }
    
    // Get the raw public key data
    const std::vector<unsigned char>& GetKeyData() const { return m_vchPubKey; }
    
    // Check if the key is valid
    bool IsValid() const;
    
    // Get the size of the public key
    size_t size() const { return m_vchPubKey.size(); }
    
    // Get key ID (hash of the public key)
    CKeyID GetID() const;
    
    // Convert to CPubKey (only for ECDSA keys)
    bool GetCPubKey(CPubKey& pubkey) const;
    
    // Serialization
    template<typename Stream>
    void Serialize(Stream& s) const {
        uint8_t type = static_cast<uint8_t>(m_type);
        s << type << m_vchPubKey;
    }
    
    template<typename Stream>
    void Unserialize(Stream& s) {
        uint8_t type;
        s >> type >> m_vchPubKey;
        m_type = static_cast<KeyType>(type);
    }
    
    // Comparison operators
    friend bool operator==(const CQuantumPubKey& a, const CQuantumPubKey& b) {
        return a.m_type == b.m_type && a.m_vchPubKey == b.m_vchPubKey;
    }
    
    friend bool operator!=(const CQuantumPubKey& a, const CQuantumPubKey& b) {
        return !(a == b);
    }
    
    friend bool operator<(const CQuantumPubKey& a, const CQuantumPubKey& b) {
        if (a.m_type != b.m_type) return a.m_type < b.m_type;
        return a.m_vchPubKey < b.m_vchPubKey;
    }
};

/**
 * A quantum-safe private key that can handle multiple algorithm types
 */
class CQuantumKey
{
private:
    KeyType m_type{KeyType::ECDSA};
    secure_vector m_vchPrivKey;
    std::vector<unsigned char> m_vchPubKey;  // Store public key for quantum algorithms
    bool m_fValid{false};
    
    // For ECDSA compatibility
    std::unique_ptr<CKey> m_ecdsaKey;
    
public:
    CQuantumKey() = default;
    ~CQuantumKey();
    
    // Disable copying for security
    CQuantumKey(const CQuantumKey&) = delete;
    CQuantumKey& operator=(const CQuantumKey&) = delete;
    
    // Allow moving
    CQuantumKey(CQuantumKey&&) noexcept = default;
    CQuantumKey& operator=(CQuantumKey&&) noexcept = default;
    
    // Create from existing CKey (ECDSA)
    explicit CQuantumKey(const CKey& key);
    
    // Generate a new key of the specified type
    void MakeNewKey(KeyType type);
    
    // Check if the key is valid
    bool IsValid() const { return m_fValid; }
    
    // Get the key type
    KeyType GetType() const { return m_type; }
    
    // Get the public key
    CQuantumPubKey GetPubKey() const;
    
    // Sign a message
    bool Sign(const uint256& hash, std::vector<unsigned char>& vchSig) const;
    
    // Verify a signature (static method)
    static bool Verify(const uint256& hash, const std::vector<unsigned char>& vchSig, 
                      const CQuantumPubKey& pubkey);
    
    // Get the underlying CKey (only for ECDSA keys)
    const CKey* GetECDSAKey() const;
    
    // Derive child key (BIP32)
    bool Derive(CQuantumKey& keyChild, ChainCode& ccChild, unsigned int nChild, 
                const ChainCode& cc) const;
    
    // Load from serialized data
    bool Load(const secure_vector& privkey, const CQuantumPubKey& pubkey);
    
    // Get serialized private key data (be careful with this!)
    secure_vector GetPrivKeyData() const;
    
    // Clear the key data
    void Clear();
};

/**
 * Extended key with quantum support
 */
struct CExtQuantumKey {
    unsigned char nDepth;
    unsigned char vchFingerprint[4];
    unsigned int nChild;
    ChainCode chaincode;
    CQuantumKey key;
    
    CExtQuantumKey() = default;
    
    void Encode(std::vector<unsigned char>& code) const;
    void Decode(const std::vector<unsigned char>& code);
    bool Derive(CExtQuantumKey& out, unsigned int nChild) const;
    
    CQuantumPubKey GetPubKey() const {
        return key.GetPubKey();
    }
};

} // namespace quantum

#endif // BITCOIN_CRYPTO_QUANTUM_KEY_H