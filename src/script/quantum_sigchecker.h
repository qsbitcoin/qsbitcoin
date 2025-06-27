// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_QUANTUM_SIGCHECKER_H
#define BITCOIN_SCRIPT_QUANTUM_SIGCHECKER_H

#include <script/interpreter.h>
#include <script/quantum_signature.h>
#include <crypto/quantum_key.h>
#include <sync.h>

#include <memory>
#include <unordered_map>

// Forward declaration to avoid including oqs.h in header
typedef struct OQS_SIG OQS_SIG;

namespace quantum {

/**
 * Cache for OQS signature contexts (simplified for now)
 */
class OQSContextCache {
public:
    /** Get or create an OQS_SIG context for the given scheme */
    OQS_SIG* GetContext(SignatureSchemeID scheme_id) const;
    
    /** Clear the cache */
    void Clear();
    
    /** Get cache statistics */
    size_t GetCacheSize() const;
};

/** Global OQS context cache */
extern OQSContextCache g_oqs_context_cache;

} // namespace quantum

/**
 * Extended signature checker that supports quantum signatures
 * This inherits from BaseSignatureChecker and adds quantum signature support
 */
class QuantumSignatureChecker : public BaseSignatureChecker
{
public:
    /** Check a quantum signature */
    virtual bool CheckQuantumSignature(
        const std::vector<unsigned char>& vchSig,
        const std::vector<unsigned char>& vchPubKey,
        const CScript& scriptCode,
        SigVersion sigversion,
        quantum::SignatureSchemeID scheme_id) const
    {
        return false;
    }
    
    /** Verify a quantum signature against a hash */
    virtual bool VerifyQuantumSignature(
        const std::vector<unsigned char>& vchSig,
        const quantum::CQuantumPubKey& pubkey,
        const uint256& sighash) const;
};

/**
 * Transaction signature checker with quantum support
 * This extends GenericTransactionSignatureChecker to add quantum signatures
 */
template <class T>
class QuantumTransactionSignatureChecker : public GenericTransactionSignatureChecker<T>
{
private:
    // We need to store these to access them since base class has them private
    const T* m_tx;
    unsigned int m_nIn;
    const CAmount m_amount;
    const PrecomputedTransactionData* m_txdata;
    
public:
    QuantumTransactionSignatureChecker(const T* tx, unsigned int nInIn, const CAmount& amountIn, 
                                       const PrecomputedTransactionData& txdataIn,
                                       MissingDataBehavior mdb = MissingDataBehavior::ASSERT_FAIL)
        : GenericTransactionSignatureChecker<T>(tx, nInIn, amountIn, txdataIn, mdb),
          m_tx(tx), m_nIn(nInIn), m_amount(amountIn), m_txdata(&txdataIn) {}
    
    bool CheckQuantumSignature(
        const std::vector<unsigned char>& vchSig,
        const std::vector<unsigned char>& vchPubKey,
        const CScript& scriptCode,
        SigVersion sigversion,
        uint8_t scheme_id) const override;
        
protected:
    bool VerifyQuantumSignature(
        const std::vector<unsigned char>& vchSig,
        const quantum::CQuantumPubKey& pubkey,
        const uint256& sighash) const;
};

#endif // BITCOIN_SCRIPT_QUANTUM_SIGCHECKER_H