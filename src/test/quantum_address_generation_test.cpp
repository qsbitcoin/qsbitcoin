// Test quantum address generation

#include <boost/test/unit_test.hpp>
#include <test/util/setup_common.h>
#include <script/descriptor.h>
#include <script/signingprovider.h>
#include <crypto/quantum_key.h>
#include <key_io.h>
#include <util/strencodings.h>
#include <wallet/scriptpubkeyman.h>
#include <util/chaintype.h>

BOOST_FIXTURE_TEST_SUITE(quantum_address_generation_test, RegTestingSetup)

BOOST_AUTO_TEST_CASE(test_qpkh_descriptor_creates_p2wsh)
{
    // Create a quantum key
    quantum::CQuantumKey qKey;
    qKey.MakeNewKey(quantum::KeyType::ML_DSA_65);
    BOOST_REQUIRE(qKey.IsValid());
    
    quantum::CQuantumPubKey qPubKey = qKey.GetPubKey();
    std::vector<unsigned char> pubkey_data = qPubKey.GetKeyData();
    std::string pubkey_hex = HexStr(pubkey_data);
    
    BOOST_TEST_MESSAGE("Created ML-DSA key, pubkey size: " << pubkey_data.size());
    
    // Create descriptor string
    std::string desc_str = "qpkh(quantum:ml-dsa:" + pubkey_hex + ")";
    BOOST_TEST_MESSAGE("Descriptor: " << desc_str.substr(0, 50) << "...");
    
    // Parse descriptor
    FlatSigningProvider keys;
    std::string error;
    auto parsed = Parse(desc_str, keys, error, false);
    
    BOOST_REQUIRE_MESSAGE(!parsed.empty() && parsed[0], "Failed to parse descriptor: " + error);
    
    // Expand descriptor to get scripts
    FlatSigningProvider out;
    std::vector<CScript> scripts;
    parsed[0]->Expand(0, keys, scripts, out);
    
    BOOST_REQUIRE(!scripts.empty());
    
    // Check the generated script
    const CScript& script = scripts[0];
    BOOST_TEST_MESSAGE("Generated script size: " << script.size());
    BOOST_TEST_MESSAGE("Script hex: " << HexStr(script));
    
    // Parse the script to check its structure
    if (script.size() >= 2) {
        // Should be OP_0 followed by 32-byte hash for P2WSH
        opcodetype opcode;
        std::vector<unsigned char> data;
        CScript::const_iterator pc = script.begin();
        
        BOOST_REQUIRE(script.GetOp(pc, opcode, data));
        BOOST_CHECK_EQUAL(opcode, OP_0);
        BOOST_TEST_MESSAGE("First opcode: " << (int)opcode << " (should be 0 for OP_0)");
        
        BOOST_REQUIRE(script.GetOp(pc, opcode, data));
        BOOST_CHECK_EQUAL(data.size(), 32); // P2WSH uses 32-byte hash
        BOOST_TEST_MESSAGE("Witness program size: " << data.size() << " bytes (should be 32 for P2WSH)");
        
        // Create an address from the script
        CTxDestination dest;
        bool extracted = ExtractDestination(script, dest);
        BOOST_REQUIRE(extracted);
        
        std::string address = EncodeDestination(dest);
        BOOST_TEST_MESSAGE("Generated address: " << address);
        
        // The address should start with bcrt1q (regtest bech32)
        BOOST_TEST_MESSAGE("Address prefix check: " << address.substr(0, 6) << " == bcrt1q");
        BOOST_CHECK_MESSAGE(address.substr(0, 6) == "bcrt1q", "Address does not start with bcrt1q, it starts with: " + address.substr(0, 10));
        
        // Decode the address to verify it's P2WSH
        // Bech32 addresses have witness version and program embedded
        // P2WSH will have 32-byte program (64 hex chars)
        // P2WPKH will have 20-byte program (40 hex chars)
    }
}

BOOST_AUTO_TEST_SUITE_END()