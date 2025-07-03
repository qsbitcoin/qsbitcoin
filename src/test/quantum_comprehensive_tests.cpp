// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <consensus/validation.h>
#include <crypto/quantum_key.h>
#include <crypto/sha256.h>
#include <script/quantum_signature.h>
#include <script/quantum_sigchecker.h>
#include <policy/policy.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <script/sign.h>
#include <script/signingprovider.h>
#include <test/util/setup_common.h>
#include <util/strencodings.h>

using namespace quantum;

BOOST_FIXTURE_TEST_SUITE(quantum_comprehensive_tests, BasicTestingSetup)

// Test 1: Complete quantum transaction lifecycle
BOOST_AUTO_TEST_CASE(quantum_transaction_complete_cycle)
{
    // Create quantum keys for both signature schemes
    CQuantumKey ml_dsa_key, slh_dsa_key;
    ml_dsa_key.MakeNewKey(KeyType::ML_DSA_65);
    slh_dsa_key.MakeNewKey(KeyType::SLH_DSA_192F);
    
    CQuantumPubKey ml_dsa_pubkey = ml_dsa_key.GetPubKey();
    CQuantumPubKey slh_dsa_pubkey = slh_dsa_key.GetPubKey();
    
    // Verify key sizes
    BOOST_CHECK_EQUAL(ml_dsa_pubkey.GetKeyData().size(), 1952); // ML-DSA-65 pubkey size
    BOOST_CHECK_EQUAL(slh_dsa_pubkey.GetKeyData().size(), 48);   // SLH-DSA-192f pubkey size
    
    // Test ML-DSA transaction
    {
        // Create witness script
        CScript witness_script;
        witness_script << ml_dsa_pubkey.GetKeyData() << OP_CHECKSIG_EX;
        BOOST_CHECK_EQUAL(witness_script.size(), 1952 + 3 + 1); // ML-DSA-65 pubkey + PUSHDATA2 + opcode
        
        // Create P2WSH scriptPubKey
        uint256 script_hash;
        CSHA256().Write(witness_script.data(), witness_script.size()).Finalize(script_hash.begin());
        CScript scriptPubKey;
        scriptPubKey << OP_0 << std::vector<unsigned char>(script_hash.begin(), script_hash.end());
        
        // Create transaction
        CMutableTransaction mtx;
        mtx.version = 2;
        mtx.vin.resize(1);
        mtx.vin[0].prevout.hash = Txid::FromHex("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef").value();
        mtx.vin[0].prevout.n = 0;
        mtx.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
        mtx.vout.resize(1);
        mtx.vout[0].nValue = 99900000;
        mtx.vout[0].scriptPubKey = CScript() << OP_TRUE;
        
        // Sign transaction
        CAmount amount = 100000000;
        uint256 sighash = SignatureHash(witness_script, mtx, 0, SIGHASH_ALL, amount, SigVersion::WITNESS_V0);
        
        std::vector<unsigned char> sig;
        BOOST_CHECK(ml_dsa_key.Sign(sighash, sig));
        sig.push_back(SIGHASH_ALL);
        BOOST_CHECK_EQUAL(sig.size(), 3309 + 1); // 3309 bytes + sighash byte
        
        // Prepend algorithm ID for OP_CHECKSIG_EX
        std::vector<unsigned char> full_sig;
        full_sig.push_back(quantum::SCHEME_ML_DSA_65);
        full_sig.insert(full_sig.end(), sig.begin(), sig.end());
        
        // Build witness
        CScriptWitness witness;
        witness.stack.push_back(full_sig);
        witness.stack.push_back(std::vector<unsigned char>(witness_script.begin(), witness_script.end()));
        mtx.vin[0].scriptWitness = witness;
        
        
        CTransaction tx(mtx);
        
        // Verify script execution
        PrecomputedTransactionData txdata(tx);
        QuantumTransactionSignatureChecker checker(&tx, 0, amount, txdata, MissingDataBehavior::FAIL);
        ScriptError error = SCRIPT_ERR_OK;
        
        // Should fail without quantum flags
        unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS;
        BOOST_CHECK(!VerifyScript(CScript(), scriptPubKey, &witness, flags, checker, &error));
        BOOST_CHECK_EQUAL(error, SCRIPT_ERR_PUSH_SIZE);
        
        // Should succeed with quantum flags
        flags |= SCRIPT_VERIFY_QUANTUM_SIGS;
        error = SCRIPT_ERR_OK;
        BOOST_CHECK(VerifyScript(CScript(), scriptPubKey, &witness, flags, checker, &error));
        BOOST_CHECK_EQUAL(error, SCRIPT_ERR_OK);
        
        // Check transaction weight
        size_t weight = GetTransactionWeight(tx);
        BOOST_CHECK_GT(weight, 3000); // Should be significant due to large signature
        BOOST_CHECK_LT(weight, MAX_STANDARD_TX_WEIGHT * 3); // But still within extended limits
    }
    
    // Test SLH-DSA transaction
    {
        // Create witness script
        CScript witness_script;
        witness_script << slh_dsa_pubkey.GetKeyData() << OP_CHECKSIG_EX;
        BOOST_CHECK_EQUAL(witness_script.size(), 48 + 1 + 1); // SLH-DSA-192f pubkey + length byte + opcode
        
        // Create P2WSH scriptPubKey
        uint256 script_hash;
        CSHA256().Write(witness_script.data(), witness_script.size()).Finalize(script_hash.begin());
        CScript scriptPubKey;
        scriptPubKey << OP_0 << std::vector<unsigned char>(script_hash.begin(), script_hash.end());
        
        // Create transaction
        CMutableTransaction mtx;
        mtx.version = 2;
        mtx.vin.resize(1);
        mtx.vin[0].prevout.hash = Txid::FromHex("fedcba9876543210fedcba9876543210fedcba9876543210fedcba9876543210").value();
        mtx.vin[0].prevout.n = 0;
        mtx.vout.resize(1);
        mtx.vout[0].nValue = 99900000;
        mtx.vout[0].scriptPubKey = CScript() << OP_TRUE;
        
        // Sign transaction
        CAmount amount = 100000000;
        uint256 sighash = SignatureHash(witness_script, mtx, 0, SIGHASH_ALL, amount, SigVersion::WITNESS_V0);
        
        std::vector<unsigned char> sig;
        BOOST_CHECK(slh_dsa_key.Sign(sighash, sig));
        sig.push_back(SIGHASH_ALL);
        BOOST_CHECK_EQUAL(sig.size(), 35664 + 1); // 35664 bytes + sighash byte
        
        // Prepend algorithm ID for OP_CHECKSIG_EX
        std::vector<unsigned char> full_sig;
        full_sig.push_back(quantum::SCHEME_SLH_DSA_192F);
        full_sig.insert(full_sig.end(), sig.begin(), sig.end());
        
        // Build witness
        CScriptWitness witness;
        witness.stack.push_back(full_sig);
        witness.stack.push_back(std::vector<unsigned char>(witness_script.begin(), witness_script.end()));
        mtx.vin[0].scriptWitness = witness;
        
        CTransaction tx(mtx);
        
        // Verify with quantum flags
        PrecomputedTransactionData txdata(tx);
        QuantumTransactionSignatureChecker checker(&tx, 0, amount, txdata, MissingDataBehavior::FAIL);
        ScriptError error = SCRIPT_ERR_OK;
        
        unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS | SCRIPT_VERIFY_QUANTUM_SIGS;
        BOOST_CHECK(VerifyScript(CScript(), scriptPubKey, &witness, flags, checker, &error));
        BOOST_CHECK_EQUAL(error, SCRIPT_ERR_OK);
    }
}

