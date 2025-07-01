// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <script/script.h>
#include <script/interpreter.h>
#include <script/script_error.h>
#include <script/sign.h>
#include <script/signingprovider.h>
#include <script/quantum_witness.h>
#include <crypto/quantum_key.h>
#include <key.h>
#include <pubkey.h>
#include <primitives/transaction.h>
#include <policy/policy.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <crypto/sha256.h>

#include <vector>

using namespace quantum;

// CastToBool is declared in interpreter.cpp, forward declare it here
extern bool CastToBool(const std::vector<unsigned char>& vch);

BOOST_FIXTURE_TEST_SUITE(quantum_softfork_tests, BasicTestingSetup)

// Helper function to create a dummy quantum signature of specified size
std::vector<unsigned char> CreateDummyQuantumSignature(size_t total_size, uint8_t scheme_id)
{
    std::vector<unsigned char> sig;
    
    // Reserve space to ensure we hit the exact size
    sig.reserve(total_size);
    
    // Fill with dummy data to reach the exact size
    sig.resize(total_size, 0x01);
    
    // Set the last byte as sighash type
    if (!sig.empty()) {
        sig.back() = SIGHASH_ALL;
    }
    
    return sig;
}

BOOST_AUTO_TEST_CASE(quantum_push_size_limit_witness_v0)
{
    // Test that quantum signatures can exceed 520-byte limit in witness scripts
    
    // Create ML-DSA signature (3310 bytes with sighash)
    std::vector<unsigned char> ml_dsa_sig = CreateDummyQuantumSignature(3310, SCHEME_ML_DSA_65);
    BOOST_CHECK_EQUAL(ml_dsa_sig.size(), 3310);
    
    // Create SLH-DSA signature (35665 bytes with sighash)
    std::vector<unsigned char> slh_dsa_sig = CreateDummyQuantumSignature(35665, SCHEME_SLH_DSA_192F);
    BOOST_CHECK_EQUAL(slh_dsa_sig.size(), 35665);
    
    // Create quantum pubkeys
    std::vector<unsigned char> ml_dsa_pubkey(1952, 0x02); // ML-DSA pubkey
    std::vector<unsigned char> slh_dsa_pubkey(48, 0x03);   // SLH-DSA pubkey
    
    // Test 1: ML-DSA witness script execution
    {
        CScript witness_script;
        witness_script << ml_dsa_pubkey << OP_CHECKSIG_ML_DSA;
        
        std::vector<std::vector<unsigned char>> witness_stack;
        witness_stack.push_back(ml_dsa_sig);
        witness_stack.push_back(std::vector<unsigned char>(witness_script.begin(), witness_script.end()));
        
        // Create a dummy transaction for the checker
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vout.resize(1);
        PrecomputedTransactionData txdata(mtx);
        
        // Without SCRIPT_VERIFY_QUANTUM_SIGS, should fail due to push size
        {
            MutableTransactionSignatureChecker checker(&mtx, 0, 0, txdata, MissingDataBehavior::FAIL);
            // Create proper witness program hash from witness script
            uint256 script_hash;
            CSHA256().Write(witness_script.data(), witness_script.size()).Finalize(script_hash.begin());
            std::vector<unsigned char> witness_program_vec(script_hash.begin(), script_hash.end());
            
            ScriptError error = SCRIPT_ERR_OK;
            unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS;
            
            CScriptWitness scriptWitness;
            scriptWitness.stack = witness_stack;
            
            BOOST_CHECK(!VerifyScript(CScript(), CScript() << OP_0 << witness_program_vec, 
                                    &scriptWitness, flags, checker, &error));
            BOOST_CHECK_EQUAL(error, SCRIPT_ERR_PUSH_SIZE);
        }
        
        // With SCRIPT_VERIFY_QUANTUM_SIGS, should pass push size check (but fail sig verification)
        {
            MutableTransactionSignatureChecker checker(&mtx, 0, 0, txdata, MissingDataBehavior::FAIL);
            // Create proper witness program hash from witness script
            uint256 script_hash;
            CSHA256().Write(witness_script.data(), witness_script.size()).Finalize(script_hash.begin());
            std::vector<unsigned char> witness_program_vec(script_hash.begin(), script_hash.end());
            ScriptError error = SCRIPT_ERR_OK;
            unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS | SCRIPT_VERIFY_QUANTUM_SIGS;
            
            CScriptWitness scriptWitness;
            scriptWitness.stack = witness_stack;
            
            // Will fail because we're using dummy signatures, but should get past push size check
            BOOST_CHECK(!VerifyScript(CScript(), CScript() << OP_0 << witness_program_vec, 
                                    &scriptWitness, flags, checker, &error));
            // The error should NOT be SCRIPT_ERR_PUSH_SIZE when quantum flags are set
            // It might fail with other errors like SCRIPT_ERR_EVAL_FALSE or SCRIPT_ERR_QUANTUM_SIG_VERIFICATION
            BOOST_CHECK_MESSAGE(error != SCRIPT_ERR_PUSH_SIZE, 
                              "Got SCRIPT_ERR_PUSH_SIZE even with SCRIPT_VERIFY_QUANTUM_SIGS flag set. Error code: " + std::to_string(error));
        }
    }
    
    // Test 2: SLH-DSA witness script execution
    {
        CScript witness_script;
        witness_script << slh_dsa_pubkey << OP_CHECKSIG_SLH_DSA;
        
        std::vector<std::vector<unsigned char>> witness_stack;
        witness_stack.push_back(slh_dsa_sig);
        witness_stack.push_back(std::vector<unsigned char>(witness_script.begin(), witness_script.end()));
        
        // Create a dummy transaction for the checker
        CMutableTransaction mtx;
        mtx.vin.resize(1);
        mtx.vout.resize(1);
        PrecomputedTransactionData txdata(mtx);
        
        // Without SCRIPT_VERIFY_QUANTUM_SIGS, should fail due to push size
        {
            MutableTransactionSignatureChecker checker(&mtx, 0, 0, txdata, MissingDataBehavior::FAIL);
            // Create proper witness program hash from witness script
            uint256 script_hash;
            CSHA256().Write(witness_script.data(), witness_script.size()).Finalize(script_hash.begin());
            std::vector<unsigned char> witness_program_vec(script_hash.begin(), script_hash.end());
            
            ScriptError error = SCRIPT_ERR_OK;
            unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS;
            
            CScriptWitness scriptWitness;
            scriptWitness.stack = witness_stack;
            
            BOOST_CHECK(!VerifyScript(CScript(), CScript() << OP_0 << witness_program_vec, 
                                    &scriptWitness, flags, checker, &error));
            BOOST_CHECK_EQUAL(error, SCRIPT_ERR_PUSH_SIZE);
        }
        
        // With SCRIPT_VERIFY_QUANTUM_SIGS, should pass push size check
        {
            MutableTransactionSignatureChecker checker(&mtx, 0, 0, txdata, MissingDataBehavior::FAIL);
            // Create proper witness program hash from witness script
            uint256 script_hash;
            CSHA256().Write(witness_script.data(), witness_script.size()).Finalize(script_hash.begin());
            std::vector<unsigned char> witness_program_vec(script_hash.begin(), script_hash.end());
            ScriptError error = SCRIPT_ERR_OK;
            unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS | SCRIPT_VERIFY_QUANTUM_SIGS;
            
            CScriptWitness scriptWitness;
            scriptWitness.stack = witness_stack;
            
            // Will fail because we're using dummy signatures, but should get past push size check
            BOOST_CHECK(!VerifyScript(CScript(), CScript() << OP_0 << witness_program_vec, 
                                    &scriptWitness, flags, checker, &error));
            // The error should NOT be SCRIPT_ERR_PUSH_SIZE when quantum flags are set
            // It might fail with other errors like SCRIPT_ERR_EVAL_FALSE or SCRIPT_ERR_QUANTUM_SIG_VERIFICATION
            BOOST_CHECK_MESSAGE(error != SCRIPT_ERR_PUSH_SIZE, 
                              "Got SCRIPT_ERR_PUSH_SIZE even with SCRIPT_VERIFY_QUANTUM_SIGS flag set. Error code: " + std::to_string(error));
        }
    }
}

