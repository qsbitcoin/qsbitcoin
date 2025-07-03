// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CRYPTO_ECDSA_SCHEME_H
#define BITCOIN_CRYPTO_ECDSA_SCHEME_H

#include <crypto/signature_scheme.h>

namespace quantum {

/**
 * ECDSA signature scheme implementation
 * This wraps the existing Bitcoin Core ECDSA functionality
 * to provide compatibility with the ISignatureScheme interface
 */
class ECDSAScheme : public ISignatureScheme {
public:
    ECDSAScheme() = default;
    ~ECDSAScheme() override = default;
    
    bool Sign(const uint256& hash, const CKey& key, 
              std::vector<unsigned char>& sig) const override;
    
    bool Verify(const uint256& hash, const CPubKey& pubkey,
                const std::vector<unsigned char>& sig) const override;
    
    size_t GetMaxSignatureSize() const override { 
        return 72; // DER-encoded ECDSA signature max size
    }
    
    size_t GetPublicKeySize() const override { 
        return 33; // Compressed public key
    }
    
    size_t GetPrivateKeySize() const override { 
        return 32; // 256-bit private key
    }
    
    SignatureSchemeID GetSchemeId() const override { 
        return SCHEME_ECDSA; 
    }
    
    std::string GetName() const override { 
        return "ECDSA"; 
    }
    
    bool IsQuantumSafe() const override { 
        return false; 
    }
};

} // namespace quantum

#endif // BITCOIN_CRYPTO_ECDSA_SCHEME_H