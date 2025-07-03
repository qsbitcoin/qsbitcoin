// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <script/quantum_signature.h>
#include <script/quantum_witness.h>
#include <script/interpreter.h>
#include <script/sign.h>
#include <crypto/quantum_key.h>
#include <crypto/sha256.h>
#include <primitives/transaction.h>
#include <consensus/validation.h>
#include <policy/policy.h>
#include <test/util/setup_common.h>
#include <test/util/logging.h>
#include <script/quantum_sigchecker.h>

using namespace quantum;

// Helper function to create a quantum signature with algorithm ID prepended
static std::vector<unsigned char> CreateQuantumSignatureWithAlgoId(const CQuantumKey& key, const uint256& hash, int hashtype)
{
    std::vector<unsigned char> sig;
    if (!key.Sign(hash, sig)) {
        return {};
    }
    
    // Prepend algorithm ID
    std::vector<unsigned char> full_sig;
    full_sig.push_back(key.GetType() == KeyType::ML_DSA_65 ? SCHEME_ML_DSA_65 : SCHEME_SLH_DSA_192F);
    full_sig.insert(full_sig.end(), sig.begin(), sig.end());
    full_sig.push_back((unsigned char)hashtype);
    
    return full_sig;
}

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
        std::vector<unsigned char> valid_sig(ML_DSA_65_SIG_SIZE, 0xFF);
        std::vector<unsigned char> valid_pubkey(ML_DSA_65_PUBKEY_SIZE, 0xAA);
        
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
    BOOST_CHECK_EQUAL(SCHEME_ECDSA, 0x01);
    BOOST_CHECK_EQUAL(SCHEME_ML_DSA_65, 0x02);
    BOOST_CHECK_EQUAL(SCHEME_SLH_DSA_192F, 0x03);
    BOOST_CHECK_EQUAL(SCHEME_ML_DSA_87, 0x04);
    BOOST_CHECK_EQUAL(SCHEME_FALCON_512, 0x05);
    BOOST_CHECK_EQUAL(SCHEME_FALCON_1024, 0x06);
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

BOOST_AUTO_TEST_CASE(quantum_direct_signature_check)
{
    // Test direct quantum signature verification first
    {
        CQuantumKey key;
        key.MakeNewKey(KeyType::ML_DSA_65);
        CQuantumPubKey pubkey = key.GetPubKey();
        
        // Create witness script first
        CScript witness_script;
        witness_script << std::vector<unsigned char>{quantum::SCHEME_ML_DSA_65} << pubkey.GetKeyData() << OP_CHECKSIG_EX;
        
        // Create transaction with proper inputs
        CMutableTransaction mtx;
        mtx.version = 2;  // Witness transactions should be version 2
        mtx.vin.resize(1);
        mtx.vin[0].prevout.hash = Txid::FromHex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef").value();
        mtx.vin[0].prevout.n = 0;
        mtx.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
        mtx.vout.resize(1);
        mtx.vout[0].nValue = 100000000;
        
        // Compute the correct signature hash for the transaction
        uint256 sighash = SignatureHash(witness_script, mtx, 0, SIGHASH_ALL, 100000000, SigVersion::WITNESS_V0);
        
        // Sign the transaction hash
        std::vector<unsigned char> sig;
        BOOST_CHECK(key.Sign(sighash, sig));
        
        // Verify directly
        BOOST_CHECK(CQuantumKey::Verify(sighash, sig, pubkey));
        
        // Now test with signature checker
        // For witness transactions, we need PrecomputedTransactionData
        CTransaction tx(mtx);
        PrecomputedTransactionData txdata(tx);
        QuantumTransactionSignatureChecker<CTransaction> checker(&tx, 0, 100000000, txdata, MissingDataBehavior::FAIL);
        
        // Create full signature with algorithm ID for OP_CHECKSIG_EX
        std::vector<unsigned char> full_sig = CreateQuantumSignatureWithAlgoId(key, sighash, SIGHASH_ALL);
        BOOST_CHECK(!full_sig.empty());
        
        // Test CheckQuantumSignature directly
        // First, let's verify our inputs are correct
        BOOST_CHECK_MESSAGE(full_sig.size() > 0, "Signature is empty");
        BOOST_CHECK_MESSAGE(full_sig.back() == SIGHASH_ALL, "Signature doesn't end with SIGHASH_ALL");
        BOOST_CHECK_MESSAGE(pubkey.GetKeyData().size() == MAX_ML_DSA_65_PUBKEY_SIZE, 
                          "Pubkey size is " + std::to_string(pubkey.GetKeyData().size()) + 
                          " expected " + std::to_string(MAX_ML_DSA_65_PUBKEY_SIZE));
        
        bool result = checker.CheckQuantumSignature(full_sig, pubkey.GetKeyData(), witness_script, 
                                                   SigVersion::WITNESS_V0, quantum::SCHEME_ML_DSA_65);
        BOOST_CHECK_MESSAGE(result, "Direct CheckQuantumSignature failed");
    }
}

