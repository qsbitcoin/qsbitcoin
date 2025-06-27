// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <script/quantum_signature.h>
#include <crypto/quantum_key.h>
#include <primitives/transaction.h>
#include <consensus/validation.h>
#include <policy/policy.h>
#include <test/util/setup_common.h>

using namespace quantum;

BOOST_FIXTURE_TEST_SUITE(quantum_transaction_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(quantum_signature_serialization)
{
    // Test ML-DSA signature serialization
    {
        CQuantumKey key;
        key.MakeNewKey(KeyType::ML_DSA_65);
        
        uint256 hash;
        hash.SetNull();
        hash.data()[31] = 1;
        
        std::vector<unsigned char> sig;
        BOOST_CHECK(key.Sign(hash, sig));
        
        CQuantumPubKey pubkey = key.GetPubKey();
        std::vector<unsigned char> pubkey_data = pubkey.GetKeyData();
        
        // Create quantum signature structure
        QuantumSignature qsig(SCHEME_ML_DSA_65, sig, pubkey_data);
        
        // Serialize
        std::vector<unsigned char> serialized = EncodeQuantumSignature(qsig);
        
        // Deserialize
        QuantumSignature qsig2;
        BOOST_CHECK(ParseQuantumSignature(serialized, qsig2));
        
        // Verify fields match
        BOOST_CHECK_EQUAL(qsig2.scheme_id, SCHEME_ML_DSA_65);
        BOOST_CHECK(qsig2.signature == sig);
        BOOST_CHECK(qsig2.pubkey == pubkey_data);
    }
    
    // Test SLH-DSA signature serialization
    {
        CQuantumKey key;
        key.MakeNewKey(KeyType::SLH_DSA_192F);
        
        uint256 hash;
        hash.SetNull();
        hash.data()[31] = 2;
        
        std::vector<unsigned char> sig;
        BOOST_CHECK(key.Sign(hash, sig));
        
        CQuantumPubKey pubkey = key.GetPubKey();
        std::vector<unsigned char> pubkey_data = pubkey.GetKeyData();
        
        // Create quantum signature structure
        QuantumSignature qsig(SCHEME_SLH_DSA_192F, sig, pubkey_data);
        
        // Serialize
        std::vector<unsigned char> serialized = EncodeQuantumSignature(qsig);
        
        // Deserialize
        QuantumSignature qsig2;
        BOOST_CHECK(ParseQuantumSignature(serialized, qsig2));
        
        // Verify fields match
        BOOST_CHECK_EQUAL(qsig2.scheme_id, SCHEME_SLH_DSA_192F);
        BOOST_CHECK(qsig2.signature == sig);
        BOOST_CHECK(qsig2.pubkey == pubkey_data);
    }
}

BOOST_AUTO_TEST_CASE(quantum_signature_size_validation)
{
    // Test that oversized signatures are rejected
    {
        std::vector<unsigned char> huge_sig(MAX_QUANTUM_SIG_SIZE + 1, 0xFF);
        std::vector<unsigned char> normal_pubkey(100, 0xAA);
        
        QuantumSignature qsig(SCHEME_ML_DSA_65, huge_sig, normal_pubkey);
        BOOST_CHECK(!qsig.IsValidSize());
    }
    
    // Test that oversized pubkeys are rejected
    {
        std::vector<unsigned char> normal_sig(100, 0xFF);
        std::vector<unsigned char> huge_pubkey(MAX_QUANTUM_PUBKEY_SIZE_DYNAMIC + 1, 0xAA);
        
        QuantumSignature qsig(SCHEME_ML_DSA_65, normal_sig, huge_pubkey);
        BOOST_CHECK(!qsig.IsValidSize());
    }
    
    // Test valid sizes
    {
        std::vector<unsigned char> valid_sig(MAX_ML_DSA_65_SIG_SIZE - 10, 0xFF);
        std::vector<unsigned char> valid_pubkey(MAX_ML_DSA_65_PUBKEY_SIZE - 10, 0xAA);
        
        QuantumSignature qsig(SCHEME_ML_DSA_65, valid_sig, valid_pubkey);
        BOOST_CHECK(qsig.IsValidSize());
    }
}

BOOST_AUTO_TEST_CASE(quantum_signature_weight_calculation)
{
    // Test ML-DSA weight calculation (gets 3x factor)
    {
        std::vector<unsigned char> sig(3000, 0xFF);
        std::vector<unsigned char> pubkey(1900, 0xAA);
        
        QuantumSignature qsig(SCHEME_ML_DSA_65, sig, pubkey);
        size_t expected_size = qsig.GetSerializedSize();
        int64_t weight = GetQuantumSignatureWeight(qsig);
        
        // ML-DSA gets 3x weight factor
        BOOST_CHECK_EQUAL(weight, expected_size * 3);
    }
    
    // Test SLH-DSA weight calculation (gets 2x factor)
    {
        std::vector<unsigned char> sig(40000, 0xFF);
        std::vector<unsigned char> pubkey(48, 0xAA);
        
        QuantumSignature qsig(SCHEME_SLH_DSA_192F, sig, pubkey);
        size_t expected_size = qsig.GetSerializedSize();
        int64_t weight = GetQuantumSignatureWeight(qsig);
        
        // SLH-DSA gets 2x weight factor
        BOOST_CHECK_EQUAL(weight, expected_size * 2);
    }
    
    // Test ECDSA weight calculation (standard 4x factor)
    {
        std::vector<unsigned char> sig(71, 0xFF);
        std::vector<unsigned char> pubkey(33, 0xAA);
        
        QuantumSignature qsig(SCHEME_ECDSA, sig, pubkey);
        size_t expected_size = qsig.GetSerializedSize();
        int64_t weight = GetQuantumSignatureWeight(qsig);
        
        // ECDSA gets standard 4x weight factor
        BOOST_CHECK_EQUAL(weight, expected_size * WITNESS_SCALE_FACTOR);
    }
}

BOOST_AUTO_TEST_CASE(quantum_transaction_weight)
{
    // Create a simple transaction with quantum signatures
    CMutableTransaction mtx;
    mtx.version = 2;
    mtx.nLockTime = 0;
    
    // Add an input (dummy for now)
    CTxIn input;
    input.prevout = COutPoint(Txid::FromHex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef").value(), 0);
    input.nSequence = CTxIn::SEQUENCE_FINAL;
    
    // For this test, we'll simulate putting a quantum signature in scriptSig
    // In practice, this would be done by the signing code
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    
    uint256 hash;
    hash.SetNull();
    
    std::vector<unsigned char> sig;
    BOOST_CHECK(key.Sign(hash, sig));
    
    CQuantumPubKey pubkey = key.GetPubKey();
    QuantumSignature qsig(SCHEME_ML_DSA_65, sig, pubkey.GetKeyData());
    
    // Encode quantum signature for scriptSig
    std::vector<unsigned char> encoded_sig = EncodeQuantumSignature(qsig);
    input.scriptSig = CScript() << encoded_sig;
    
    mtx.vin.push_back(input);
    
    // Add an output
    CTxOut output(10000, CScript() << OP_TRUE);
    mtx.vout.push_back(output);
    
    // Create transaction
    CTransaction tx(mtx);
    
    // Check transaction weight
    int32_t weight = GetTransactionWeight(tx);
    BOOST_CHECK_GT(weight, 0);
    
    // The weight should be less than MAX_STANDARD_TX_WEIGHT
    // even with a large quantum signature
    BOOST_CHECK_LE(weight, MAX_STANDARD_TX_WEIGHT);
}

BOOST_AUTO_TEST_CASE(quantum_signature_scheme_ids)
{
    // Test that scheme IDs are correctly defined
    BOOST_CHECK_EQUAL(SCHEME_ECDSA, 0x00);
    BOOST_CHECK_EQUAL(SCHEME_ML_DSA_65, 0x01);
    BOOST_CHECK_EQUAL(SCHEME_SLH_DSA_192F, 0x02);
    BOOST_CHECK_EQUAL(SCHEME_ML_DSA_87, 0x03);
    BOOST_CHECK_EQUAL(SCHEME_FALCON_512, 0x04);
    BOOST_CHECK_EQUAL(SCHEME_FALCON_1024, 0x05);
}

BOOST_AUTO_TEST_CASE(quantum_signature_varint_encoding)
{
    // Test that varint encoding works correctly for different sizes
    
    // Small signature (1 byte varint)
    {
        std::vector<unsigned char> small_sig(100, 0xFF);
        std::vector<unsigned char> small_pubkey(50, 0xAA);
        
        QuantumSignature qsig(SCHEME_ML_DSA_65, small_sig, small_pubkey);
        std::vector<unsigned char> encoded = EncodeQuantumSignature(qsig);
        
        // Scheme ID (1) + sig varint (1) + sig (100) + pubkey varint (1) + pubkey (50) = 153
        BOOST_CHECK_EQUAL(encoded.size(), 153);
    }
    
    // Large signature (3 byte varint)
    {
        std::vector<unsigned char> large_sig(10000, 0xFF);
        std::vector<unsigned char> large_pubkey(5000, 0xAA);
        
        QuantumSignature qsig(SCHEME_ML_DSA_65, large_sig, large_pubkey);
        std::vector<unsigned char> encoded = EncodeQuantumSignature(qsig);
        
        // Scheme ID (1) + sig varint (3) + sig (10000) + pubkey varint (3) + pubkey (5000) = 15007
        BOOST_CHECK_EQUAL(encoded.size(), 15007);
    }
}

BOOST_AUTO_TEST_SUITE_END()