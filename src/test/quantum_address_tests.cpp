// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <quantum_address.h>
#include <crypto/quantum_key.h>
#include <base58.h>
#include <test/util/random.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

using namespace quantum;

BOOST_FIXTURE_TEST_SUITE(quantum_address_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(quantum_address_encode_decode)
{
    // Test address encoding and decoding
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    CQuantumPubKey pubkey = key.GetPubKey();
    
    // Encode address
    std::string address = EncodeQuantumAddress(pubkey);
    BOOST_CHECK(!address.empty());
    // Note: The first character depends on the Base58 encoding of version byte 0x51
    // We'll check it's a valid address instead of checking the first character
    
    // Decode address
    QuantumAddressType addrType;
    uint256 hash;
    BOOST_CHECK(DecodeQuantumAddress(address, addrType, hash));
    BOOST_CHECK_EQUAL(addrType, QuantumAddressType::P2QPKH_ML_DSA);
    
    // Verify hash matches
    uint256 expectedHash = QuantumHash256(pubkey.GetKeyData());
    BOOST_CHECK(hash == expectedHash);
}

BOOST_AUTO_TEST_CASE(quantum_address_types)
{
    // Test different address types
    CQuantumKey key1;
    key1.MakeNewKey(KeyType::ML_DSA_65);
    CQuantumPubKey pubkey1 = key1.GetPubKey();
    
    CQuantumKey key2;
    key2.MakeNewKey(KeyType::SLH_DSA_192F);
    CQuantumPubKey pubkey2 = key2.GetPubKey();
    
    // ML-DSA address
    std::string addr1 = EncodeQuantumAddress(pubkey1);
    BOOST_CHECK(!addr1.empty());
    
    QuantumAddressType type1;
    uint256 hash1;
    BOOST_CHECK(DecodeQuantumAddress(addr1, type1, hash1));
    BOOST_CHECK_EQUAL(type1, QuantumAddressType::P2QPKH_ML_DSA);
    
    // SLH-DSA address
    std::string addr2 = EncodeQuantumAddress(pubkey2);
    BOOST_CHECK(!addr2.empty());
    BOOST_CHECK(addr1 != addr2); // Different addresses
    
    QuantumAddressType type2;
    uint256 hash2;
    BOOST_CHECK(DecodeQuantumAddress(addr2, type2, hash2));
    BOOST_CHECK_EQUAL(type2, QuantumAddressType::P2QPKH_SLH_DSA);
}

BOOST_AUTO_TEST_CASE(quantum_script_creation)
{
    // Test P2QPKH script creation
    uint256 pubkeyHash = m_rng.rand256();
    
    // ML-DSA script
    CScript script1 = CreateP2QPKHScript(pubkeyHash, KeyType::ML_DSA_65);
    BOOST_CHECK(!script1.empty());
    
    // Extract address from script
    QuantumAddressType addrType;
    uint256 extractedHash;
    BOOST_CHECK(ExtractQuantumAddress(script1, addrType, extractedHash));
    BOOST_CHECK_EQUAL(addrType, QuantumAddressType::P2QPKH_ML_DSA);
    BOOST_CHECK(extractedHash == pubkeyHash);
    
    // SLH-DSA script
    CScript script2 = CreateP2QPKHScript(pubkeyHash, KeyType::SLH_DSA_192F);
    BOOST_CHECK(!script2.empty());
    BOOST_CHECK(script1 != script2); // Different scripts for different key types
}

BOOST_AUTO_TEST_CASE(quantum_p2qsh_script)
{
    // Test P2QSH script creation
    uint256 scriptHash = m_rng.rand256();
    
    CScript script = CreateP2QSHScript(scriptHash);
    BOOST_CHECK(!script.empty());
    
    // Extract address from script
    QuantumAddressType addrType;
    uint256 extractedHash;
    BOOST_CHECK(ExtractQuantumAddress(script, addrType, extractedHash));
    BOOST_CHECK_EQUAL(addrType, QuantumAddressType::P2QSH);
    BOOST_CHECK(extractedHash == scriptHash);
}

BOOST_AUTO_TEST_CASE(quantum_address_validation)
{
    // Test address validation
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    CQuantumPubKey pubkey = key.GetPubKey();
    
    std::string validAddress = EncodeQuantumAddress(pubkey);
    BOOST_CHECK(IsValidQuantumAddress(validAddress));
    
    // Test invalid addresses
    BOOST_CHECK(!IsValidQuantumAddress(""));
    BOOST_CHECK(!IsValidQuantumAddress("InvalidAddress"));
    BOOST_CHECK(!IsValidQuantumAddress("Q")); // Too short
    
    // Corrupt the address
    std::string corrupted = validAddress;
    if (!corrupted.empty()) {
        corrupted[corrupted.length() - 1] = (corrupted[corrupted.length() - 1] == 'A') ? 'B' : 'A';
        BOOST_CHECK(!IsValidQuantumAddress(corrupted)); // Should fail checksum
    }
}

BOOST_AUTO_TEST_CASE(quantum_hash_function)
{
    // Test quantum hash function
    std::vector<unsigned char> data1 = {1, 2, 3, 4, 5};
    std::vector<unsigned char> data2 = {1, 2, 3, 4, 6}; // Different last byte
    
    uint256 hash1 = QuantumHash256(data1);
    uint256 hash2 = QuantumHash256(data2);
    
    // Different inputs should produce different hashes
    BOOST_CHECK(hash1 != hash2);
    
    // Same input should produce same hash
    uint256 hash1_again = QuantumHash256(data1);
    BOOST_CHECK(hash1 == hash1_again);
}

BOOST_AUTO_TEST_CASE(quantum_witness_program)
{
    // Test witness program creation
    uint256 pubkeyHash = m_rng.rand256();
    
    CScript witnessProgram = CreateQuantumWitnessProgram(0, KeyType::ML_DSA_65, pubkeyHash);
    BOOST_CHECK(!witnessProgram.empty());
    BOOST_CHECK_EQUAL(witnessProgram.size(), 34); // OP_0 + 32 bytes
    
    // Check structure
    std::vector<unsigned char> scriptData(witnessProgram.begin(), witnessProgram.end());
    BOOST_CHECK_EQUAL(scriptData[0], OP_0);
    BOOST_CHECK_EQUAL(scriptData[1], 32); // Push 32 bytes
}

BOOST_AUTO_TEST_CASE(quantum_address_type_strings)
{
    // Test address type string conversion
    BOOST_CHECK_EQUAL(GetQuantumAddressTypeString(QuantumAddressType::P2QPKH_ML_DSA), "P2QPKH-ML-DSA");
    BOOST_CHECK_EQUAL(GetQuantumAddressTypeString(QuantumAddressType::P2QPKH_SLH_DSA), "P2QPKH-SLH-DSA");
    BOOST_CHECK_EQUAL(GetQuantumAddressTypeString(QuantumAddressType::P2QSH), "P2QSH");
    
    // Test key type extraction
    BOOST_CHECK_EQUAL(GetKeyTypeForAddress(QuantumAddressType::P2QPKH_ML_DSA), KeyType::ML_DSA_65);
    BOOST_CHECK_EQUAL(GetKeyTypeForAddress(QuantumAddressType::P2QPKH_SLH_DSA), KeyType::SLH_DSA_192F);
}

BOOST_AUTO_TEST_CASE(quantum_address_round_trip)
{
    // Test full round trip: key -> address -> script -> address
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    CQuantumPubKey pubkey = key.GetPubKey();
    
    // Create address
    std::string address = EncodeQuantumAddress(pubkey);
    BOOST_CHECK(!address.empty());
    
    // Decode to get hash
    QuantumAddressType addrType;
    uint256 pubkeyHash;
    BOOST_CHECK(DecodeQuantumAddress(address, addrType, pubkeyHash));
    
    // Create script from hash
    CScript script = CreateP2QPKHScript(pubkeyHash, GetKeyTypeForAddress(addrType));
    
    // Extract back from script
    QuantumAddressType extractedType;
    uint256 extractedHash;
    BOOST_CHECK(ExtractQuantumAddress(script, extractedType, extractedHash));
    
    // Verify everything matches
    BOOST_CHECK_EQUAL(extractedType, addrType);
    BOOST_CHECK(extractedHash == pubkeyHash);
}

BOOST_AUTO_TEST_SUITE_END()