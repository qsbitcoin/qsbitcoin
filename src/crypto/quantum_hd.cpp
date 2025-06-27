// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/quantum_hd.h>
#include <crypto/hmac_sha512.h>
#include <crypto/sha256.h>
#include <crypto/ripemd160.h>
#include <hash.h>
#include <support/cleanse.h>

#include <cstring>

namespace quantum {

// Helper function to perform BIP32-style hash
static void QuantumBIP32Hash(const ChainCode& cc, unsigned int nChild, 
                            const unsigned char* data, size_t data_len,
                            unsigned char out[64])
{
    unsigned char num[4];
    WriteBE32(num, nChild);
    
    CHMAC_SHA512 hmac(cc.begin(), cc.size());
    hmac.Write(data, data_len);
    hmac.Write(num, 4);
    hmac.Finalize(out);
}

bool DeriveQuantumChild(CQuantumKey& keyChild, ChainCode& ccChild, 
                       unsigned int nChild, const ChainCode& cc,
                       const secure_vector& keyParent, KeyType keyType)
{
    // For quantum keys, we always use "hardened" derivation
    // (using the private key material)
    if (keyParent.empty()) {
        return false;
    }
    
    unsigned char out[64];
    
    // For quantum keys, we hash a portion of the parent private key
    // We use the first 32 bytes of the private key as input
    size_t hash_len = std::min(size_t(32), keyParent.size());
    
    // Add a type byte to ensure different key types have different derivations
    unsigned char data[33];
    data[0] = static_cast<unsigned char>(keyType);
    memcpy(data + 1, keyParent.data(), hash_len);
    
    QuantumBIP32Hash(cc, nChild, data, hash_len + 1, out);
    
    // Use the first 32 bytes as seed for new key generation
    // The last 32 bytes become the new chain code
    memcpy(ccChild.begin(), out + 32, 32);
    
    // Generate new quantum key using derived seed
    // We'll use the derived material as additional entropy
    keyChild.Clear();
    keyChild.MakeNewKey(keyType);
    
    // TODO: In a full implementation, we would use the derived seed
    // to deterministically generate the quantum key. For now, we're
    // using the RNG which means keys won't be reproducible.
    
    // Clean up
    memory_cleanse(out, sizeof(out));
    memory_cleanse(data, sizeof(data));
    
    return keyChild.IsValid();
}

bool GenerateQuantumMaster(CQuantumKey& keyMaster, ChainCode& ccMaster,
                          std::span<const std::byte> seed, KeyType keyType)
{
    if (seed.size() < 32) {
        return false;
    }
    
    // Similar to BIP32, use HMAC-SHA512 with "Bitcoin seed" as key
    static const unsigned char hashkey[] = "Bitcoin seed";
    
    unsigned char out[64];
    CHMAC_SHA512 hmac(hashkey, sizeof(hashkey) - 1);
    hmac.Write(reinterpret_cast<const unsigned char*>(seed.data()), seed.size());
    hmac.Finalize(out);
    
    // First 32 bytes for key generation seed
    // Last 32 bytes for chain code
    memcpy(ccMaster.begin(), out + 32, 32);
    
    // Generate master key
    keyMaster.Clear();
    keyMaster.MakeNewKey(keyType);
    
    // TODO: Use the seed to deterministically generate the key
    
    // Clean up
    memory_cleanse(out, sizeof(out));
    
    return keyMaster.IsValid();
}

void GetQuantumKeyFingerprint(const CQuantumPubKey& pubkey, 
                             unsigned char fingerprint[4])
{
    // Calculate fingerprint as first 4 bytes of RIPEMD160(SHA256(pubkey))
    // This matches BIP32 fingerprint calculation
    CKeyID id = pubkey.GetID();
    memcpy(fingerprint, &id, 4);
}

} // namespace quantum