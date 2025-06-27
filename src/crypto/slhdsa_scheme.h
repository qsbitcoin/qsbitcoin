// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CRYPTO_SLHDSA_SCHEME_H
#define BITCOIN_CRYPTO_SLHDSA_SCHEME_H

#include <crypto/signature_scheme.h>

namespace quantum {

/**
 * SLH-DSA (Stateless Hash-based Digital Signature Algorithm) implementation
 * Using SLH-DSA-SHA2-192f parameters as specified by NIST
 * This provides quantum-safe signatures for high-security cold storage
 */
class SLHDSAScheme : public ISignatureScheme {
public:
    SLHDSAScheme();
    ~SLHDSAScheme() override;
    
    bool Sign(const uint256& hash, const CKey& key, 
              std::vector<unsigned char>& sig) const override;
    
    bool Verify(const uint256& hash, const CPubKey& pubkey,
                const std::vector<unsigned char>& sig) const override;
    
    size_t GetMaxSignatureSize() const override { 
        return 35664; // SLH-DSA-SHA2-192f signature size
    }
    
    size_t GetPublicKeySize() const override { 
        return 48; // SLH-DSA-SHA2-192f public key size
    }
    
    size_t GetPrivateKeySize() const override { 
        return 96; // SLH-DSA-SHA2-192f private key size
    }
    
    SignatureSchemeId GetSchemeId() const override { 
        return SignatureSchemeId::SLH_DSA; 
    }
    
    std::string GetName() const override { 
        return "SLH-DSA-SHA2-192f"; 
    }
    
    bool IsQuantumSafe() const override { 
        return true; 
    }
    
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace quantum

#endif // BITCOIN_CRYPTO_SLHDSA_SCHEME_H