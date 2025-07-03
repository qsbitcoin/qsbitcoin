// Debug test for quantum signature verification

#include <boost/test/unit_test.hpp>
#include <test/util/setup_common.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <crypto/quantum_key.h>
#include <primitives/transaction.h>
#include <script/sigcache.h>
#include <script/sign.h>
#include <iostream>
#include <iomanip>
#include <uint256.h>
#include <crypto/sha256.h>
#include <consensus/validation.h>

// Include the quantum signature checker
#include <script/quantum_sigchecker.h>

BOOST_FIXTURE_TEST_SUITE(quantum_debug_test, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(debug_quantum_p2wsh_verification)
{
    BOOST_TEST_MESSAGE("=== DEBUG QUANTUM P2WSH VERIFICATION TEST ===");
    
    // Create quantum key
    quantum::CQuantumKey qKey;
    qKey.MakeNewKey(quantum::KeyType::ML_DSA_65);
    BOOST_REQUIRE(qKey.IsValid());
    
    auto qPubKey = qKey.GetPubKey();
    BOOST_TEST_MESSAGE("Created ML-DSA key, pubkey size: " << qPubKey.GetKeyData().size());
    
    // Create witness script - note: for P2WSH, we need just pubkey and opcode
    CScript witnessScript;
    witnessScript << ToByteVector(qPubKey.GetKeyData()) << OP_CHECKSIG_EX;
    BOOST_TEST_MESSAGE("Witness script size: " << witnessScript.size());
    
    // Debug: Print witness script bytes
    std::stringstream ss;
    ss << "Witness script bytes: ";
    for (size_t i = 0; i < std::min(static_cast<size_t>(witnessScript.size()), size_t(20)); i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)witnessScript[i] << " ";
    }
    ss << "...";
    BOOST_TEST_MESSAGE(ss.str());
    
    // Create P2WSH scriptPubKey
    uint256 scriptHash;
    CSHA256().Write(witnessScript.data(), witnessScript.size()).Finalize(scriptHash.begin());
    // Create P2WSH scriptPubKey manually
    CScript scriptPubKey;
    scriptPubKey << OP_0 << std::vector<unsigned char>(scriptHash.begin(), scriptHash.end());
    
    // Create a dummy previous transaction output
    CTxOut prevOut(100000, scriptPubKey);
    
    // Create spending transaction
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    uint256 dummyHash;
    dummyHash.SetNull();
    mtx.vin[0].prevout = COutPoint(Txid::FromUint256(dummyHash), 0);
    mtx.vout.resize(1);
    mtx.vout[0].nValue = 90000;
    mtx.vout[0].scriptPubKey = CScript() << OP_TRUE;
    
    CTransaction tx(mtx);
    
    // Create signature
    uint256 sighash = SignatureHash(witnessScript, tx, 0, SIGHASH_ALL, prevOut.nValue, SigVersion::WITNESS_V0);
    BOOST_TEST_MESSAGE("Sighash: " << sighash.ToString());
    
    std::vector<unsigned char> vchSig;
    BOOST_REQUIRE(qKey.Sign(sighash, vchSig));
    
    // Add algorithm ID and sighash type
    std::vector<unsigned char> fullSig;
    fullSig.push_back(0x02); // ML-DSA ID
    fullSig.insert(fullSig.end(), vchSig.begin(), vchSig.end());
    fullSig.push_back(SIGHASH_ALL);
    
    BOOST_TEST_MESSAGE("Full signature size: " << fullSig.size());
    
    // Create witness
    CScriptWitness witness;
    witness.stack.push_back(fullSig);
    witness.stack.push_back(std::vector<unsigned char>(witnessScript.begin(), witnessScript.end()));
    
    BOOST_TEST_MESSAGE("Witness stack size: " << witness.stack.size());
    BOOST_TEST_MESSAGE("Witness stack[0] (sig) size: " << witness.stack[0].size());
    BOOST_TEST_MESSAGE("Witness stack[1] (script) size: " << witness.stack[1].size());
    
    // Verify - use quantum transaction signature checker
    PrecomputedTransactionData txdata(tx);
    QuantumTransactionSignatureChecker checker(&tx, 0, prevOut.nValue, txdata, MissingDataBehavior::FAIL);
    
    // First test: Does the checker support quantum signatures?
    BOOST_TEST_MESSAGE("Testing if checker supports CheckQuantumSignature...");
    
    // Create a test script for direct verification - matching witness script
    CScript testScript;
    testScript << ToByteVector(qPubKey.GetKeyData()) << OP_CHECKSIG_EX;
    
    bool directCheck = checker.CheckQuantumSignature(fullSig, qPubKey.GetKeyData(), testScript, SigVersion::WITNESS_V0, 0x02);
    BOOST_TEST_MESSAGE("Direct CheckQuantumSignature result: " << (directCheck ? "SUCCESS" : "FAILURE"));
    
    // Now test full script verification
    unsigned int flags = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS | SCRIPT_VERIFY_QUANTUM_SIGS;
    ScriptError serror;
    
    BOOST_TEST_MESSAGE("\nCalling VerifyScript with flags=0x" << std::hex << flags << std::dec);
    bool result = VerifyScript(CScript(), scriptPubKey, &witness, flags, checker, &serror);
    
    BOOST_TEST_MESSAGE("VerifyScript result: " << (result ? "SUCCESS" : "FAILURE"));
    if (!result) {
        BOOST_TEST_MESSAGE("Error: " << ScriptErrorString(serror));
        BOOST_TEST_MESSAGE("Error code: " << (int)serror);
    }
    
    // Also test with standard TransactionSignatureChecker to see the difference
    BOOST_TEST_MESSAGE("\nTesting with standard TransactionSignatureChecker...");
    TransactionSignatureChecker stdChecker(&tx, 0, prevOut.nValue, txdata, MissingDataBehavior::FAIL);
    
    ScriptError serror2;
    bool result2 = VerifyScript(CScript(), scriptPubKey, &witness, flags, stdChecker, &serror2);
    BOOST_TEST_MESSAGE("Standard checker result: " << (result2 ? "SUCCESS" : "FAILURE"));
    if (!result2) {
        BOOST_TEST_MESSAGE("Error: " << ScriptErrorString(serror2));
    }
    
    // Remove ProduceSignature test for now
    
    BOOST_CHECK(result);
    
    BOOST_TEST_MESSAGE("=== END DEBUG TEST ===");
}

BOOST_AUTO_TEST_SUITE_END()