// Test 2: Witness corruption prevention
BOOST_AUTO_TEST_CASE(quantum_witness_corruption_prevention)
{
    // Create a transaction with valid quantum witness
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    CQuantumPubKey pubkey = key.GetPubKey();
    
    CScript witness_script;
    witness_script << pubkey.GetKeyData() << OP_CHECKSIG_EX;
    
    uint256 script_hash;
    CSHA256().Write(witness_script.data(), witness_script.size()).Finalize(script_hash.begin());
    CScript scriptPubKey;
    scriptPubKey << OP_0 << std::vector<unsigned char>(script_hash.begin(), script_hash.end());
    
    CMutableTransaction mtx;
    mtx.version = 2;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.hash = Txid::FromHex("abababababababababababababababababababababababababababababababab").value();
    mtx.vin[0].prevout.n = 0;
    mtx.vout.resize(1);
    mtx.vout[0].nValue = 99900000;
    mtx.vout[0].scriptPubKey = CScript() << OP_TRUE;
    
    // Sign properly
    CAmount amount = 100000000;
    uint256 sighash = SignatureHash(witness_script, mtx, 0, SIGHASH_ALL, amount, SigVersion::WITNESS_V0);
    
    std::vector<unsigned char> sig;
    BOOST_CHECK(key.Sign(sighash, sig));
    sig.push_back(SIGHASH_ALL);
    
    // Prepend algorithm ID for OP_CHECKSIG_EX
    std::vector<unsigned char> full_sig;
    full_sig.push_back(quantum::SCHEME_ML_DSA_65);
    full_sig.insert(full_sig.end(), sig.begin(), sig.end());
    
    // Create valid witness with 2 elements
    CScriptWitness witness;
    witness.stack.push_back(full_sig);
    witness.stack.push_back(std::vector<unsigned char>(witness_script.begin(), witness_script.end()));
    mtx.vin[0].scriptWitness = witness;
    
    // Verify original witness
    BOOST_CHECK_EQUAL(witness.stack.size(), 2);
    BOOST_CHECK_EQUAL(witness.stack[0].size(), 3311); // algo ID + 3309 + sighash byte
    
    // Test witness malleation attempts
    CTransaction tx(mtx);
    
    // Attempt 1: Add extra element
    {
        CMutableTransaction mtx_mal = mtx;
        mtx_mal.vin[0].scriptWitness.stack.push_back(std::vector<unsigned char>{0x00});
        
        CTransaction tx_mal(mtx_mal);
        PrecomputedTransactionData txdata(tx_mal);
        QuantumTransactionSignatureChecker checker(&tx_mal, 0, amount, txdata, MissingDataBehavior::FAIL);
        ScriptError error = SCRIPT_ERR_OK;
        
        unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS | SCRIPT_VERIFY_QUANTUM_SIGS;
        BOOST_CHECK(!VerifyScript(CScript(), scriptPubKey, &mtx_mal.vin[0].scriptWitness, flags, checker, &error));
    }
    
    // Attempt 2: Remove element (corruption)
    {
        CMutableTransaction mtx_mal = mtx;
        mtx_mal.vin[0].scriptWitness.stack.pop_back();
        
        CTransaction tx_mal(mtx_mal);
        PrecomputedTransactionData txdata(tx_mal);
        QuantumTransactionSignatureChecker checker(&tx_mal, 0, amount, txdata, MissingDataBehavior::FAIL);
        ScriptError error = SCRIPT_ERR_OK;
        
        unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS | SCRIPT_VERIFY_QUANTUM_SIGS;
        BOOST_CHECK(!VerifyScript(CScript(), scriptPubKey, &mtx_mal.vin[0].scriptWitness, flags, checker, &error));
    }
    
    // Original should still verify
    {
        PrecomputedTransactionData txdata(tx);
        QuantumTransactionSignatureChecker checker(&tx, 0, amount, txdata, MissingDataBehavior::FAIL);
        ScriptError error = SCRIPT_ERR_OK;
        
        unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS | SCRIPT_VERIFY_QUANTUM_SIGS;
        BOOST_CHECK(VerifyScript(CScript(), scriptPubKey, &mtx.vin[0].scriptWitness, flags, checker, &error));
        BOOST_CHECK_EQUAL(error, SCRIPT_ERR_OK);
    }
}

