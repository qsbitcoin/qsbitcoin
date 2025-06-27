// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <script/script.h>
#include <script/interpreter.h>
#include <script/script_error.h>
#include <crypto/quantum_key.h>
#include <quantum_address.h>
#include <test/util/setup_common.h>
#include <uint256.h>

#include <vector>

using namespace quantum;

BOOST_FIXTURE_TEST_SUITE(script_quantum_tests_fixed, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(quantum_opcodes_basic)
{
    // Test that opcodes are properly defined
    BOOST_CHECK_EQUAL(OP_CHECKSIG_ML_DSA, OP_NOP4);
    BOOST_CHECK_EQUAL(OP_CHECKSIG_SLH_DSA, OP_NOP5);
    BOOST_CHECK_EQUAL(OP_CHECKSIGVERIFY_ML_DSA, OP_NOP6);
    BOOST_CHECK_EQUAL(OP_CHECKSIGVERIFY_SLH_DSA, OP_NOP7);
    
    // Test opcode names
    BOOST_CHECK_EQUAL(GetOpName(OP_CHECKSIG_ML_DSA), "OP_CHECKSIG_ML_DSA");
    BOOST_CHECK_EQUAL(GetOpName(OP_CHECKSIG_SLH_DSA), "OP_CHECKSIG_SLH_DSA");
    BOOST_CHECK_EQUAL(GetOpName(OP_CHECKSIGVERIFY_ML_DSA), "OP_CHECKSIGVERIFY_ML_DSA");
    BOOST_CHECK_EQUAL(GetOpName(OP_CHECKSIGVERIFY_SLH_DSA), "OP_CHECKSIGVERIFY_SLH_DSA");
}

BOOST_AUTO_TEST_CASE(quantum_script_creation)
{
    // Create quantum keys
    CQuantumKey mldsaKey;
    mldsaKey.MakeNewKey(KeyType::ML_DSA_65);
    CQuantumPubKey mldsaPubKey = mldsaKey.GetPubKey();
    
    CQuantumKey slhdsaKey;
    slhdsaKey.MakeNewKey(KeyType::SLH_DSA_192F);
    CQuantumPubKey slhdsaPubKey = slhdsaKey.GetPubKey();
    
    // Test P2QPKH script creation for ML-DSA
    uint256 mldsaHash = quantum::QuantumHash256(mldsaPubKey.GetKeyData());
    CScript mldsaScript = quantum::CreateP2QPKHScript(mldsaHash, KeyType::ML_DSA_65);
    
    // Verify ML-DSA script structure
    std::vector<unsigned char> scriptData(mldsaScript.begin(), mldsaScript.end());
    BOOST_CHECK_EQUAL(scriptData.size(), 37);
    BOOST_CHECK_EQUAL(scriptData[0], OP_DUP);
    BOOST_CHECK_EQUAL(scriptData[1], OP_HASH256);
    BOOST_CHECK_EQUAL(scriptData[2], 32); // Push 32 bytes
    BOOST_CHECK_EQUAL(scriptData[35], OP_EQUALVERIFY);
    BOOST_CHECK_EQUAL(scriptData[36], OP_CHECKSIG_ML_DSA);
    
    // Test P2QPKH script creation for SLH-DSA
    uint256 slhdsaHash = quantum::QuantumHash256(slhdsaPubKey.GetKeyData());
    CScript slhdsaScript = quantum::CreateP2QPKHScript(slhdsaHash, KeyType::SLH_DSA_192F);
    
    // Verify SLH-DSA script structure
    scriptData.assign(slhdsaScript.begin(), slhdsaScript.end());
    BOOST_CHECK_EQUAL(scriptData.size(), 37);
    BOOST_CHECK_EQUAL(scriptData[36], OP_CHECKSIG_SLH_DSA);
}

BOOST_AUTO_TEST_CASE(quantum_key_signing_basic)
{
    // Test basic quantum key signing functionality
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    
    uint256 hash;
    hash.SetNull();
    hash.data()[31] = 1;  // Set the least significant byte to 1
    
    std::vector<unsigned char> sig;
    BOOST_CHECK(key.Sign(hash, sig));
    BOOST_CHECK(!sig.empty());
    
    CQuantumPubKey pubkey = key.GetPubKey();
    BOOST_CHECK(CQuantumKey::Verify(hash, sig, pubkey));
    
    // Wrong hash should fail
    uint256 wrongHash;
    wrongHash.SetNull();
    wrongHash.data()[31] = 2;  // Set the least significant byte to 2
    BOOST_CHECK(!CQuantumKey::Verify(wrongHash, sig, pubkey));
}

BOOST_AUTO_TEST_CASE(quantum_opcodes_as_nops)
{
    // Test that quantum opcodes act as NOPs without SCRIPT_VERIFY_QUANTUM_SIGS
    CScript script;
    script << OP_1 << OP_CHECKSIG_ML_DSA;
    
    std::vector<std::vector<unsigned char>> stack;
    ScriptError error;
    BaseSignatureChecker checker;
    
    // Without SCRIPT_VERIFY_QUANTUM_SIGS, the opcode should act as a NOP
    unsigned int flags = SCRIPT_VERIFY_P2SH;
    BOOST_CHECK(EvalScript(stack, script, flags, checker, SigVersion::BASE, &error));
    BOOST_CHECK_EQUAL(stack.size(), 1U); // OP_1 remains on stack
    
    // With DISCOURAGE_UPGRADABLE_NOPS, it should fail
    flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS;
    stack.clear();
    BOOST_CHECK(!EvalScript(stack, script, flags, checker, SigVersion::BASE, &error));
    BOOST_CHECK_EQUAL(error, SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS);
}

BOOST_AUTO_TEST_CASE(p2qsh_script_creation)
{
    // Create a simple quantum redeem script
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    CQuantumPubKey pubKey = key.GetPubKey();
    
    CScript innerScript;
    innerScript << pubKey.GetKeyData() << OP_CHECKSIG_ML_DSA;
    
    // Create P2QSH script
    std::vector<unsigned char> innerScriptData(innerScript.begin(), innerScript.end());
    uint256 scriptHash = quantum::QuantumHash256(innerScriptData);
    CScript scriptPubKey = quantum::CreateP2QSHScript(scriptHash);
    
    // Verify P2QSH script structure
    std::vector<unsigned char> scriptData(scriptPubKey.begin(), scriptPubKey.end());
    BOOST_CHECK_EQUAL(scriptData.size(), 35);
    BOOST_CHECK_EQUAL(scriptData[0], OP_HASH256);
    BOOST_CHECK_EQUAL(scriptData[1], 32); // Push 32 bytes
    BOOST_CHECK_EQUAL(scriptData[34], OP_EQUAL);
}

BOOST_AUTO_TEST_SUITE_END()