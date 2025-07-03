// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/quantum_sigchecker.h>
#include <crypto/quantum_key.h>
#include <hash.h>
#include <logging.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <uint256.h>

#include <algorithm>

namespace quantum {

// Global context cache instance (simplified implementation)
OQSContextCache g_oqs_context_cache;

OQS_SIG* OQSContextCache::GetContext(SignatureSchemeID scheme_id) const
{
    // For now, just return nullptr - the actual quantum signature verification
    // is handled by CQuantumKey which manages its own contexts
    return nullptr;
}

void OQSContextCache::Clear()
{
    // No-op for now
}

size_t OQSContextCache::GetCacheSize() const
{
    return 0;
}

} // namespace quantum

bool QuantumSignatureChecker::VerifyQuantumSignature(
    const std::vector<unsigned char>& vchSig,
    const quantum::CQuantumPubKey& pubkey,
    const uint256& sighash) const
{
    // Use the global quantum key verification
    return quantum::CQuantumKey::Verify(sighash, vchSig, pubkey);
}

template <class T>
bool QuantumTransactionSignatureChecker<T>::CheckQuantumSignature(
    const std::vector<unsigned char>& vchSig,
    const std::vector<unsigned char>& vchPubKey,
    const CScript& scriptCode,
    SigVersion sigversion,
    uint8_t scheme_id) const
{
    // Determine key type from scheme ID
    quantum::KeyType keyType;
    switch (static_cast<quantum::SignatureSchemeID>(scheme_id)) {
        case quantum::SCHEME_ML_DSA_65:
            keyType = quantum::KeyType::ML_DSA_65;
            break;
        case quantum::SCHEME_SLH_DSA_192F:
            keyType = quantum::KeyType::SLH_DSA_192F;
            break;
        default:
            return false;
    }
    
    // Create quantum public key
    quantum::CQuantumPubKey pubkey(keyType, vchPubKey);
    if (!pubkey.IsValid()) {
        return false;
    }
    
    // Extract hash type from signature (last byte)
    if (vchSig.empty()) {
        return false;
    }
    
    std::vector<unsigned char> vchSigNoHashType;
    int nHashType = SIGHASH_ALL;
    
    if (vchSig.size() > 0) {
        nHashType = vchSig.back();
        // Remove first byte (algorithm ID) and last byte (hash type)
        if (vchSig.size() > 2) {
            vchSigNoHashType.assign(vchSig.begin() + 1, vchSig.end() - 1);
        }
    }
    
    // Compute the signature hash using our stored member variables
    uint256 sighash = SignatureHash(scriptCode, *m_tx, m_nIn, nHashType, 
                                    m_amount, sigversion, m_txdata);
    
    // Verify the signature
    return VerifyQuantumSignature(vchSigNoHashType, pubkey, sighash);
}

template <class T>
bool QuantumTransactionSignatureChecker<T>::VerifyQuantumSignature(
    const std::vector<unsigned char>& vchSig,
    const quantum::CQuantumPubKey& pubkey,
    const uint256& sighash) const
{
    return quantum::CQuantumKey::Verify(sighash, vchSig, pubkey);
}

// Explicit template instantiations
template class QuantumTransactionSignatureChecker<CTransaction>;
template class QuantumTransactionSignatureChecker<CMutableTransaction>;