// Test 3: Push size limits and edge cases
BOOST_AUTO_TEST_CASE(quantum_push_size_edge_cases)
{
    // Test exact limits
    {
        // MAX_SCRIPT_ELEMENT_SIZE should work without quantum flags
        std::vector<unsigned char> data(MAX_SCRIPT_ELEMENT_SIZE, 0xFF);
        CScript script;
        script << data << OP_DROP << OP_1;
        
        std::vector<std::vector<unsigned char>> stack;
        ScriptError error = SCRIPT_ERR_OK;
        BaseSignatureChecker checker;
        
        unsigned int flags = SCRIPT_VERIFY_P2SH;
        BOOST_CHECK(EvalScript(stack, script, flags, checker, SigVersion::BASE, &error));
        BOOST_CHECK_EQUAL(error, SCRIPT_ERR_OK);
    }
    
    // Test quantum signature sizes
    {
        // ML-DSA signature size (including sighash byte)
        std::vector<unsigned char> ml_dsa_sig(3310, 0x01);
        CScript script;
        script << ml_dsa_sig;
        
        std::vector<std::vector<unsigned char>> stack;
        ScriptError error;
        BaseSignatureChecker checker;
        
        // Should fail without quantum flags
        error = SCRIPT_ERR_OK;
        unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS;
        BOOST_CHECK(!EvalScript(stack, script, flags, checker, SigVersion::WITNESS_V0, &error));
        BOOST_CHECK_EQUAL(error, SCRIPT_ERR_PUSH_SIZE);
        
        // Should pass with quantum flags in witness context
        stack.clear();
        error = SCRIPT_ERR_OK;
        flags |= SCRIPT_VERIFY_QUANTUM_SIGS;
        // In ExecuteWitnessScript, the size check is different
        // We need to test through the full witness execution path
    }
    
    // Test SLH-DSA signature size
    {
        std::vector<unsigned char> slh_dsa_sig(35665, 0x01); // 35664 + sighash byte
        BOOST_CHECK_GT(slh_dsa_sig.size(), MAX_SCRIPT_ELEMENT_SIZE);
        
        // These large signatures are only valid in witness context with quantum flags
    }
}

