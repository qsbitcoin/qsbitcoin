// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CRYPTO_MLDSA_SCHEME_H
#define BITCOIN_CRYPTO_MLDSA_SCHEME_H

#include <crypto/signature_scheme.h>
#include <script/quantum_signature.h>

namespace quantum {

/**
 * ML-DSA (Module Lattice Digital Signature Algorithm) implementation
 * Using ML-DSA-65 parameters as specified by NIST
 * This provides quantum-safe signatures for standard transactions
 */
class MLDSAScheme : public ISignatureScheme {
public:
    MLDSAScheme();
    ~MLDSAScheme() override;
    
    bool Sign(const uint256& hash, const CKey& key, 
              std::vector<unsigned char>& sig) const override;
    
    bool Verify(const uint256& hash, const CPubKey& pubkey,
                const std::vector<unsigned char>& sig) const override;
    
    size_t GetMaxSignatureSize() const override { 
        return ML_DSA_65_SIG_SIZE;
    }
    
    size_t GetPublicKeySize() const override { 
        return ML_DSA_65_PUBKEY_SIZE;
    }
    
    size_t GetPrivateKeySize() const override { 
        return ML_DSA_65_PRIVKEY_SIZE;
    }
    
    SignatureSchemeId GetSchemeId() const override { 
        return SignatureSchemeId::ML_DSA; 
    }
    
    std::string GetName() const override { 
        return "ML-DSA-65"; 
    }
    
    bool IsQuantumSafe() const override { 
        return true; 
    }
    
private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace quantum

#endif // BITCOIN_CRYPTO_MLDSA_SCHEME_H