BOOST_AUTO_TEST_CASE(quantum_push_size_evalscript)
{
    // Test that EvalScript properly handles quantum push sizes
    
    // Create a large quantum pubkey push
    std::vector<unsigned char> ml_dsa_pubkey(1952, 0x02);
    
    CScript script;
    script << ml_dsa_pubkey << OP_DROP << OP_1;
    
    std::vector<std::vector<unsigned char>> stack;
    ScriptError error;
    BaseSignatureChecker checker;
    
    // Without SCRIPT_VERIFY_QUANTUM_SIGS, should fail
    {
        unsigned int flags = SCRIPT_VERIFY_P2SH;
        BOOST_CHECK(!EvalScript(stack, script, flags, checker, SigVersion::BASE, &error));
        BOOST_CHECK_EQUAL(error, SCRIPT_ERR_PUSH_SIZE);
    }
    
    // With SCRIPT_VERIFY_QUANTUM_SIGS, should succeed
    {
        stack.clear();
        unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_QUANTUM_SIGS;
        BOOST_CHECK(EvalScript(stack, script, flags, checker, SigVersion::BASE, &error));
        BOOST_CHECK_EQUAL(stack.size(), 1U);
        BOOST_CHECK(CastToBool(stack[0])); // Should have OP_1 on stack
    }
}