BOOST_AUTO_TEST_CASE(quantum_witness_script_execution)
{
    // Test that quantum signatures work in witness scripts with proper soft fork flags
    
    // Test ML-DSA witness script
    {
        CQuantumKey key;
        key.MakeNewKey(KeyType::ML_DSA_65);
        CQuantumPubKey pubkey = key.GetPubKey();
        
        // Create witness script: <pubkey> OP_CHECKSIG_EX
        CScript witness_script;
        witness_script << pubkey.GetKeyData() << OP_CHECKSIG_EX;
        
        // Create P2WSH scriptPubKey
        uint256 script_hash;
        CSHA256().Write(witness_script.data(), witness_script.size()).Finalize(script_hash.begin());
        CScript scriptPubKey;
        scriptPubKey << OP_0 << std::vector<unsigned char>(script_hash.begin(), script_hash.end());
        
        // Create a dummy transaction to sign with proper inputs
        CMutableTransaction mtx;
        mtx.version = 2;  // Witness transactions should be version 2
        mtx.vin.resize(1);
        mtx.vin[0].prevout.hash = Txid::FromHex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef").value();
        mtx.vin[0].prevout.n = 0;
        mtx.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
        mtx.vout.resize(1);
        mtx.vout[0].nValue = 100000000; // 1 BTC
        
        // Create signature
        uint256 hash = SignatureHash(witness_script, mtx, 0, SIGHASH_ALL, 100000000, SigVersion::WITNESS_V0);
        std::vector<unsigned char> sig = CreateQuantumSignatureWithAlgoId(key, hash, SIGHASH_ALL);
        BOOST_CHECK(!sig.empty());
        
        // Build witness stack
        CScriptWitness witness;
        witness.stack.push_back(sig);
        witness.stack.push_back(std::vector<unsigned char>(witness_script.begin(), witness_script.end()));
        
        // Verify witness script execution passes with quantum flags
        CTransaction tx(mtx);
        PrecomputedTransactionData txdata(tx);
        QuantumTransactionSignatureChecker<CTransaction> checker(&tx, 0, 100000000, txdata, MissingDataBehavior::FAIL);
        ScriptError error = SCRIPT_ERR_OK;
        
        // Should pass with SCRIPT_VERIFY_QUANTUM_SIGS
        unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS | SCRIPT_VERIFY_QUANTUM_SIGS;
        BOOST_CHECK(VerifyScript(CScript(), scriptPubKey, &witness, flags, checker, &error));
        BOOST_CHECK_EQUAL(error, SCRIPT_ERR_OK);
        
        // Should fail without SCRIPT_VERIFY_QUANTUM_SIGS due to push size
        flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS;
        BOOST_CHECK(!VerifyScript(CScript(), scriptPubKey, &witness, flags, checker, &error));
        BOOST_CHECK_EQUAL(error, SCRIPT_ERR_PUSH_SIZE);
    }
    
    // Test SLH-DSA witness script
    {
        CQuantumKey key;
        key.MakeNewKey(KeyType::SLH_DSA_192F);
        CQuantumPubKey pubkey = key.GetPubKey();
        
        // Create witness script: <pubkey> OP_CHECKSIG_EX
        CScript witness_script;
        witness_script << pubkey.GetKeyData() << OP_CHECKSIG_EX;
        
        // Create P2WSH scriptPubKey
        uint256 script_hash;
        CSHA256().Write(witness_script.data(), witness_script.size()).Finalize(script_hash.begin());
        CScript scriptPubKey;
        scriptPubKey << OP_0 << std::vector<unsigned char>(script_hash.begin(), script_hash.end());
        
        // Create a dummy transaction to sign with proper inputs
        CMutableTransaction mtx;
        mtx.version = 2;  // Witness transactions should be version 2
        mtx.vin.resize(1);
        mtx.vin[0].prevout.hash = Txid::FromHex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef").value();
        mtx.vin[0].prevout.n = 0;
        mtx.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
        mtx.vout.resize(1);
        mtx.vout[0].nValue = 100000000; // 1 BTC
        
        // Create signature
        uint256 hash = SignatureHash(witness_script, mtx, 0, SIGHASH_ALL, 100000000, SigVersion::WITNESS_V0);
        std::vector<unsigned char> sig = CreateQuantumSignatureWithAlgoId(key, hash, SIGHASH_ALL);
        BOOST_CHECK(!sig.empty());
        
        // Build witness stack
        CScriptWitness witness;
        witness.stack.push_back(sig);
        witness.stack.push_back(std::vector<unsigned char>(witness_script.begin(), witness_script.end()));
        
        // Verify witness script execution passes with quantum flags
        CTransaction tx(mtx);
        PrecomputedTransactionData txdata(tx);
        QuantumTransactionSignatureChecker<CTransaction> checker(&tx, 0, 100000000, txdata, MissingDataBehavior::FAIL);
        ScriptError error = SCRIPT_ERR_OK;
        
        // Should pass with SCRIPT_VERIFY_QUANTUM_SIGS
        unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS | SCRIPT_VERIFY_QUANTUM_SIGS;
        BOOST_CHECK(VerifyScript(CScript(), scriptPubKey, &witness, flags, checker, &error));
        BOOST_CHECK_EQUAL(error, SCRIPT_ERR_OK);
        
        // Should fail without SCRIPT_VERIFY_QUANTUM_SIGS due to push size (SLH-DSA sig is ~35KB)
        flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS;
        BOOST_CHECK(!VerifyScript(CScript(), scriptPubKey, &witness, flags, checker, &error));
        BOOST_CHECK_EQUAL(error, SCRIPT_ERR_PUSH_SIZE);
    }
}

BOOST_AUTO_TEST_SUITE_END()