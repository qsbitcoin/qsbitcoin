// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <policy/quantum_policy.h>
#include <policy/policy.h>
#include <policy/feerate.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/quantum_signature.h>
#include <crypto/quantum_key.h>
#include <validation.h>
#include <test/util/setup_common.h>
#include <test/util/transaction_utils.h>

using namespace quantum;

BOOST_FIXTURE_TEST_SUITE(quantum_fee_tests, BasicTestingSetup)





BOOST_AUTO_TEST_CASE(quantum_transaction_weight_limits)
{
    // Test weight limit enforcement for quantum transactions
    
    // Standard transaction should pass normal weight check
    BOOST_CHECK(quantum::IsStandardTxWeight(100000, false));
    BOOST_CHECK(quantum::IsStandardTxWeight(MAX_STANDARD_TX_WEIGHT, false));
    BOOST_CHECK(!quantum::IsStandardTxWeight(MAX_STANDARD_TX_WEIGHT + 1, false));
    
    // Quantum transaction should pass higher weight check
    BOOST_CHECK(quantum::IsStandardTxWeight(100000, true));
    BOOST_CHECK(quantum::IsStandardTxWeight(MAX_STANDARD_TX_WEIGHT + 1, true));
    BOOST_CHECK(quantum::IsStandardTxWeight(MAX_STANDARD_TX_WEIGHT_QUANTUM, true));
    BOOST_CHECK(!quantum::IsStandardTxWeight(MAX_STANDARD_TX_WEIGHT_QUANTUM + 1, true));
}

BOOST_AUTO_TEST_CASE(quantum_signature_counting)
{
    // Test accurate counting of quantum signatures
    CMutableTransaction mtx;
    mtx.version = 2;
    mtx.nLockTime = 0;
    
    // Initially no quantum signatures
    CTransaction tx0(mtx);
    BOOST_CHECK_EQUAL(quantum::CountQuantumSignatures(tx0), 0);
    BOOST_CHECK(!quantum::HasQuantumSignatures(tx0));
    
    // Add one ML-DSA signature
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    uint256 hash;
    hash.SetNull();
    std::vector<unsigned char> sig;
    key.Sign(hash, sig);
    
    QuantumSignature qsig(SCHEME_ML_DSA_65, sig, key.GetPubKey().GetKeyData());
    CTxIn input;
    input.scriptSig << EncodeQuantumSignature(qsig);
    mtx.vin.push_back(input);
    
    CTransaction tx1(mtx);
    BOOST_CHECK_EQUAL(quantum::CountQuantumSignatures(tx1), 1);
    BOOST_CHECK(quantum::HasQuantumSignatures(tx1));
    
    // Add ECDSA signature (should not be counted)
    CTxIn ecdsa_input;
    ecdsa_input.scriptSig << std::vector<unsigned char>(71, 0xFF);
    mtx.vin.push_back(ecdsa_input);
    
    CTransaction tx2(mtx);
    BOOST_CHECK_EQUAL(quantum::CountQuantumSignatures(tx2), 1);
    
    // Add SLH-DSA signature
    CQuantumKey key2;
    key2.MakeNewKey(KeyType::SLH_DSA_192F);
    std::vector<unsigned char> sig2;
    key2.Sign(hash, sig2);
    
    QuantumSignature qsig2(SCHEME_SLH_DSA_192F, sig2, key2.GetPubKey().GetKeyData());
    CTxIn input2;
    input2.scriptSig << EncodeQuantumSignature(qsig2);
    mtx.vin.push_back(input2);
    
    CTransaction tx3(mtx);
    BOOST_CHECK_EQUAL(quantum::CountQuantumSignatures(tx3), 2);
}

BOOST_AUTO_TEST_CASE(quantum_signature_policy_enforcement)
{
    std::string reason;
    
    // Empty transaction should pass
    CMutableTransaction mtx;
    CTransaction tx_empty(mtx);
    BOOST_CHECK(quantum::CheckQuantumSignaturePolicy(tx_empty, reason));
    
    // Transaction with valid number of quantum signatures
    mtx.version = 2;
    mtx.nLockTime = 0;
    
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    uint256 hash;
    hash.SetNull();
    
    // Add 5 quantum signatures (should pass)
    for (int i = 0; i < 5; i++) {
        std::vector<unsigned char> sig;
        key.Sign(hash, sig);
        
        QuantumSignature qsig(SCHEME_ML_DSA_65, sig, key.GetPubKey().GetKeyData());
        CTxIn input;
        input.scriptSig << EncodeQuantumSignature(qsig);
        mtx.vin.push_back(input);
    }
    
    CTransaction tx_valid(mtx);
    BOOST_CHECK(quantum::CheckQuantumSignaturePolicy(tx_valid, reason));
    
    // Add more signatures to exceed limit
    for (int i = 0; i < 10; i++) {
        std::vector<unsigned char> sig;
        key.Sign(hash, sig);
        
        QuantumSignature qsig(SCHEME_ML_DSA_65, sig, key.GetPubKey().GetKeyData());
        CTxIn input;
        input.scriptSig << EncodeQuantumSignature(qsig);
        mtx.vin.push_back(input);
    }
    
    CTransaction tx_invalid(mtx);
    BOOST_CHECK(!quantum::CheckQuantumSignaturePolicy(tx_invalid, reason));
    BOOST_CHECK_EQUAL(reason, "too many quantum signatures");
}


BOOST_AUTO_TEST_SUITE_END()