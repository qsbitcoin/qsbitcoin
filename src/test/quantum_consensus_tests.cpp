// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <policy/quantum_policy.h>
#include <consensus/validation.h>
#include <crypto/quantum_key.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/quantum_signature.h>
#include <test/util/setup_common.h>

using namespace quantum;

BOOST_FIXTURE_TEST_SUITE(quantum_consensus_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(quantum_weight_limits)
{
    // Test weight limits for quantum transactions
    
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
    // Create a transaction with quantum signatures
    CMutableTransaction mtx;
    mtx.version = 2;
    mtx.nLockTime = 0;
    
    // Add input without quantum signature
    CTxIn input1;
    input1.prevout = COutPoint(Txid::FromHex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef").value(), 0);
    input1.scriptSig << std::vector<unsigned char>(71, 0xFF); // ECDSA-sized dummy sig
    mtx.vin.push_back(input1);
    
    CTransaction tx1(mtx);
    BOOST_CHECK_EQUAL(quantum::CountQuantumSignatures(tx1), 0);
    BOOST_CHECK(!quantum::HasQuantumSignatures(tx1));
    
    // Add input with ML-DSA signature
    CQuantumKey mldsaKey;
    mldsaKey.MakeNewKey(KeyType::ML_DSA_65);
    
    uint256 hash;
    hash.SetNull();
    std::vector<unsigned char> mldsaSig;
    BOOST_CHECK(mldsaKey.Sign(hash, mldsaSig));
    
    QuantumSignature qsig(SCHEME_ML_DSA_65, mldsaSig, mldsaKey.GetPubKey().GetKeyData());
    std::vector<unsigned char> encodedSig = EncodeQuantumSignature(qsig);
    
    CTxIn input2;
    input2.prevout = COutPoint(Txid::FromHex("fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210").value(), 0);
    input2.scriptSig << encodedSig;
    mtx.vin.push_back(input2);
    
    CTransaction tx2(mtx);
    BOOST_CHECK_EQUAL(quantum::CountQuantumSignatures(tx2), 1);
    BOOST_CHECK(quantum::HasQuantumSignatures(tx2));
}

BOOST_AUTO_TEST_CASE(quantum_fee_adjustment)
{
    CMutableTransaction mtx;
    mtx.version = 2;
    mtx.nLockTime = 0;
    
    // Transaction with no quantum signatures
    CTxIn input1;
    input1.scriptSig << std::vector<unsigned char>(71, 0xFF);
    mtx.vin.push_back(input1);
    
    CTransaction tx1(mtx);
    CAmount base_fee = 10000;
    BOOST_CHECK_EQUAL(quantum::GetQuantumAdjustedFee(base_fee, tx1), base_fee);
    
    // Transaction with ML-DSA signature
    CQuantumKey mldsaKey;
    mldsaKey.MakeNewKey(KeyType::ML_DSA_65);
    
    uint256 hash;
    hash.SetNull();
    std::vector<unsigned char> mldsaSig;
    mldsaKey.Sign(hash, mldsaSig);
    
    QuantumSignature qsig(SCHEME_ML_DSA_65, mldsaSig, mldsaKey.GetPubKey().GetKeyData());
    std::vector<unsigned char> encodedSig = EncodeQuantumSignature(qsig);
    
    CTxIn input2;
    input2.scriptSig << encodedSig;
    mtx.vin.clear();
    mtx.vin.push_back(input2);
    
    CTransaction tx2(mtx);
    CAmount adjusted_fee = quantum::GetQuantumAdjustedFee(base_fee, tx2);
    
    // Should be base_fee * QUANTUM_FEE_MULTIPLIER * ML_DSA_FEE_DISCOUNT
    CAmount expected = static_cast<CAmount>(base_fee * 1.5 * 0.9);
    BOOST_CHECK_EQUAL(adjusted_fee, expected);
}

BOOST_AUTO_TEST_CASE(quantum_signature_policy)
{
    std::string reason;
    
    // Empty transaction should pass
    CMutableTransaction mtx;
    CTransaction tx1(mtx);
    BOOST_CHECK(quantum::CheckQuantumSignaturePolicy(tx1, reason));
    
    // Transaction with valid quantum signatures
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    
    uint256 hash;
    hash.SetNull();
    std::vector<unsigned char> sig;
    key.Sign(hash, sig);
    
    QuantumSignature qsig(SCHEME_ML_DSA_65, sig, key.GetPubKey().GetKeyData());
    std::vector<unsigned char> encodedSig = EncodeQuantumSignature(qsig);
    
    // Add multiple inputs with quantum signatures
    for (int i = 0; i < 5; i++) {
        CTxIn input;
        input.scriptSig << encodedSig;
        mtx.vin.push_back(input);
    }
    
    CTransaction tx2(mtx);
    BOOST_CHECK(quantum::CheckQuantumSignaturePolicy(tx2, reason));
    
    // Add too many quantum signatures
    for (int i = 0; i < 10; i++) {
        CTxIn input;
        input.scriptSig << encodedSig;
        mtx.vin.push_back(input);
    }
    
    CTransaction tx3(mtx);
    BOOST_CHECK(!quantum::CheckQuantumSignaturePolicy(tx3, reason));
    BOOST_CHECK_EQUAL(reason, "too many quantum signatures");
}

BOOST_AUTO_TEST_CASE(mixed_signature_types)
{
    // Test transaction with mixed ECDSA and quantum signatures
    CMutableTransaction mtx;
    mtx.version = 2;
    
    // Add ECDSA input
    CTxIn ecdsaInput;
    ecdsaInput.scriptSig << std::vector<unsigned char>(71, 0xAA); // ECDSA dummy
    mtx.vin.push_back(ecdsaInput);
    
    // Add ML-DSA input
    CQuantumKey mldsaKey;
    mldsaKey.MakeNewKey(KeyType::ML_DSA_65);
    uint256 hash1;
    hash1.SetNull();
    std::vector<unsigned char> mldsaSig;
    mldsaKey.Sign(hash1, mldsaSig);
    
    QuantumSignature mldsaQsig(SCHEME_ML_DSA_65, mldsaSig, mldsaKey.GetPubKey().GetKeyData());
    CTxIn mldsaInput;
    mldsaInput.scriptSig << EncodeQuantumSignature(mldsaQsig);
    mtx.vin.push_back(mldsaInput);
    
    // Add SLH-DSA input
    CQuantumKey slhdsaKey;
    slhdsaKey.MakeNewKey(KeyType::SLH_DSA_192F);
    uint256 hash2;
    hash2.SetNull();
    std::vector<unsigned char> slhdsaSig;
    slhdsaKey.Sign(hash2, slhdsaSig);
    
    QuantumSignature slhdsaQsig(SCHEME_SLH_DSA_192F, slhdsaSig, slhdsaKey.GetPubKey().GetKeyData());
    CTxIn slhdsaInput;
    slhdsaInput.scriptSig << EncodeQuantumSignature(slhdsaQsig);
    mtx.vin.push_back(slhdsaInput);
    
    CTransaction tx(mtx);
    
    // Should count 2 quantum signatures (ML-DSA and SLH-DSA)
    BOOST_CHECK_EQUAL(quantum::CountQuantumSignatures(tx), 2);
    BOOST_CHECK(quantum::HasQuantumSignatures(tx));
    
    // Check fee adjustment with mixed types
    CAmount base_fee = 10000;
    CAmount adjusted = quantum::GetQuantumAdjustedFee(base_fee, tx);
    
    // Should apply weighted average of discounts
    // avg_discount = (1 * 0.9 + 1 * 0.95) / 2 = 0.925
    // adjusted = base_fee * 1.5 * 0.925
    CAmount expected = static_cast<CAmount>(base_fee * 1.5 * 0.925);
    BOOST_CHECK_EQUAL(adjusted, expected);
}

BOOST_AUTO_TEST_SUITE_END()