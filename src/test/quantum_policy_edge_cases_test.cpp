// Test edge cases for quantum policy functions
#include <boost/test/unit_test.hpp>
#include <test/util/setup_common.h>
#include <policy/quantum_policy.h>
#include <script/quantum_signature.h>
#include <script/quantum_witness.h>
#include <crypto/quantum_key.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <consensus/validation.h>

BOOST_FIXTURE_TEST_SUITE(quantum_policy_edge_cases_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(test_malformed_quantum_signatures)
{
    // Test with signatures that look quantum-sized but aren't valid
    CMutableTransaction mtx;
    mtx.version = CTransaction::CURRENT_VERSION;
    
    CTxIn input;
    input.prevout = COutPoint();
    input.scriptSig = CScript();
    
    // Create a large blob that's not a valid quantum signature
    std::vector<unsigned char> fakeSig(quantum::MIN_QUANTUM_SIG_SIZE_THRESHOLD + 50, 0xFF);
    
    // Add to witness stack
    input.scriptWitness.stack.push_back(fakeSig);
    input.scriptWitness.stack.push_back(std::vector<unsigned char>{0x01, 0x02, 0x03}); // Fake witness script
    
    mtx.vin.push_back(input);
    
    CTxOut output;
    output.nValue = 100000;
    output.scriptPubKey = CScript() << OP_RETURN;
    mtx.vout.push_back(output);
    
    CTransaction tx(mtx);
    
    // Should not count as quantum signature (invalid format)
    BOOST_CHECK_EQUAL(quantum::CountQuantumSignatures(tx), 0);
    BOOST_CHECK(!quantum::HasQuantumSignatures(tx));
}

BOOST_AUTO_TEST_CASE(test_quantum_signature_in_wrong_position)
{
    // Test quantum signature placed in witness script position
    CMutableTransaction mtx;
    mtx.version = CTransaction::CURRENT_VERSION;
    
    CTxIn input;
    input.prevout = COutPoint();
    input.scriptSig = CScript();
    
    quantum::CQuantumKey key;
    key.MakeNewKey(quantum::KeyType::ML_DSA_65);
    quantum::CQuantumPubKey pubkey = key.GetPubKey();
    
    quantum::QuantumSignature qsig;
    qsig.scheme_id = quantum::SCHEME_ML_DSA_65;
    qsig.signature = std::vector<unsigned char>(quantum::ML_DSA_65_SIG_SIZE, 0x01);
    qsig.pubkey = pubkey.GetKeyData();
    
    std::vector<unsigned char> serializedSig;
    qsig.Serialize(serializedSig);
    
    // Put quantum signature in wrong position (as witness script)
    input.scriptWitness.stack.push_back(std::vector<unsigned char>{0x00}); // Dummy first element
    input.scriptWitness.stack.push_back(serializedSig); // Quantum sig as witness script
    
    mtx.vin.push_back(input);
    
    CTxOut output;
    output.nValue = 100000;
    output.scriptPubKey = CScript() << OP_RETURN;
    mtx.vout.push_back(output);
    
    CTransaction tx(mtx);
    
    // Should not count the misplaced quantum signature
    BOOST_CHECK_EQUAL(quantum::CountQuantumSignatures(tx), 0);
}

BOOST_AUTO_TEST_CASE(test_empty_transaction)
{
    // Test completely empty transaction
    CMutableTransaction mtx;
    CTransaction tx(mtx);
    
    BOOST_CHECK_EQUAL(quantum::CountQuantumSignatures(tx), 0);
    BOOST_CHECK(!quantum::HasQuantumSignatures(tx));
    
    
    std::string reason;
    BOOST_CHECK(quantum::CheckQuantumSignaturePolicy(tx, reason));
}

BOOST_AUTO_TEST_CASE(test_scriptSig_quantum_signature)
{
    // Test quantum signature in scriptSig (legacy path)
    CMutableTransaction mtx;
    mtx.version = CTransaction::CURRENT_VERSION;
    
    quantum::CQuantumKey key;
    key.MakeNewKey(quantum::KeyType::ML_DSA_65);
    quantum::CQuantumPubKey pubkey = key.GetPubKey();
    
    CTxIn input;
    input.prevout = COutPoint();
    
    // Create quantum signature and put it in scriptSig
    quantum::QuantumSignature qsig;
    qsig.scheme_id = quantum::SCHEME_ML_DSA_65;
    qsig.signature = std::vector<unsigned char>(quantum::ML_DSA_65_SIG_SIZE, 0x01);
    qsig.pubkey = pubkey.GetKeyData();
    
    std::vector<unsigned char> serializedSig;
    qsig.Serialize(serializedSig);
    
    input.scriptSig = CScript() << serializedSig;
    input.nSequence = CTxIn::SEQUENCE_FINAL;
    
    mtx.vin.push_back(input);
    
    CTxOut output;
    output.nValue = 100000;
    output.scriptPubKey = CScript() << OP_RETURN;
    mtx.vout.push_back(output);
    
    CTransaction tx(mtx);
    
    // Should count quantum signature in scriptSig
    BOOST_CHECK_EQUAL(quantum::CountQuantumSignatures(tx), 1);
    BOOST_CHECK(quantum::HasQuantumSignatures(tx));
}

BOOST_AUTO_TEST_CASE(test_mixed_witness_stack_sizes)
{
    // Test transaction with inputs having different witness stack sizes
    CMutableTransaction mtx;
    mtx.version = CTransaction::CURRENT_VERSION;
    
    // Input 1: Proper quantum witness
    {
        quantum::CQuantumKey key;
        key.MakeNewKey(quantum::KeyType::ML_DSA_65);
        quantum::CQuantumPubKey pubkey = key.GetPubKey();
        
        CTxIn input;
        input.prevout = COutPoint();
        input.prevout.n = 0;
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
    
    // Input 2: Multi-element witness stack (e.g., multisig-like)
    {
        CTxIn input;
        input.prevout = COutPoint();
        input.prevout.n = 1;
        input.scriptSig = CScript();
        
        // Add multiple elements to witness stack
        input.scriptWitness.stack.push_back(std::vector<unsigned char>{0x00}); // OP_FALSE
        input.scriptWitness.stack.push_back(std::vector<unsigned char>(71, 0x30)); // ECDSA sig 1
        input.scriptWitness.stack.push_back(std::vector<unsigned char>(71, 0x30)); // ECDSA sig 2
        input.scriptWitness.stack.push_back(std::vector<unsigned char>{0x51, 0x52}); // Witness script
        
        mtx.vin.push_back(input);
    }
    
    // Input 3: Single element witness (invalid)
    {
        CTxIn input;
        input.prevout = COutPoint();
        input.prevout.n = 2;
        input.scriptSig = CScript();
        
        input.scriptWitness.stack.push_back(std::vector<unsigned char>(100, 0xAA));
        
        mtx.vin.push_back(input);
    }
    
    CTxOut output;
    output.nValue = 100000;
    output.scriptPubKey = CScript() << OP_RETURN;
    mtx.vout.push_back(output);
    
    CTransaction tx(mtx);
    
    // Should count only the first input's quantum signature
    BOOST_CHECK_EQUAL(quantum::CountQuantumSignatures(tx), 1);
}

BOOST_AUTO_TEST_CASE(test_zero_value_fee_calculation)
{
    // Test fee calculation with zero base fee
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
    
}

BOOST_AUTO_TEST_CASE(test_invalid_signature_scheme_ids)
{
    // Test with invalid scheme IDs
    CMutableTransaction mtx;
    mtx.version = CTransaction::CURRENT_VERSION;
    
    CTxIn input;
    input.prevout = COutPoint();
    input.scriptSig = CScript();
    
    // Create a signature with invalid scheme ID
    quantum::QuantumSignature qsig;
    qsig.scheme_id = static_cast<quantum::SignatureSchemeID>(0xFF); // Invalid scheme ID
    qsig.signature = std::vector<unsigned char>(1000, 0x01); // Large enough to trigger size check
    qsig.pubkey = std::vector<unsigned char>(100, 0x02);
    
    std::vector<unsigned char> serializedSig;
    qsig.Serialize(serializedSig);
    
    input.scriptWitness.stack.push_back(serializedSig);
    input.scriptWitness.stack.push_back(std::vector<unsigned char>{0x01, 0x02, 0x03});
    
    mtx.vin.push_back(input);
    
    CTxOut output;
    output.nValue = 100000;
    output.scriptPubKey = CScript() << OP_RETURN;
    mtx.vout.push_back(output);
    
    CTransaction tx(mtx);
    
    // Should not count signatures with invalid scheme IDs
    BOOST_CHECK_EQUAL(quantum::CountQuantumSignatures(tx), 0);
    BOOST_CHECK(!quantum::HasQuantumSignatures(tx));
}

BOOST_AUTO_TEST_SUITE_END()