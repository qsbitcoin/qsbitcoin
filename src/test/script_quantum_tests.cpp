// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <script/script.h>
#include <script/interpreter.h>
#include <script/script_error.h>
#include <crypto/quantum_key.h>
#include <crypto/quantum_key_io.h>
#include <quantum_address.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <primitives/transaction.h>

#include <vector>

using namespace quantum;

// Forward declaration
bool CastToBool(const std::vector<unsigned char>& vch);

BOOST_FIXTURE_TEST_SUITE(script_quantum_tests, BasicTestingSetup)

// Helper function to create a simple transaction for testing
static CMutableTransaction CreateTestTransaction()
{
    CMutableTransaction tx;
    tx.version = 2;
    tx.vin.resize(1);
    tx.vin[0].prevout.hash = Txid();
    tx.vin[0].prevout.n = 0;
    tx.vin[0].scriptSig = CScript();
    tx.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
    tx.vout.resize(1);
    tx.vout[0].nValue = 1000;
    tx.vout[0].scriptPubKey = CScript();
    tx.nLockTime = 0;
    return tx;
}

// Helper function to sign a transaction with a quantum key
static bool SignTransactionQuantum(const CQuantumKey& key, const CMutableTransaction& tx, int nIn, const CScript& scriptPubKey, std::vector<unsigned char>& vchSig)
{
    // Create transaction hash for signing
    uint256 hash = SignatureHash(scriptPubKey, tx, nIn, SIGHASH_ALL, 0, SigVersion::BASE);
    
    // Sign with quantum key
    if (!key.Sign(hash, vchSig)) {
        return false;
    }
    
    // Append sighash type
    vchSig.push_back(static_cast<unsigned char>(SIGHASH_ALL));
    return true;
}

BOOST_AUTO_TEST_CASE(quantum_opcodes_basic)
{
    // Test that quantum opcodes are recognized
    BOOST_CHECK_EQUAL(GetOpName(OP_CHECKSIG_ML_DSA), "OP_CHECKSIG_ML_DSA");
    BOOST_CHECK_EQUAL(GetOpName(OP_CHECKSIG_SLH_DSA), "OP_CHECKSIG_SLH_DSA");
    BOOST_CHECK_EQUAL(GetOpName(OP_CHECKSIGVERIFY_ML_DSA), "OP_CHECKSIGVERIFY_ML_DSA");
    BOOST_CHECK_EQUAL(GetOpName(OP_CHECKSIGVERIFY_SLH_DSA), "OP_CHECKSIGVERIFY_SLH_DSA");
}