// Test 4: Policy and consensus flag validation
BOOST_AUTO_TEST_CASE(quantum_policy_consensus_flags)
{
    // Verify quantum flags are in the right sets
    BOOST_CHECK((STANDARD_SCRIPT_VERIFY_FLAGS & SCRIPT_VERIFY_QUANTUM_SIGS) != 0);
    BOOST_CHECK((MANDATORY_SCRIPT_VERIFY_FLAGS & SCRIPT_VERIFY_QUANTUM_SIGS) != 0);
    
    // Test that IsWitnessStandard accepts quantum transactions
    {
        CMutableTransaction mtx;
        mtx.version = 2;
        mtx.vin.resize(1);
        mtx.vout.resize(1);
        mtx.vout[0].nValue = 100000000;
        mtx.vout[0].scriptPubKey = CScript() << OP_TRUE;
        
        // Add large quantum witness
        CScriptWitness witness;
        witness.stack.push_back(std::vector<unsigned char>(3310, 0x01)); // ML-DSA sig with sighash
        witness.stack.push_back(std::vector<unsigned char>(1953, 0x02)); // Witness script
        mtx.vin[0].scriptWitness = witness;
        
        CTransaction tx(mtx);
        CCoinsView coinView;
        CCoinsViewCache coins(&coinView);
        
        // Should pass standardness check
        BOOST_CHECK(IsWitnessStandard(tx, coins));
    }
}

