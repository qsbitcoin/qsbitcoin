// Simple test for quantum descriptor parsing

#include <boost/test/unit_test.hpp>
#include <test/util/setup_common.h>
#include <script/descriptor.h>
#include <crypto/quantum_key.h>
#include <util/strencodings.h>

BOOST_FIXTURE_TEST_SUITE(quantum_simple_descriptor_test, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(test_quantum_descriptor_parsing)
{
    BOOST_TEST_MESSAGE("Testing quantum descriptor parsing...");
    
    // Create quantum key
    quantum::CQuantumKey qKey;
    qKey.MakeNewKey(quantum::KeyType::ML_DSA_65);
    BOOST_REQUIRE(qKey.IsValid());
    
    auto pubkey = qKey.GetPubKey();
    std::string pubkey_hex = HexStr(pubkey.GetKeyData());
    
    BOOST_TEST_MESSAGE("Created ML-DSA key, pubkey hex length: " << pubkey_hex.length());
    
    // Create descriptor string
    std::string desc_str = "qpkh(quantum:ml-dsa:" + pubkey_hex + ")";
    BOOST_TEST_MESSAGE("Descriptor string length: " << desc_str.length());
    
    // Parse descriptor multiple times
    for (int i = 0; i < 5; i++) {
        FlatSigningProvider keys;
        std::string error;
        auto parsed = Parse(desc_str, keys, error, false);
        
        BOOST_REQUIRE_MESSAGE(!parsed.empty() && parsed[0], 
            "Iteration " << i << " - Failed to parse descriptor: " + error);
        
        // Expand to get scripts
        FlatSigningProvider out;
        std::vector<CScript> scripts;
        parsed[0]->Expand(0, keys, scripts, out);
        
        BOOST_REQUIRE(!scripts.empty());
        BOOST_TEST_MESSAGE("Iteration " << i << " - Script size: " << scripts[0].size());
    }
    
    BOOST_TEST_MESSAGE("All iterations passed!");
}

BOOST_AUTO_TEST_SUITE_END()