BOOST_AUTO_TEST_CASE(quantum_script_creation)
{
    // Create ML-DSA key
    CQuantumKey mldsaKey;
    mldsaKey.MakeNewKey(KeyType::ML_DSA_65);
    CQuantumPubKey mldsaPubKey = mldsaKey.GetPubKey();
    
    // Create SLH-DSA key
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

BOOST_AUTO_TEST_CASE(quantum_signature_verification_ml_dsa)
{
    // Enable quantum signature verification
    const unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_STRICTENC | SCRIPT_VERIFY_QUANTUM_SIGS;
    
    // Create ML-DSA key
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    CQuantumPubKey pubKey = key.GetPubKey();
    
    // Create P2QPKH script
    uint256 pubKeyHash = quantum::QuantumHash256(pubKey.GetKeyData());
    CScript scriptPubKey = quantum::CreateP2QPKHScript(pubKeyHash, KeyType::ML_DSA_65);
    
    // Create test transaction
    CMutableTransaction tx = CreateTestTransaction();
    tx.vout[0].scriptPubKey = scriptPubKey;
    
    // Sign transaction
    std::vector<unsigned char> vchSig;
    BOOST_CHECK(SignTransactionQuantum(key, tx, 0, scriptPubKey, vchSig));
    
    // Create scriptSig
    CScript scriptSig;
    scriptSig << vchSig << pubKey.GetKeyData();
    
    // Verify script execution
    ScriptError err = SCRIPT_ERR_OK;
    std::vector<std::vector<unsigned char>> stack;
    const CTransaction txConst(tx);
    TransactionSignatureChecker checker(&txConst, 0, 0, MissingDataBehavior::FAIL);
    BOOST_CHECK(EvalScript(stack, scriptSig, flags, checker, SigVersion::BASE, &err));
    BOOST_CHECK(err == SCRIPT_ERR_OK);
    
    // Now evaluate the scriptPubKey
    BOOST_CHECK(EvalScript(stack, scriptPubKey, flags, checker, SigVersion::BASE, &err));
    BOOST_CHECK(err == SCRIPT_ERR_OK);
    BOOST_CHECK(!stack.empty());
    BOOST_CHECK(CastToBool(stack.back()));
}

BOOST_AUTO_TEST_CASE(quantum_signature_verification_slh_dsa)
{
    // Enable quantum signature verification
    const unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_STRICTENC | SCRIPT_VERIFY_QUANTUM_SIGS;
    
    // Create SLH-DSA key
    CQuantumKey key;
    key.MakeNewKey(KeyType::SLH_DSA_192F);
    CQuantumPubKey pubKey = key.GetPubKey();
    
    // Create P2QPKH script
    uint256 pubKeyHash = quantum::QuantumHash256(pubKey.GetKeyData());
    CScript scriptPubKey = quantum::CreateP2QPKHScript(pubKeyHash, KeyType::SLH_DSA_192F);
    
    // Create test transaction
    CMutableTransaction tx = CreateTestTransaction();
    tx.vout[0].scriptPubKey = scriptPubKey;
    
    // Sign transaction
    std::vector<unsigned char> vchSig;
    BOOST_CHECK(SignTransactionQuantum(key, tx, 0, scriptPubKey, vchSig));
    
    // Create scriptSig
    CScript scriptSig;
    scriptSig << vchSig << pubKey.GetKeyData();
    
    // Verify script execution
    ScriptError err = SCRIPT_ERR_OK;
    std::vector<std::vector<unsigned char>> stack;
    const CTransaction txConst(tx);
    TransactionSignatureChecker checker(&txConst, 0, 0, MissingDataBehavior::FAIL);
    BOOST_CHECK(EvalScript(stack, scriptSig, flags, checker, SigVersion::BASE, &err));
    BOOST_CHECK(err == SCRIPT_ERR_OK);
    
    // Now evaluate the scriptPubKey
    BOOST_CHECK(EvalScript(stack, scriptPubKey, flags, checker, SigVersion::BASE, &err));
    BOOST_CHECK(err == SCRIPT_ERR_OK);
    BOOST_CHECK(!stack.empty());
    BOOST_CHECK(CastToBool(stack.back()));
}

BOOST_AUTO_TEST_CASE(quantum_signature_verification_without_flag)
{
    // Test without SCRIPT_VERIFY_QUANTUM_SIGS flag - should fail
    const unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_STRICTENC;
    
    // Create ML-DSA key
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    CQuantumPubKey pubKey = key.GetPubKey();
    
    // Create P2QPKH script
    uint256 pubKeyHash = quantum::QuantumHash256(pubKey.GetKeyData());
    CScript scriptPubKey = quantum::CreateP2QPKHScript(pubKeyHash, KeyType::ML_DSA_65);
    
    // Create test transaction
    CMutableTransaction tx = CreateTestTransaction();
    tx.vout[0].scriptPubKey = scriptPubKey;
    
    // Sign transaction
    std::vector<unsigned char> vchSig;
    BOOST_CHECK(SignTransactionQuantum(key, tx, 0, scriptPubKey, vchSig));
    
    // Create scriptSig
    CScript scriptSig;
    scriptSig << vchSig << pubKey.GetKeyData();
    
    // Verify script execution - should succeed for scriptSig
    ScriptError err = SCRIPT_ERR_OK;
    std::vector<std::vector<unsigned char>> stack;
    const CTransaction txConst(tx);
    TransactionSignatureChecker checker(&txConst, 0, 0, MissingDataBehavior::FAIL);
    BOOST_CHECK(EvalScript(stack, scriptSig, flags, checker, SigVersion::BASE, &err));
    
    // Now evaluate the scriptPubKey - should fail because quantum sigs are not enabled
    BOOST_CHECK(!EvalScript(stack, scriptPubKey, flags, checker, SigVersion::BASE, &err));
    BOOST_CHECK(err == SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS);
}

BOOST_AUTO_TEST_CASE(quantum_checksigverify_opcodes)
{
    const unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_STRICTENC | SCRIPT_VERIFY_QUANTUM_SIGS;
    
    // Test ML-DSA CHECKSIGVERIFY
    {
        CQuantumKey key;
        key.MakeNewKey(KeyType::ML_DSA_65);
        CQuantumPubKey pubKey = key.GetPubKey();
        
        // Create script with CHECKSIGVERIFY_ML_DSA followed by OP_1
        CScript scriptPubKey;
        scriptPubKey << pubKey.GetKeyData() << OP_CHECKSIGVERIFY_ML_DSA << OP_1;
        
        // Create test transaction
        CMutableTransaction tx = CreateTestTransaction();
        tx.vout[0].scriptPubKey = scriptPubKey;
        
        // Sign transaction
        std::vector<unsigned char> vchSig;
        BOOST_CHECK(SignTransactionQuantum(key, tx, 0, scriptPubKey, vchSig));
        
        // Create scriptSig
        CScript scriptSig;
        scriptSig << vchSig;
        
        // Verify script execution
        ScriptError err = SCRIPT_ERR_OK;
        std::vector<std::vector<unsigned char>> stack;
        const CTransaction txConst(tx);
    TransactionSignatureChecker checker(&txConst, 0, 0, MissingDataBehavior::FAIL);
        BOOST_CHECK(EvalScript(stack, scriptSig, flags, checker, SigVersion::BASE, &err));
        BOOST_CHECK(EvalScript(stack, scriptPubKey, flags, checker, SigVersion::BASE, &err));
        BOOST_CHECK(err == SCRIPT_ERR_OK);
        BOOST_CHECK(!stack.empty());
        BOOST_CHECK(CastToBool(stack.back()));
    }
    
    // Test SLH-DSA CHECKSIGVERIFY
    {
        CQuantumKey key;
        key.MakeNewKey(KeyType::SLH_DSA_192F);
        CQuantumPubKey pubKey = key.GetPubKey();
        
        // Create script with CHECKSIGVERIFY_SLH_DSA followed by OP_1
        CScript scriptPubKey;
        scriptPubKey << pubKey.GetKeyData() << OP_CHECKSIGVERIFY_SLH_DSA << OP_1;
        
        // Create test transaction
        CMutableTransaction tx = CreateTestTransaction();
        tx.vout[0].scriptPubKey = scriptPubKey;
        
        // Sign transaction
        std::vector<unsigned char> vchSig;
        BOOST_CHECK(SignTransactionQuantum(key, tx, 0, scriptPubKey, vchSig));
        
        // Create scriptSig
        CScript scriptSig;
        scriptSig << vchSig;
        
        // Verify script execution
        ScriptError err = SCRIPT_ERR_OK;
        std::vector<std::vector<unsigned char>> stack;
        const CTransaction txConst(tx);
    TransactionSignatureChecker checker(&txConst, 0, 0, MissingDataBehavior::FAIL);
        BOOST_CHECK(EvalScript(stack, scriptSig, flags, checker, SigVersion::BASE, &err));
        BOOST_CHECK(EvalScript(stack, scriptPubKey, flags, checker, SigVersion::BASE, &err));
        BOOST_CHECK(err == SCRIPT_ERR_OK);
        BOOST_CHECK(!stack.empty());
        BOOST_CHECK(CastToBool(stack.back()));
    }
}

BOOST_AUTO_TEST_CASE(quantum_signature_invalid_cases)
{
    const unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_STRICTENC | SCRIPT_VERIFY_QUANTUM_SIGS;
    
    // Test with wrong signature
    {
        CQuantumKey key1, key2;
        key1.MakeNewKey(KeyType::ML_DSA_65);
        key2.MakeNewKey(KeyType::ML_DSA_65);
        
        CQuantumPubKey pubKey1 = key1.GetPubKey();
        
        // Create P2QPKH script for key1
        uint256 pubKeyHash = quantum::QuantumHash256(pubKey1.GetKeyData());
        CScript scriptPubKey = quantum::CreateP2QPKHScript(pubKeyHash, KeyType::ML_DSA_65);
        
        // Create test transaction
        CMutableTransaction tx = CreateTestTransaction();
        tx.vout[0].scriptPubKey = scriptPubKey;
        
        // Sign with key2 (wrong key)
        std::vector<unsigned char> vchSig;
        BOOST_CHECK(SignTransactionQuantum(key2, tx, 0, scriptPubKey, vchSig));
        
        // Create scriptSig with key1's pubkey but key2's signature
        CScript scriptSig;
        scriptSig << vchSig << pubKey1.GetKeyData();
        
        // Verify script execution - should fail
        ScriptError err = SCRIPT_ERR_OK;
        std::vector<std::vector<unsigned char>> stack;
        const CTransaction txConst(tx);
    TransactionSignatureChecker checker(&txConst, 0, 0, MissingDataBehavior::FAIL);
        BOOST_CHECK(EvalScript(stack, scriptSig, flags, checker, SigVersion::BASE, &err));
        BOOST_CHECK(!EvalScript(stack, scriptPubKey, flags, checker, SigVersion::BASE, &err));
        BOOST_CHECK(err == SCRIPT_ERR_QUANTUM_SIG_VERIFICATION);
    }
    
    // Test with wrong public key type
    {
        CQuantumKey mldsaKey;
        mldsaKey.MakeNewKey(KeyType::ML_DSA_65);
        CQuantumPubKey mldsaPubKey = mldsaKey.GetPubKey();
        
        // Create P2QPKH script expecting SLH-DSA
        uint256 pubKeyHash = quantum::QuantumHash256(mldsaPubKey.GetKeyData());
        CScript scriptPubKey = quantum::CreateP2QPKHScript(pubKeyHash, KeyType::SLH_DSA_192F);
        
        // Create test transaction
        CMutableTransaction tx = CreateTestTransaction();
        tx.vout[0].scriptPubKey = scriptPubKey;
        
        // Sign with ML-DSA key
        std::vector<unsigned char> vchSig;
        BOOST_CHECK(SignTransactionQuantum(mldsaKey, tx, 0, scriptPubKey, vchSig));
        
        // Create scriptSig
        CScript scriptSig;
        scriptSig << vchSig << mldsaPubKey.GetKeyData();
        
        // Verify script execution - should fail due to key type mismatch
        ScriptError err = SCRIPT_ERR_OK;
        std::vector<std::vector<unsigned char>> stack;
        const CTransaction txConst(tx);
    TransactionSignatureChecker checker(&txConst, 0, 0, MissingDataBehavior::FAIL);
        BOOST_CHECK(EvalScript(stack, scriptSig, flags, checker, SigVersion::BASE, &err));
        BOOST_CHECK(!EvalScript(stack, scriptPubKey, flags, checker, SigVersion::BASE, &err));
        BOOST_CHECK(err == SCRIPT_ERR_QUANTUM_WRONG_KEY_TYPE);
    }
}

BOOST_AUTO_TEST_CASE(quantum_p2qsh_script)
{
    const unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_STRICTENC | SCRIPT_VERIFY_QUANTUM_SIGS;
    
    // Create a quantum key
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    CQuantumPubKey pubKey = key.GetPubKey();
    
    // Create inner script (simple pay-to-pubkey with quantum signature)
    CScript innerScript;
    innerScript << pubKey.GetKeyData() << OP_CHECKSIG_ML_DSA;
    
    // Create P2QSH script
    uint256 scriptHash = quantum::QuantumHash256(std::vector<unsigned char>(innerScript.begin(), innerScript.end()));
    CScript scriptPubKey = quantum::CreateP2QSHScript(scriptHash);
    
    // Verify P2QSH script structure
    std::vector<unsigned char> scriptData(scriptPubKey.begin(), scriptPubKey.end());
    BOOST_CHECK_EQUAL(scriptData.size(), 35);
    BOOST_CHECK_EQUAL(scriptData[0], OP_HASH256);
    BOOST_CHECK_EQUAL(scriptData[1], 32); // Push 32 bytes
    BOOST_CHECK_EQUAL(scriptData[34], OP_EQUAL);
    
    // Create test transaction
    CMutableTransaction tx = CreateTestTransaction();
    tx.vout[0].scriptPubKey = scriptPubKey;
    
    // Sign transaction
    std::vector<unsigned char> vchSig;
    BOOST_CHECK(SignTransactionQuantum(key, tx, 0, innerScript, vchSig));
    
    // Create scriptSig with signature and serialized inner script
    CScript scriptSig;
    scriptSig << vchSig;
    scriptSig << std::vector<unsigned char>(innerScript.begin(), innerScript.end());
    
    // Verify script execution
    ScriptError err = SCRIPT_ERR_OK;
    std::vector<std::vector<unsigned char>> stack;
    const CTransaction txConst(tx);
    TransactionSignatureChecker checker(&txConst, 0, 0, MissingDataBehavior::FAIL);
    BOOST_CHECK(EvalScript(stack, scriptSig, flags, checker, SigVersion::BASE, &err));
    BOOST_CHECK(EvalScript(stack, scriptPubKey, flags, checker, SigVersion::BASE, &err));
    BOOST_CHECK(err == SCRIPT_ERR_OK);
    BOOST_CHECK(!stack.empty());
    BOOST_CHECK(CastToBool(stack.back()));
}

BOOST_AUTO_TEST_SUITE_END()