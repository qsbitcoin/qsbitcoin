// Debug test for quantum push size issue
#include <boost/test/unit_test.hpp>
#include <script/script.h>
#include <script/interpreter.h>
#include <script/quantum_signature.h>
#include <script/quantum_sigchecker.h>
#include <crypto/quantum_key.h>
#include <script/quantum_witness.h>
#include <primitives/transaction.h>
#include <test/util/setup_common.h>

using namespace quantum;

BOOST_FIXTURE_TEST_SUITE(debug_quantum_push, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(test_witness_script_sizes)
{
    // Create a quantum key
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    CQuantumPubKey pubkey = key.GetPubKey();
    
    // Create witness script
    CScript witness_script = CreateQuantumWitnessScript(pubkey);
    
    BOOST_TEST_MESSAGE("Witness script size: " << witness_script.size());
    BOOST_TEST_MESSAGE("Pubkey size: " << pubkey.GetKeyData().size());
    
    // Parse the witness script
    CScript::const_iterator pc = witness_script.begin();
    opcodetype opcode;
    std::vector<unsigned char> vch;
    int element = 0;
    
    while (pc < witness_script.end()) {
        if (witness_script.GetOp(pc, opcode, vch)) {
            if (!vch.empty()) {
                BOOST_TEST_MESSAGE("Element " << element << ": data of size " << vch.size());
            } else {
                BOOST_TEST_MESSAGE("Element " << element << ": opcode " << static_cast<int>(opcode));
            }
            element++;
        }
    }
    
    // Create a signature
    uint256 hash;
    hash.SetNull();
    std::vector<unsigned char> sig;
    BOOST_CHECK(key.Sign(hash, sig));
    
    BOOST_TEST_MESSAGE("Raw signature size: " << sig.size());
    
    // Add algorithm ID and sighash
    std::vector<unsigned char> full_sig;
    full_sig.push_back(SCHEME_ML_DSA_65);
    full_sig.insert(full_sig.end(), sig.begin(), sig.end());
    full_sig.push_back(SIGHASH_ALL);
    
    BOOST_TEST_MESSAGE("Full signature size: " << full_sig.size());
    
    // Test if these sizes would pass the push size check
    if (full_sig.size() >= 3300 && full_sig.size() <= 3320) {
        BOOST_TEST_MESSAGE("Signature size would pass ML-DSA check");
    } else {
        BOOST_TEST_MESSAGE("Signature size would NOT pass ML-DSA check");
    }
    
    if (witness_script.size() > 50 && witness_script.size() < 100) {
        BOOST_TEST_MESSAGE("Witness script would match SLH-DSA range");
    } else if (witness_script.size() > 1950 && witness_script.size() < 2000) {
        BOOST_TEST_MESSAGE("Witness script would match ML-DSA range");
    } else {
        BOOST_TEST_MESSAGE("Witness script size " << witness_script.size() << " doesn't match any expected range");
    }
}

BOOST_AUTO_TEST_SUITE_END()