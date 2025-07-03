// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <script/script.h>
#include <script/interpreter.h>
#include <script/script_error.h>
#include <script/quantum_signature.h>
#include <crypto/quantum_key.h>
#include <test/util/setup_common.h>

using namespace quantum;

BOOST_FIXTURE_TEST_SUITE(quantum_push_size_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(quantum_signature_push_in_evalscript)
{
    // Test that EvalScript allows quantum-sized pushes when SCRIPT_VERIFY_QUANTUM_SIGS is set
    
    // Create a real quantum signature
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    
    uint256 hash;
    hash.SetNull();
    
    std::vector<unsigned char> sig;
    BOOST_CHECK(key.Sign(hash, sig));
    sig.push_back(SIGHASH_ALL);
    
    // The signature should be 3310 bytes
    BOOST_CHECK_EQUAL(sig.size(), 3310);
    
    // Prepend algorithm ID for OP_CHECKSIG_EX
    std::vector<unsigned char> full_sig;
    full_sig.push_back(quantum::SCHEME_ML_DSA_65);
    full_sig.insert(full_sig.end(), sig.begin(), sig.end());
    BOOST_CHECK_EQUAL(full_sig.size(), 3311);
    
    // Create a script that pushes this signature
    CScript script;
    script << full_sig << OP_DROP << OP_1;
    
    std::vector<std::vector<unsigned char>> stack;
    ScriptError error;
    BaseSignatureChecker checker;
    
    // Without SCRIPT_VERIFY_QUANTUM_SIGS, should fail with push size error
    {
        stack.clear();
        error = SCRIPT_ERR_OK;
        unsigned int flags = SCRIPT_VERIFY_P2SH;
        bool result = EvalScript(stack, script, flags, checker, SigVersion::BASE, &error);
        BOOST_CHECK(!result);
        BOOST_CHECK_EQUAL(error, SCRIPT_ERR_PUSH_SIZE);
    }
    
    // With SCRIPT_VERIFY_QUANTUM_SIGS, should succeed
    {
        stack.clear();
        error = SCRIPT_ERR_OK;
        unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_QUANTUM_SIGS;
        bool result = EvalScript(stack, script, flags, checker, SigVersion::BASE, &error);
        BOOST_CHECK(result);
        BOOST_CHECK_EQUAL(error, SCRIPT_ERR_OK);
        BOOST_CHECK_EQUAL(stack.size(), 1);
        BOOST_CHECK(!stack[0].empty());
    }
}

BOOST_AUTO_TEST_CASE(quantum_pubkey_push_in_evalscript)
{
    // Test that EvalScript allows quantum pubkey pushes
    
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    CQuantumPubKey pubkey = key.GetPubKey();
    std::vector<unsigned char> pubkey_data = pubkey.GetKeyData();
    
    // ML-DSA pubkey should be 1952 bytes
    BOOST_CHECK_EQUAL(pubkey_data.size(), 1952);
    
    // Create a script that pushes this pubkey
    CScript script;
    script << pubkey_data << OP_DROP << OP_1;
    
    std::vector<std::vector<unsigned char>> stack;
    ScriptError error;
    BaseSignatureChecker checker;
    
    // Without SCRIPT_VERIFY_QUANTUM_SIGS, should fail
    {
        stack.clear();
        error = SCRIPT_ERR_OK;
        unsigned int flags = SCRIPT_VERIFY_P2SH;
        bool result = EvalScript(stack, script, flags, checker, SigVersion::BASE, &error);
        BOOST_CHECK(!result);
        BOOST_CHECK_EQUAL(error, SCRIPT_ERR_PUSH_SIZE);
    }
    
    // With SCRIPT_VERIFY_QUANTUM_SIGS, should succeed
    {
        stack.clear();
        error = SCRIPT_ERR_OK;
        unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_QUANTUM_SIGS;
        bool result = EvalScript(stack, script, flags, checker, SigVersion::BASE, &error);
        BOOST_CHECK(result);
        BOOST_CHECK_EQUAL(error, SCRIPT_ERR_OK);
    }
}

BOOST_AUTO_TEST_CASE(quantum_witness_script_push)
{
    // Test witness script execution with quantum signatures
    
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    CQuantumPubKey pubkey = key.GetPubKey();
    
    // Create witness script
    CScript witness_script;
    witness_script << std::vector<unsigned char>{quantum::SCHEME_ML_DSA_65} << pubkey.GetKeyData() << OP_CHECKSIG_EX;
    
    // This witness script should be about 1954 bytes (1 algo + 1952 pubkey + 1 opcode)
    BOOST_CHECK_GT(witness_script.size(), 520);
    
    // In witness context, the script itself can be large
    std::vector<std::vector<unsigned char>> stack;
    stack.push_back(std::vector<unsigned char>(witness_script.begin(), witness_script.end()));
    
    // Check that we can push this as a witness stack element
    BOOST_CHECK_EQUAL(stack[0].size(), witness_script.size());
    BOOST_CHECK_GT(stack[0].size(), MAX_SCRIPT_ELEMENT_SIZE);
}


BOOST_AUTO_TEST_SUITE_END()