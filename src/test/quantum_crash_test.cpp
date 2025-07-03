// Test to reproduce quantum address generation crash

#include <boost/test/unit_test.hpp>
#include <test/util/setup_common.h>
#include <wallet/wallet.h>
#include <wallet/scriptpubkeyman.h>
#include <wallet/quantum_wallet_setup.h>
#include <crypto/quantum_key.h>
#include <logging.h>

BOOST_FIXTURE_TEST_SUITE(quantum_crash_test, TestingSetup)

BOOST_AUTO_TEST_CASE(test_quantum_address_generation_crash)
{
    BOOST_TEST_MESSAGE("Testing quantum address generation for crashes...");
    
    // Test 1: Basic quantum key generation
    try {
        for (int i = 0; i < 10; i++) {
            quantum::CQuantumKey qKey;
            qKey.MakeNewKey(quantum::KeyType::ML_DSA_65);
            BOOST_REQUIRE(qKey.IsValid());
            
            auto pubkey = qKey.GetPubKey();
            BOOST_TEST_MESSAGE("Iteration " << i << ": Generated ML-DSA key, pubkey size: " << pubkey.GetKeyData().size());
        }
        BOOST_TEST_MESSAGE("✓ Multiple ML-DSA key generation successful");
    } catch (const std::exception& e) {
        BOOST_FAIL("Exception during ML-DSA key generation: " << e.what());
    }
    
    // Test 2: SLH-DSA key generation
    try {
        for (int i = 0; i < 5; i++) {
            quantum::CQuantumKey qKey;
            qKey.MakeNewKey(quantum::KeyType::SLH_DSA_192F);
            BOOST_REQUIRE(qKey.IsValid());
            
            auto pubkey = qKey.GetPubKey();
            BOOST_TEST_MESSAGE("Iteration " << i << ": Generated SLH-DSA key, pubkey size: " << pubkey.GetKeyData().size());
        }
        BOOST_TEST_MESSAGE("✓ Multiple SLH-DSA key generation successful");
    } catch (const std::exception& e) {
        BOOST_FAIL("Exception during SLH-DSA key generation: " << e.what());
    }
    
    // Test 3: Descriptor creation
    try {
        quantum::CQuantumKey qKey;
        qKey.MakeNewKey(quantum::KeyType::ML_DSA_65);
        auto pubkey = qKey.GetPubKey();
        
        std::string pubkey_hex = HexStr(pubkey.GetKeyData());
        std::string desc_str = "qpkh(quantum:ml-dsa:" + pubkey_hex + ")";
        
        FlatSigningProvider keys;
        std::string error;
        auto parsed = Parse(desc_str, keys, error, false);
        
        BOOST_REQUIRE_MESSAGE(!parsed.empty() && parsed[0], "Descriptor parsing failed: " + error);
        BOOST_TEST_MESSAGE("✓ Descriptor parsed successfully");
        
        // Expand to get scripts
        FlatSigningProvider out;
        std::vector<CScript> scripts;
        parsed[0]->Expand(0, keys, scripts, out);
        
        BOOST_REQUIRE(!scripts.empty());
        BOOST_TEST_MESSAGE("✓ Descriptor expanded to scripts");
        
    } catch (const std::exception& e) {
        BOOST_FAIL("Exception during descriptor creation: " << e.what());
    }
    
    BOOST_TEST_MESSAGE("All tests passed without crashes!");
}

BOOST_AUTO_TEST_SUITE_END()