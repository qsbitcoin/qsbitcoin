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

BOOST_AUTO_TEST_CASE(quantum_fee_adjustment_basic)
{
    // Test basic fee adjustment for different signature types
    CAmount base_fee = 10000;
    
    // Create transaction with no quantum signatures
    CMutableTransaction tx_ecdsa;
    tx_ecdsa.version = 2;
    tx_ecdsa.nLockTime = 0;
    
    CTxIn input1;
    input1.prevout = COutPoint(Txid::FromHex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef").value(), 0);
    input1.scriptSig << std::vector<unsigned char>(71, 0xFF); // ECDSA dummy sig
    tx_ecdsa.vin.push_back(input1);
    
    CTransaction ecdsa_tx(tx_ecdsa);
    BOOST_CHECK_EQUAL(quantum::GetQuantumAdjustedFee(base_fee, ecdsa_tx), base_fee);
    
    // Create transaction with ML-DSA signature
    CMutableTransaction tx_mldsa;
    tx_mldsa.version = 2;
    tx_mldsa.nLockTime = 0;
    
    CQuantumKey mldsaKey;
    mldsaKey.MakeNewKey(KeyType::ML_DSA_65);
    
    uint256 hash;
    hash.SetNull();
    std::vector<unsigned char> mldsaSig;
    BOOST_CHECK(mldsaKey.Sign(hash, mldsaSig));
    
    QuantumSignature qsig_mldsa(SCHEME_ML_DSA_65, mldsaSig, mldsaKey.GetPubKey().GetKeyData());
    std::vector<unsigned char> encodedSig = EncodeQuantumSignature(qsig_mldsa);
    
    CTxIn input2;
    input2.prevout = COutPoint(Txid::FromHex("fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210").value(), 0);
    input2.scriptSig << encodedSig;
    tx_mldsa.vin.push_back(input2);
    
    CTransaction mldsa_tx(tx_mldsa);
    
    // Expected: base_fee * 1.5 * 0.9 (ML-DSA discount)
    CAmount expected_mldsa = static_cast<CAmount>(base_fee * 1.5 * 0.9);
    BOOST_CHECK_EQUAL(quantum::GetQuantumAdjustedFee(base_fee, mldsa_tx), expected_mldsa);
    
    // Create transaction with SLH-DSA signature
    CMutableTransaction tx_slhdsa;
    tx_slhdsa.version = 2;
    tx_slhdsa.nLockTime = 0;
    
    CQuantumKey slhdsaKey;
    slhdsaKey.MakeNewKey(KeyType::SLH_DSA_192F);
    
    std::vector<unsigned char> slhdsaSig;
    BOOST_CHECK(slhdsaKey.Sign(hash, slhdsaSig));
    
    QuantumSignature qsig_slhdsa(SCHEME_SLH_DSA_192F, slhdsaSig, slhdsaKey.GetPubKey().GetKeyData());
    std::vector<unsigned char> encodedSigSlh = EncodeQuantumSignature(qsig_slhdsa);
    
    CTxIn input3;
    input3.prevout = COutPoint(Txid::FromHex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef").value(), 1);
    input3.scriptSig << encodedSigSlh;
    tx_slhdsa.vin.push_back(input3);
    
    CTransaction slhdsa_tx(tx_slhdsa);
    
    // Expected: base_fee * 1.5 * 0.95 (SLH-DSA discount)
    CAmount expected_slhdsa = static_cast<CAmount>(base_fee * 1.5 * 0.95);
    BOOST_CHECK_EQUAL(quantum::GetQuantumAdjustedFee(base_fee, slhdsa_tx), expected_slhdsa);
}

BOOST_AUTO_TEST_CASE(quantum_fee_mixed_signatures)
{
    // Test fee calculation with mixed signature types
    CAmount base_fee = 20000;
    
    CMutableTransaction mtx;
    mtx.version = 2;
    mtx.nLockTime = 0;
    
    // Add ECDSA input
    CTxIn ecdsa_input;
    ecdsa_input.scriptSig << std::vector<unsigned char>(71, 0xAA);
    mtx.vin.push_back(ecdsa_input);
    
    // Add ML-DSA input
    CQuantumKey mldsaKey;
    mldsaKey.MakeNewKey(KeyType::ML_DSA_65);
    uint256 hash;
    hash.SetNull();
    std::vector<unsigned char> mldsaSig;
    mldsaKey.Sign(hash, mldsaSig);
    
    QuantumSignature qsig_mldsa(SCHEME_ML_DSA_65, mldsaSig, mldsaKey.GetPubKey().GetKeyData());
    CTxIn mldsa_input;
    mldsa_input.scriptSig << EncodeQuantumSignature(qsig_mldsa);
    mtx.vin.push_back(mldsa_input);
    
    // Add SLH-DSA input
    CQuantumKey slhdsaKey;
    slhdsaKey.MakeNewKey(KeyType::SLH_DSA_192F);
    std::vector<unsigned char> slhdsaSig;
    slhdsaKey.Sign(hash, slhdsaSig);
    
    QuantumSignature qsig_slhdsa(SCHEME_SLH_DSA_192F, slhdsaSig, slhdsaKey.GetPubKey().GetKeyData());
    CTxIn slhdsa_input;
    slhdsa_input.scriptSig << EncodeQuantumSignature(qsig_slhdsa);
    mtx.vin.push_back(slhdsa_input);
    
    CTransaction tx(mtx);
    
    // Should apply weighted average of quantum signature discounts
    // avg_discount = (1 * 0.9 + 1 * 0.95) / 2 = 0.925
    CAmount expected = static_cast<CAmount>(base_fee * 1.5 * 0.925);
    BOOST_CHECK_EQUAL(quantum::GetQuantumAdjustedFee(base_fee, tx), expected);
}

BOOST_AUTO_TEST_CASE(quantum_fee_multiple_same_type)
{
    // Test fee calculation with multiple signatures of the same type
    CAmount base_fee = 30000;
    
    CMutableTransaction mtx;
    mtx.version = 2;
    mtx.nLockTime = 0;
    
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    uint256 hash;
    hash.SetNull();
    
    // Add 3 ML-DSA inputs
    for (int i = 0; i < 3; i++) {
        std::vector<unsigned char> sig;
        key.Sign(hash, sig);
        
        QuantumSignature qsig(SCHEME_ML_DSA_65, sig, key.GetPubKey().GetKeyData());
        CTxIn input;
        input.scriptSig << EncodeQuantumSignature(qsig);
        mtx.vin.push_back(input);
    }
    
    CTransaction tx(mtx);
    
    // All ML-DSA, so discount should be 0.9
    CAmount expected = static_cast<CAmount>(base_fee * 1.5 * 0.9);
    BOOST_CHECK_EQUAL(quantum::GetQuantumAdjustedFee(base_fee, tx), expected);
}

BOOST_AUTO_TEST_CASE(quantum_fee_minimum_protection)
{
    // Test that adjusted fee never goes below base fee
    CAmount base_fee = 1000;
    
    CMutableTransaction mtx;
    mtx.version = 2;
    mtx.nLockTime = 0;
    
    // Add ML-DSA input
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
    
    CTransaction tx(mtx);
    
    // Even with discount, fee should be at least base_fee
    CAmount adjusted = quantum::GetQuantumAdjustedFee(base_fee, tx);
    BOOST_CHECK_GE(adjusted, base_fee);
}

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

BOOST_AUTO_TEST_CASE(quantum_fee_estimation_accuracy)
{
    // Test that fee estimation correctly accounts for signature sizes
    CAmount base_fee_rate = 1000; // satoshis per kvB
    
    // Create a transaction with known sizes
    CMutableTransaction mtx;
    mtx.version = 2;
    mtx.nLockTime = 0;
    
    // Add output
    CTxOut output;
    output.nValue = 50000;
    output.scriptPubKey = CScript() << OP_DUP << OP_HASH160 << std::vector<unsigned char>(20, 0) << OP_EQUALVERIFY << OP_CHECKSIG;
    mtx.vout.push_back(output);
    
    // Add ML-DSA input
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    uint256 hash;
    hash.SetNull();
    std::vector<unsigned char> sig;
    key.Sign(hash, sig);
    
    QuantumSignature qsig(SCHEME_ML_DSA_65, sig, key.GetPubKey().GetKeyData());
    CTxIn input;
    input.prevout = COutPoint(Txid::FromHex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef").value(), 0);
    input.scriptSig << EncodeQuantumSignature(qsig);
    mtx.vin.push_back(input);
    
    CTransaction tx(mtx);
    
    // Calculate transaction size
    unsigned int tx_size = GetTransactionWeight(tx);
    unsigned int tx_vsize = (tx_size + WITNESS_SCALE_FACTOR - 1) / WITNESS_SCALE_FACTOR;
    
    // Calculate base fee
    CAmount base_fee = (base_fee_rate * tx_vsize) / 1000;
    
    // Calculate adjusted fee
    CAmount adjusted_fee = quantum::GetQuantumAdjustedFee(base_fee, tx);
    
    // Verify the adjustment is correct
    CAmount expected = static_cast<CAmount>(base_fee * 1.5 * 0.9);
    BOOST_CHECK_EQUAL(adjusted_fee, expected);
}

BOOST_AUTO_TEST_SUITE_END()