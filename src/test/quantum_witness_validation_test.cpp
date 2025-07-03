// Test for quantum witness data validation in policy functions
#include <boost/test/unit_test.hpp>
#include <test/util/setup_common.h>
#include <policy/quantum_policy.h>
#include <script/quantum_signature.h>
#include <script/quantum_witness.h>
#include <crypto/quantum_key.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <consensus/validation.h>
#include <uint256.h>

BOOST_FIXTURE_TEST_SUITE(quantum_witness_validation_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(test_witness_quantum_signature_counting)
{
    // Create a quantum key
    quantum::CQuantumKey key;
    key.MakeNewKey(quantum::KeyType::ML_DSA_65);
    quantum::CQuantumPubKey pubkey = key.GetPubKey();
    
    // Create a transaction with P2WSH quantum input
    CMutableTransaction mtx;
    mtx.version = CTransaction::CURRENT_VERSION;
    mtx.nLockTime = 0;
    
    // Add an input
    CTxIn input;
    input.prevout = COutPoint(); // Default constructor creates null outpoint
    input.prevout.n = 0; // Set index
    input.scriptSig = CScript(); // Empty for P2WSH
    input.nSequence = CTxIn::SEQUENCE_FINAL;
    
    // Create witness stack with quantum signature
    quantum::QuantumSignature qsig;
    qsig.scheme_id = quantum::SCHEME_ML_DSA_65;
    qsig.signature = std::vector<unsigned char>(quantum::ML_DSA_65_SIG_SIZE, 0x01);
    qsig.pubkey = pubkey.GetKeyData();
    
    // Create witness script (format from descriptor)
    CScript witnessScript;
    witnessScript << std::vector<unsigned char>{quantum::SCHEME_ML_DSA_65};
    witnessScript << pubkey.GetKeyData();
    witnessScript << OP_CHECKSIG_EX;
    
    // Create witness stack
    std::vector<unsigned char> serializedSig;
    qsig.Serialize(serializedSig);
    
    input.scriptWitness.stack.push_back(serializedSig);
    input.scriptWitness.stack.push_back(std::vector<unsigned char>(witnessScript.begin(), witnessScript.end()));
    
    mtx.vin.push_back(input);
    
    // Add a dummy output
    CTxOut output;
    output.nValue = 100000;
    output.scriptPubKey = CScript() << OP_RETURN;
    mtx.vout.push_back(output);
    
    CTransaction tx(mtx);
    
    // Test that quantum signatures are detected in witness data
    BOOST_CHECK_EQUAL(quantum::CountQuantumSignatures(tx), 1);
    BOOST_CHECK(quantum::HasQuantumSignatures(tx));
    
    // Test fee adjustment works
    CAmount base_fee = 10000;
    CAmount adjusted_fee = quantum::GetQuantumAdjustedFee(base_fee, tx);
    BOOST_CHECK(adjusted_fee != base_fee); // Should be adjusted for quantum
    
    // Test with SLH-DSA signature
    CMutableTransaction mtx2;
    mtx2.version = CTransaction::CURRENT_VERSION;
    mtx2.nLockTime = 0;
    
    quantum::CQuantumKey key2;
    key2.MakeNewKey(quantum::KeyType::SLH_DSA_192F);
    quantum::CQuantumPubKey pubkey2 = key2.GetPubKey();
    
    CTxIn input2;
    input2.prevout = COutPoint(); // Default constructor creates null outpoint
    input2.prevout.n = 1; // Different index
    input2.scriptSig = CScript();
    input2.nSequence = CTxIn::SEQUENCE_FINAL;
    
    quantum::QuantumSignature qsig2;
    qsig2.scheme_id = quantum::SCHEME_SLH_DSA_192F;
    qsig2.signature = std::vector<unsigned char>(quantum::SLH_DSA_192F_SIG_SIZE, 0x02);
    qsig2.pubkey = pubkey2.GetKeyData();
    
    std::vector<unsigned char> serializedSig2;
    qsig2.Serialize(serializedSig2);
    
    input2.scriptWitness.stack.push_back(serializedSig2);
    input2.scriptWitness.stack.push_back(std::vector<unsigned char>(witnessScript.begin(), witnessScript.end()));
    
    mtx2.vin.push_back(input2);
    mtx2.vout.push_back(output);
    
    CTransaction tx2(mtx2);
    
    BOOST_CHECK_EQUAL(quantum::CountQuantumSignatures(tx2), 1);
    BOOST_CHECK(quantum::HasQuantumSignatures(tx2));
    
    // Test policy check
    std::string reason;
    BOOST_CHECK(quantum::CheckQuantumSignaturePolicy(tx, reason));
    BOOST_CHECK(quantum::CheckQuantumSignaturePolicy(tx2, reason));
}

BOOST_AUTO_TEST_CASE(test_no_witness_quantum_signatures)
{
    // Test that transactions without witness data return 0
    CMutableTransaction mtx;
    mtx.version = CTransaction::CURRENT_VERSION;
    
    CTxIn input;
    input.prevout = COutPoint(); // Default constructor creates null outpoint
    input.prevout.n = 2; // Different index
    input.scriptSig = CScript() << OP_0; // Non-quantum scriptSig
    input.nSequence = CTxIn::SEQUENCE_FINAL;
    // No witness data
    
    mtx.vin.push_back(input);
    
    CTxOut output;
    output.nValue = 50000;
    output.scriptPubKey = CScript() << OP_RETURN;
    mtx.vout.push_back(output);
    
    CTransaction tx(mtx);
    
    BOOST_CHECK_EQUAL(quantum::CountQuantumSignatures(tx), 0);
    BOOST_CHECK(!quantum::HasQuantumSignatures(tx));
    
    CAmount base_fee = 10000;
    CAmount adjusted_fee = quantum::GetQuantumAdjustedFee(base_fee, tx);
    BOOST_CHECK_EQUAL(adjusted_fee, base_fee); // No adjustment for non-quantum
}

BOOST_AUTO_TEST_CASE(test_multiple_quantum_inputs)
{
    // Test transaction with multiple quantum inputs
    CMutableTransaction mtx;
    mtx.version = CTransaction::CURRENT_VERSION;
    mtx.nLockTime = 0;
    
    // Create two ML-DSA inputs
    for (int i = 0; i < 2; i++) {
        quantum::CQuantumKey key;
        key.MakeNewKey(quantum::KeyType::ML_DSA_65);
        quantum::CQuantumPubKey pubkey = key.GetPubKey();
        
        CTxIn input;
        input.prevout = COutPoint();
        input.prevout.n = i;
        input.scriptSig = CScript(); // Empty for P2WSH
        input.nSequence = CTxIn::SEQUENCE_FINAL;
        
        quantum::QuantumSignature qsig;
        qsig.scheme_id = quantum::SCHEME_ML_DSA_65;
        qsig.signature = std::vector<unsigned char>(quantum::ML_DSA_65_SIG_SIZE, 0x01);
        qsig.pubkey = pubkey.GetKeyData();
        
        CScript witnessScript;
        witnessScript << std::vector<unsigned char>{quantum::SCHEME_ML_DSA_65};
        witnessScript << pubkey.GetKeyData();
        witnessScript << OP_CHECKSIG_EX;
        
        std::vector<unsigned char> serializedSig;
        qsig.Serialize(serializedSig);
        
        input.scriptWitness.stack.push_back(serializedSig);
        input.scriptWitness.stack.push_back(std::vector<unsigned char>(witnessScript.begin(), witnessScript.end()));
        
        mtx.vin.push_back(input);
    }
    
    // Add one SLH-DSA input
    quantum::CQuantumKey key;
    key.MakeNewKey(quantum::KeyType::SLH_DSA_192F);
    quantum::CQuantumPubKey pubkey = key.GetPubKey();
    
    CTxIn input;
    input.prevout = COutPoint();
    input.prevout.n = 2;
    input.scriptSig = CScript();
    input.nSequence = CTxIn::SEQUENCE_FINAL;
    
    quantum::QuantumSignature qsig;
    qsig.scheme_id = quantum::SCHEME_SLH_DSA_192F;
    qsig.signature = std::vector<unsigned char>(quantum::SLH_DSA_192F_SIG_SIZE, 0x02);
    qsig.pubkey = pubkey.GetKeyData();
    
    CScript witnessScript;
    witnessScript << std::vector<unsigned char>{quantum::SCHEME_SLH_DSA_192F};
    witnessScript << pubkey.GetKeyData();
    witnessScript << OP_CHECKSIG_EX;
    
    std::vector<unsigned char> serializedSig;
    qsig.Serialize(serializedSig);
    
    input.scriptWitness.stack.push_back(serializedSig);
    input.scriptWitness.stack.push_back(std::vector<unsigned char>(witnessScript.begin(), witnessScript.end()));
    
    mtx.vin.push_back(input);
    
    // Add output
    CTxOut output;
    output.nValue = 100000;
    output.scriptPubKey = CScript() << OP_RETURN;
    mtx.vout.push_back(output);
    
    CTransaction tx(mtx);
    
    // Should count 3 quantum signatures total
    BOOST_CHECK_EQUAL(quantum::CountQuantumSignatures(tx), 3);
    BOOST_CHECK(quantum::HasQuantumSignatures(tx));
    
    // Test fee adjustment with mixed signature types
    CAmount base_fee = 10000;
    CAmount adjusted_fee = quantum::GetQuantumAdjustedFee(base_fee, tx);
    
    // Fee should be adjusted based on weighted average
    // 2 ML-DSA (10% discount each) + 1 SLH-DSA (5% discount)
    // Average discount = (2 * 0.9 + 1 * 0.95) / 3 = 0.916667
    BOOST_CHECK(adjusted_fee > base_fee); // Should be higher due to quantum multiplier
    BOOST_CHECK(adjusted_fee < base_fee * quantum::QUANTUM_FEE_MULTIPLIER); // But with discount
}

BOOST_AUTO_TEST_CASE(test_mixed_quantum_and_ecdsa_inputs)
{
    // Test transaction with both quantum and ECDSA inputs
    CMutableTransaction mtx;
    mtx.version = CTransaction::CURRENT_VERSION;
    mtx.nLockTime = 0;
    
    // Add ECDSA input (no witness)
    CTxIn ecdsaInput;
    ecdsaInput.prevout = COutPoint();
    ecdsaInput.prevout.n = 0;
    ecdsaInput.scriptSig = CScript() << std::vector<unsigned char>(71, 0x30); // Dummy ECDSA sig
    ecdsaInput.nSequence = CTxIn::SEQUENCE_FINAL;
    mtx.vin.push_back(ecdsaInput);
    
    // Add quantum input
    quantum::CQuantumKey key;
    key.MakeNewKey(quantum::KeyType::ML_DSA_65);
    quantum::CQuantumPubKey pubkey = key.GetPubKey();
    
    CTxIn quantumInput;
    quantumInput.prevout = COutPoint();
    quantumInput.prevout.n = 1;
    quantumInput.scriptSig = CScript(); // Empty for P2WSH
    quantumInput.nSequence = CTxIn::SEQUENCE_FINAL;
    
    quantum::QuantumSignature qsig;
    qsig.scheme_id = quantum::SCHEME_ML_DSA_65;
    qsig.signature = std::vector<unsigned char>(quantum::ML_DSA_65_SIG_SIZE, 0x01);
    qsig.pubkey = pubkey.GetKeyData();
    
    CScript witnessScript;
    witnessScript << std::vector<unsigned char>{quantum::SCHEME_ML_DSA_65};
    witnessScript << pubkey.GetKeyData();
    witnessScript << OP_CHECKSIG_EX;
    
    std::vector<unsigned char> serializedSig;
    qsig.Serialize(serializedSig);
    
    quantumInput.scriptWitness.stack.push_back(serializedSig);
    quantumInput.scriptWitness.stack.push_back(std::vector<unsigned char>(witnessScript.begin(), witnessScript.end()));
    
    mtx.vin.push_back(quantumInput);
    
    // Add output
    CTxOut output;
    output.nValue = 100000;
    output.scriptPubKey = CScript() << OP_RETURN;
    mtx.vout.push_back(output);
    
    CTransaction tx(mtx);
    
    // Should count only 1 quantum signature
    BOOST_CHECK_EQUAL(quantum::CountQuantumSignatures(tx), 1);
    BOOST_CHECK(quantum::HasQuantumSignatures(tx));
    
    // Fee should still be adjusted for the quantum signature
    CAmount base_fee = 10000;
    CAmount adjusted_fee = quantum::GetQuantumAdjustedFee(base_fee, tx);
    BOOST_CHECK(adjusted_fee != base_fee);
}

BOOST_AUTO_TEST_CASE(test_fee_discount_calculations)
{
    // Test specific fee discount calculations
    CAmount base_fee = 10000;
    
    // Test ML-DSA only transaction
    {
        CMutableTransaction mtx;
        mtx.version = CTransaction::CURRENT_VERSION;
        
        quantum::CQuantumKey key;
        key.MakeNewKey(quantum::KeyType::ML_DSA_65);
        quantum::CQuantumPubKey pubkey = key.GetPubKey();
        
        CTxIn input;
        input.prevout = COutPoint();
        input.scriptSig = CScript();
        
        quantum::QuantumSignature qsig;
        qsig.scheme_id = quantum::SCHEME_ML_DSA_65;
        qsig.signature = std::vector<unsigned char>(quantum::ML_DSA_65_SIG_SIZE, 0x01);
        qsig.pubkey = pubkey.GetKeyData();
        
        CScript witnessScript;
        witnessScript << std::vector<unsigned char>{quantum::SCHEME_ML_DSA_65};
        witnessScript << pubkey.GetKeyData();
        witnessScript << OP_CHECKSIG_EX;
        
        std::vector<unsigned char> serializedSig;
        qsig.Serialize(serializedSig);
        
        input.scriptWitness.stack.push_back(serializedSig);
        input.scriptWitness.stack.push_back(std::vector<unsigned char>(witnessScript.begin(), witnessScript.end()));
        
        mtx.vin.push_back(input);
        
        CTxOut output;
        output.nValue = 100000;
        output.scriptPubKey = CScript() << OP_RETURN;
        mtx.vout.push_back(output);
        
        CTransaction tx(mtx);
        
        CAmount adjusted_fee = quantum::GetQuantumAdjustedFee(base_fee, tx);
        CAmount expected = static_cast<CAmount>(base_fee * quantum::QUANTUM_FEE_MULTIPLIER * quantum::ML_DSA_FEE_DISCOUNT);
        BOOST_CHECK_EQUAL(adjusted_fee, expected);
    }
    
    // Test SLH-DSA only transaction
    {
        CMutableTransaction mtx;
        mtx.version = CTransaction::CURRENT_VERSION;
        
        quantum::CQuantumKey key;
        key.MakeNewKey(quantum::KeyType::SLH_DSA_192F);
        quantum::CQuantumPubKey pubkey = key.GetPubKey();
        
        CTxIn input;
        input.prevout = COutPoint();
        input.scriptSig = CScript();
        
        quantum::QuantumSignature qsig;
        qsig.scheme_id = quantum::SCHEME_SLH_DSA_192F;
        qsig.signature = std::vector<unsigned char>(quantum::SLH_DSA_192F_SIG_SIZE, 0x02);
        qsig.pubkey = pubkey.GetKeyData();
        
        CScript witnessScript;
        witnessScript << std::vector<unsigned char>{quantum::SCHEME_SLH_DSA_192F};
        witnessScript << pubkey.GetKeyData();
        witnessScript << OP_CHECKSIG_EX;
        
        std::vector<unsigned char> serializedSig;
        qsig.Serialize(serializedSig);
        
        input.scriptWitness.stack.push_back(serializedSig);
        input.scriptWitness.stack.push_back(std::vector<unsigned char>(witnessScript.begin(), witnessScript.end()));
        
        mtx.vin.push_back(input);
        
        CTxOut output;
        output.nValue = 100000;
        output.scriptPubKey = CScript() << OP_RETURN;
        mtx.vout.push_back(output);
        
        CTransaction tx(mtx);
        
        CAmount adjusted_fee = quantum::GetQuantumAdjustedFee(base_fee, tx);
        CAmount expected = static_cast<CAmount>(base_fee * quantum::QUANTUM_FEE_MULTIPLIER * quantum::SLH_DSA_FEE_DISCOUNT);
        BOOST_CHECK_EQUAL(adjusted_fee, expected);
    }
}

BOOST_AUTO_TEST_CASE(test_witness_script_without_algorithm_prefix)
{
    // Test witness script format without algorithm ID prefix
    CMutableTransaction mtx;
    mtx.version = CTransaction::CURRENT_VERSION;
    
    quantum::CQuantumKey key;
    key.MakeNewKey(quantum::KeyType::ML_DSA_65);
    quantum::CQuantumPubKey pubkey = key.GetPubKey();
    
    CTxIn input;
    input.prevout = COutPoint();
    input.scriptSig = CScript();
    
    quantum::QuantumSignature qsig;
    qsig.scheme_id = quantum::SCHEME_ML_DSA_65;
    qsig.signature = std::vector<unsigned char>(quantum::ML_DSA_65_SIG_SIZE, 0x01);
    qsig.pubkey = pubkey.GetKeyData();
    
    // Create witness script WITHOUT algorithm prefix (direct pubkey format)
    CScript witnessScript;
    witnessScript << pubkey.GetKeyData();
    witnessScript << OP_CHECKSIG_EX;
    
    std::vector<unsigned char> serializedSig;
    qsig.Serialize(serializedSig);
    
    input.scriptWitness.stack.push_back(serializedSig);
    input.scriptWitness.stack.push_back(std::vector<unsigned char>(witnessScript.begin(), witnessScript.end()));
    
    mtx.vin.push_back(input);
    
    CTxOut output;
    output.nValue = 100000;
    output.scriptPubKey = CScript() << OP_RETURN;
    mtx.vout.push_back(output);
    
    CTransaction tx(mtx);
    
    // Should still count the quantum signature
    BOOST_CHECK_EQUAL(quantum::CountQuantumSignatures(tx), 1);
    BOOST_CHECK(quantum::HasQuantumSignatures(tx));
}

BOOST_AUTO_TEST_CASE(test_invalid_witness_stack_sizes)
{
    // Test with empty witness stack
    {
        CMutableTransaction mtx;
        mtx.version = CTransaction::CURRENT_VERSION;
        
        CTxIn input;
        input.prevout = COutPoint();
        input.scriptSig = CScript();
        // Empty witness stack
        
        mtx.vin.push_back(input);
        
        CTxOut output;
        output.nValue = 100000;
        output.scriptPubKey = CScript() << OP_RETURN;
        mtx.vout.push_back(output);
        
        CTransaction tx(mtx);
        
        BOOST_CHECK_EQUAL(quantum::CountQuantumSignatures(tx), 0);
        BOOST_CHECK(!quantum::HasQuantumSignatures(tx));
    }
    
    // Test with single-element witness stack (missing witness script)
    {
        CMutableTransaction mtx;
        mtx.version = CTransaction::CURRENT_VERSION;
        
        CTxIn input;
        input.prevout = COutPoint();
        input.scriptSig = CScript();
        
        quantum::QuantumSignature qsig;
        qsig.scheme_id = quantum::SCHEME_ML_DSA_65;
        qsig.signature = std::vector<unsigned char>(quantum::ML_DSA_65_SIG_SIZE, 0x01);
        qsig.pubkey = std::vector<unsigned char>(quantum::ML_DSA_65_PUBKEY_SIZE, 0x02);
        
        std::vector<unsigned char> serializedSig;
        qsig.Serialize(serializedSig);
        
        // Only add signature, no witness script
        input.scriptWitness.stack.push_back(serializedSig);
        
        mtx.vin.push_back(input);
        
        CTxOut output;
        output.nValue = 100000;
        output.scriptPubKey = CScript() << OP_RETURN;
        mtx.vout.push_back(output);
        
        CTransaction tx(mtx);
        
        // Should not count as quantum signature (invalid witness stack)
        BOOST_CHECK_EQUAL(quantum::CountQuantumSignatures(tx), 0);
        BOOST_CHECK(!quantum::HasQuantumSignatures(tx));
    }
}

BOOST_AUTO_TEST_CASE(test_quantum_policy_limits)
{
    // Test transaction with too many quantum signatures
    CMutableTransaction mtx;
    mtx.version = CTransaction::CURRENT_VERSION;
    
    // Create more inputs than MAX_STANDARD_QUANTUM_SIGS
    for (size_t i = 0; i < quantum::MAX_STANDARD_QUANTUM_SIGS + 1; i++) {
        quantum::CQuantumKey key;
        key.MakeNewKey(quantum::KeyType::ML_DSA_65);
        quantum::CQuantumPubKey pubkey = key.GetPubKey();
        
        CTxIn input;
        input.prevout = COutPoint();
        input.prevout.n = i;
        input.scriptSig = CScript();
        
        quantum::QuantumSignature qsig;
        qsig.scheme_id = quantum::SCHEME_ML_DSA_65;
        qsig.signature = std::vector<unsigned char>(quantum::ML_DSA_65_SIG_SIZE, 0x01);
        qsig.pubkey = pubkey.GetKeyData();
        
        CScript witnessScript;
        witnessScript << std::vector<unsigned char>{quantum::SCHEME_ML_DSA_65};
        witnessScript << pubkey.GetKeyData();
        witnessScript << OP_CHECKSIG_EX;
        
        std::vector<unsigned char> serializedSig;
        qsig.Serialize(serializedSig);
        
        input.scriptWitness.stack.push_back(serializedSig);
        input.scriptWitness.stack.push_back(std::vector<unsigned char>(witnessScript.begin(), witnessScript.end()));
        
        mtx.vin.push_back(input);
    }
    
    CTxOut output;
    output.nValue = 100000;
    output.scriptPubKey = CScript() << OP_RETURN;
    mtx.vout.push_back(output);
    
    CTransaction tx(mtx);
    
    // Should fail policy check due to too many quantum signatures
    std::string reason;
    BOOST_CHECK(!quantum::CheckQuantumSignaturePolicy(tx, reason));
    BOOST_CHECK(reason == "too many quantum signatures");
}

BOOST_AUTO_TEST_SUITE_END()