BOOST_AUTO_TEST_CASE(quantum_standard_script_verify_flags)
{
    // Test that SCRIPT_VERIFY_QUANTUM_SIGS is included in STANDARD_SCRIPT_VERIFY_FLAGS
    BOOST_CHECK(STANDARD_SCRIPT_VERIFY_FLAGS & SCRIPT_VERIFY_QUANTUM_SIGS);
}

BOOST_AUTO_TEST_CASE(quantum_witness_script_format)
{
    // Test quantum witness script creation and parsing
    
    // Test ML-DSA witness script
    {
        CQuantumKey key;
        key.MakeNewKey(KeyType::ML_DSA_65);
        CQuantumPubKey pubkey = key.GetPubKey();
        
        CScript witness_script = CreateQuantumWitnessScript(pubkey);
        
        // Verify script format: <pubkey> OP_CHECKSIG_ML_DSA
        CScript::const_iterator pc = witness_script.begin();
        opcodetype opcode;
        std::vector<unsigned char> vch_pubkey;
        
        BOOST_CHECK(witness_script.GetOp(pc, opcode, vch_pubkey));
        BOOST_CHECK(opcode <= OP_PUSHDATA4); // Should be a push
        BOOST_CHECK_EQUAL(vch_pubkey.size(), pubkey.GetKeyData().size());
        
        BOOST_CHECK(witness_script.GetOp(pc, opcode));
        BOOST_CHECK_EQUAL(opcode, OP_CHECKSIG_ML_DSA);
        
        BOOST_CHECK(pc == witness_script.end());
    }
    
    // Test SLH-DSA witness script
    {
        CQuantumKey key;
        key.MakeNewKey(KeyType::SLH_DSA_192F);
        CQuantumPubKey pubkey = key.GetPubKey();
        
        CScript witness_script = CreateQuantumWitnessScript(pubkey);
        
        // Verify script format: <pubkey> OP_CHECKSIG_SLH_DSA
        CScript::const_iterator pc = witness_script.begin();
        opcodetype opcode;
        std::vector<unsigned char> vch_pubkey;
        
        BOOST_CHECK(witness_script.GetOp(pc, opcode, vch_pubkey));
        BOOST_CHECK(opcode <= OP_PUSHDATA4); // Should be a push
        BOOST_CHECK_EQUAL(vch_pubkey.size(), pubkey.GetKeyData().size());
        
        BOOST_CHECK(witness_script.GetOp(pc, opcode));
        BOOST_CHECK_EQUAL(opcode, OP_CHECKSIG_SLH_DSA);
        
        BOOST_CHECK(pc == witness_script.end());
    }
}

BOOST_AUTO_TEST_CASE(quantum_mixed_signature_script)
{
    // Test scripts with both ECDSA and quantum signatures
    
    // Create keys
    CKey ecdsa_key;
    ecdsa_key.MakeNewKey(true);
    CPubKey ecdsa_pubkey = ecdsa_key.GetPubKey();
    
    CQuantumKey quantum_key;
    quantum_key.MakeNewKey(KeyType::ML_DSA_65);
    CQuantumPubKey quantum_pubkey = quantum_key.GetPubKey();
    
    // Create a 2-of-2 multisig-like script (one ECDSA, one quantum)
    CScript script;
    script << OP_DUP << ecdsa_pubkey << OP_CHECKSIGVERIFY;
    script << quantum_pubkey.GetKeyData() << OP_CHECKSIG_ML_DSA;
    
    // This script should be valid with SCRIPT_VERIFY_QUANTUM_SIGS
    // (actual execution would require proper signatures)
    std::vector<std::vector<unsigned char>> stack;
    ScriptError error;
    BaseSignatureChecker checker;
    unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_QUANTUM_SIGS;
    
    // Script parsing should succeed (execution would fail without proper sigs)
    BOOST_CHECK(!EvalScript(stack, script, flags, checker, SigVersion::BASE, &error));
    // Should fail on signature verification, not push size
    BOOST_CHECK(error != SCRIPT_ERR_PUSH_SIZE);
}

BOOST_AUTO_TEST_SUITE_END()