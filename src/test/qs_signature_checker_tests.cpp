// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <script/quantum_sigchecker.h>
#include <script/quantum_signature.h>
#include <crypto/quantum_key.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <test/util/setup_common.h>
#include <uint256.h>

#include <vector>

using namespace quantum;

BOOST_FIXTURE_TEST_SUITE(qs_signature_checker_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(oqs_context_cache)
{
    // Test OQS context caching (simplified for now)
    quantum::OQSContextCache cache;
    
    // For now, the cache is simplified and always returns nullptr
    OQS_SIG* ctx1 = cache.GetContext(SCHEME_ML_DSA_65);
    BOOST_CHECK(ctx1 == nullptr);
    BOOST_CHECK_EQUAL(cache.GetCacheSize(), 0);
    
    // Clear cache (no-op)
    cache.Clear();
    BOOST_CHECK_EQUAL(cache.GetCacheSize(), 0);
}

BOOST_AUTO_TEST_CASE(quantum_signature_checker_basic)
{
    // Create a simple transaction
    CMutableTransaction mtx;
    mtx.version = 2;
    mtx.nLockTime = 0;
    
    // Add an input
    CTxIn input;
    input.prevout = COutPoint(Txid::FromHex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef").value(), 0);
    input.nSequence = CTxIn::SEQUENCE_FINAL;
    mtx.vin.push_back(input);
    
    // Add an output
    CTxOut output(10000, CScript() << OP_TRUE);
    mtx.vout.push_back(output);
    
    CTransaction tx(mtx);
    
    // Create precomputed transaction data
    PrecomputedTransactionData txdata;
    txdata.Init(tx, {output});
    
    // Create quantum signature checker
    QuantumTransactionSignatureChecker checker(&tx, 0, 10000, txdata);
    
    // Create a quantum key and sign
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    
    // Create a dummy script
    CScript scriptCode;
    scriptCode << key.GetPubKey().GetKeyData() << OP_CHECKSIG_ML_DSA;
    
    // Create signature hash
    uint256 sighash = SignatureHash(scriptCode, tx, 0, SIGHASH_ALL, 10000, SigVersion::BASE);
    
    // Sign with quantum key
    std::vector<unsigned char> sig;
    BOOST_CHECK(key.Sign(sighash, sig));
    
    // Add hash type to signature
    sig.push_back(SIGHASH_ALL);
    
    // Test quantum signature verification
    bool result = checker.CheckQuantumSignature(
        sig, 
        key.GetPubKey().GetKeyData(), 
        scriptCode, 
        SigVersion::BASE,
        static_cast<uint8_t>(SCHEME_ML_DSA_65)
    );
    
    BOOST_CHECK(result);
}

BOOST_AUTO_TEST_CASE(quantum_signature_checker_invalid)
{
    // Create a simple transaction
    CMutableTransaction mtx;
    mtx.version = 2;
    mtx.nLockTime = 0;
    
    CTxIn input;
    input.prevout = COutPoint(Txid::FromHex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef").value(), 0);
    input.nSequence = CTxIn::SEQUENCE_FINAL;
    mtx.vin.push_back(input);
    
    CTxOut output(10000, CScript() << OP_TRUE);
    mtx.vout.push_back(output);
    
    CTransaction tx(mtx);
    
    PrecomputedTransactionData txdata;
    txdata.Init(tx, {output});
    
    QuantumTransactionSignatureChecker checker(&tx, 0, 10000, txdata);
    
    // Create keys
    CQuantumKey key1;
    key1.MakeNewKey(KeyType::ML_DSA_65);
    
    CQuantumKey key2;
    key2.MakeNewKey(KeyType::ML_DSA_65);
    
    CScript scriptCode;
    scriptCode << key1.GetPubKey().GetKeyData() << OP_CHECKSIG_ML_DSA;
    
    // Sign with key1
    uint256 sighash = SignatureHash(scriptCode, tx, 0, SIGHASH_ALL, 10000, SigVersion::BASE);
    std::vector<unsigned char> sig;
    BOOST_CHECK(key1.Sign(sighash, sig));
    sig.push_back(SIGHASH_ALL);
    
    // Try to verify with wrong key (key2)
    bool result = checker.CheckQuantumSignature(
        sig, 
        key2.GetPubKey().GetKeyData(), // Wrong key!
        scriptCode, 
        SigVersion::BASE,
        static_cast<uint8_t>(SCHEME_ML_DSA_65)
    );
    
    BOOST_CHECK(!result); // Should fail
}

BOOST_AUTO_TEST_CASE(quantum_signature_checker_slh_dsa)
{
    // Test with SLH-DSA signature
    CMutableTransaction mtx;
    mtx.version = 2;
    mtx.nLockTime = 0;
    
    CTxIn input;
    input.prevout = COutPoint(Txid::FromHex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef").value(), 0);
    input.nSequence = CTxIn::SEQUENCE_FINAL;
    mtx.vin.push_back(input);
    
    CTxOut output(10000, CScript() << OP_TRUE);
    mtx.vout.push_back(output);
    
    CTransaction tx(mtx);
    
    PrecomputedTransactionData txdata;
    txdata.Init(tx, {output});
    
    QuantumTransactionSignatureChecker checker(&tx, 0, 10000, txdata);
    
    // Create SLH-DSA key
    CQuantumKey key;
    key.MakeNewKey(KeyType::SLH_DSA_192F);
    
    CScript scriptCode;
    scriptCode << key.GetPubKey().GetKeyData() << OP_CHECKSIG_SLH_DSA;
    
    uint256 sighash = SignatureHash(scriptCode, tx, 0, SIGHASH_ALL, 10000, SigVersion::BASE);
    
    std::vector<unsigned char> sig;
    BOOST_CHECK(key.Sign(sighash, sig));
    sig.push_back(SIGHASH_ALL);
    
    // Test SLH-DSA signature verification
    bool result = checker.CheckQuantumSignature(
        sig, 
        key.GetPubKey().GetKeyData(), 
        scriptCode, 
        SigVersion::BASE,
        static_cast<uint8_t>(SCHEME_SLH_DSA_192F)
    );
    
    BOOST_CHECK(result);
}

BOOST_AUTO_TEST_CASE(quantum_signature_malleability)
{
    // Test signature malleability protection
    CMutableTransaction mtx;
    mtx.version = 2;
    mtx.nLockTime = 0;
    
    CTxIn input;
    input.prevout = COutPoint(Txid::FromHex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef").value(), 0);
    input.nSequence = CTxIn::SEQUENCE_FINAL;
    mtx.vin.push_back(input);
    
    CTxOut output(10000, CScript() << OP_TRUE);
    mtx.vout.push_back(output);
    
    CTransaction tx(mtx);
    
    PrecomputedTransactionData txdata;
    txdata.Init(tx, {output});
    
    QuantumTransactionSignatureChecker checker(&tx, 0, 10000, txdata);
    
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    
    CScript scriptCode;
    scriptCode << key.GetPubKey().GetKeyData() << OP_CHECKSIG_ML_DSA;
    
    uint256 sighash = SignatureHash(scriptCode, tx, 0, SIGHASH_ALL, 10000, SigVersion::BASE);
    
    std::vector<unsigned char> sig;
    BOOST_CHECK(key.Sign(sighash, sig));
    sig.push_back(SIGHASH_ALL);
    
    // Original signature should verify
    BOOST_CHECK(checker.CheckQuantumSignature(
        sig, key.GetPubKey().GetKeyData(), scriptCode, SigVersion::BASE,
        static_cast<uint8_t>(SCHEME_ML_DSA_65)
    ));
    
    // Modify a byte in the signature (malleate it)
    if (sig.size() > 100) {
        sig[100] ^= 0x01; // Flip one bit
        
        // Malleated signature should not verify
        BOOST_CHECK(!checker.CheckQuantumSignature(
            sig, key.GetPubKey().GetKeyData(), scriptCode, SigVersion::BASE,
            static_cast<uint8_t>(SCHEME_ML_DSA_65)
        ));
    }
}

BOOST_AUTO_TEST_SUITE_END()