// Test 5: Different signature hash types
BOOST_AUTO_TEST_CASE(quantum_sighash_types)
{
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    CQuantumPubKey pubkey = key.GetPubKey();
    
    CScript witness_script;
    witness_script << pubkey.GetKeyData() << OP_CHECKSIG_EX;
    
    CMutableTransaction mtx;
    mtx.version = 2;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.hash = Txid::FromHex("1234567890abcdef1234567890abcdef1234567890abcdef1234567890abcdef").value();
    mtx.vin[0].prevout.n = 0;
    mtx.vout.resize(2);
    mtx.vout[0].nValue = 50000000;
    mtx.vout[0].scriptPubKey = CScript() << OP_TRUE;
    mtx.vout[1].nValue = 49900000;
    mtx.vout[1].scriptPubKey = CScript() << OP_FALSE;
    
    CAmount amount = 100000000;
    
    // Test different sighash types
    int sighash_types[] = {SIGHASH_ALL, SIGHASH_NONE, SIGHASH_SINGLE, 
                          SIGHASH_ALL | SIGHASH_ANYONECANPAY};
    
    for (int sighash_type : sighash_types) {
        uint256 sighash = SignatureHash(witness_script, mtx, 0, sighash_type, amount, SigVersion::WITNESS_V0);
        
        std::vector<unsigned char> sig;
        BOOST_CHECK(key.Sign(sighash, sig));
        sig.push_back(sighash_type);
        
        // Verify signature
        std::vector<unsigned char> sig_no_hashtype(sig.begin(), sig.end() - 1);
        BOOST_CHECK(CQuantumKey::Verify(sighash, sig_no_hashtype, pubkey));
    }
}

// Test 6: Error handling and invalid inputs
BOOST_AUTO_TEST_CASE(quantum_error_handling)
{
    // Test invalid key types
    {
        CQuantumKey key;
        // Default constructed key should be invalid
        BOOST_CHECK(!key.IsValid());
        
        uint256 hash;
        std::vector<unsigned char> sig;
        BOOST_CHECK(!key.Sign(hash, sig));
    }
    
    // Test invalid public keys
    {
        std::vector<unsigned char> invalid_data(100, 0xFF);
        CQuantumPubKey invalid_pubkey(KeyType::ML_DSA_65, invalid_data);
        BOOST_CHECK(!invalid_pubkey.IsValid());
    }
    
    // Test signature verification with wrong key
    {
        CQuantumKey key1, key2;
        key1.MakeNewKey(KeyType::ML_DSA_65);
        key2.MakeNewKey(KeyType::ML_DSA_65);
        
        uint256 hash = GetRandHash();
        std::vector<unsigned char> sig;
        BOOST_CHECK(key1.Sign(hash, sig));
        
        // Should fail with wrong pubkey
        BOOST_CHECK(!CQuantumKey::Verify(hash, sig, key2.GetPubKey()));
        
        // Should succeed with correct pubkey
        BOOST_CHECK(CQuantumKey::Verify(hash, sig, key1.GetPubKey()));
    }
    
    // Test malformed signatures
    {
        CQuantumKey key;
        key.MakeNewKey(KeyType::ML_DSA_65);
        CQuantumPubKey pubkey = key.GetPubKey();
        
        uint256 hash = GetRandHash();
        
        // Empty signature
        std::vector<unsigned char> empty_sig;
        BOOST_CHECK(!CQuantumKey::Verify(hash, empty_sig, pubkey));
        
        // Truncated signature
        std::vector<unsigned char> sig;
        BOOST_CHECK(key.Sign(hash, sig));
        sig.resize(sig.size() / 2);
        BOOST_CHECK(!CQuantumKey::Verify(hash, sig, pubkey));
    }
}

BOOST_AUTO_TEST_SUITE_END()