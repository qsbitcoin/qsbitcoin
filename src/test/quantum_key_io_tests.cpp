// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/quantum_key.h>
#include <crypto/quantum_key_io.h>
#include <test/util/random.h>
#include <test/util/setup_common.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

using namespace quantum;

BOOST_FIXTURE_TEST_SUITE(quantum_key_io_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(quantum_key_export_import_hex)
{
    // Test hex format export/import
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    BOOST_CHECK(key.IsValid());
    
    // Export as hex
    std::string exported = ExportQuantumKey(key, ExportFormat::HEX);
    BOOST_CHECK(!exported.empty());
    // New format: 1 byte version + 2 bytes pubkey size + 1952 bytes pubkey + 4032 bytes privkey = 5987 bytes
    BOOST_CHECK_EQUAL(exported.length(), (1 + 2 + 1952 + 4032) * 2); // 5987 bytes in hex
    
    // Import from hex
    CQuantumKey imported;
    BOOST_CHECK(ImportQuantumKey(imported, exported, ExportFormat::HEX));
    BOOST_CHECK(imported.IsValid());
    BOOST_CHECK_EQUAL(imported.GetType(), KeyType::ML_DSA_65);
    
    // Check if private keys are the same
    secure_vector privkey1 = key.GetPrivKeyData();
    secure_vector privkey2 = imported.GetPrivKeyData();
    BOOST_CHECK_MESSAGE(privkey1 == privkey2, 
        "Private keys should be equal. Sizes: " << privkey1.size() 
        << " vs " << privkey2.size());
    
    // Verify they produce the same signatures
    uint256 hash = m_rng.rand256();
    std::vector<unsigned char> sig1, sig2;
    
    BOOST_CHECK(key.Sign(hash, sig1));
    BOOST_CHECK(imported.Sign(hash, sig2));
    
    // Both signatures should verify with both public keys
    CQuantumPubKey pubkey1 = key.GetPubKey();
    CQuantumPubKey pubkey2 = imported.GetPubKey();
    
    // Debug: Check if public keys are equal
    BOOST_CHECK_MESSAGE(pubkey1 == pubkey2, 
        "Public keys should be equal. Sizes: " << pubkey1.GetKeyData().size() 
        << " vs " << pubkey2.GetKeyData().size());
    
    BOOST_CHECK(CQuantumKey::Verify(hash, sig1, pubkey1));
    BOOST_CHECK(CQuantumKey::Verify(hash, sig2, pubkey2));
    
    // Cross-verify: original sig with imported pubkey and vice versa
    BOOST_CHECK_MESSAGE(CQuantumKey::Verify(hash, sig1, pubkey2), 
        "Original signature should verify with imported public key");
    BOOST_CHECK_MESSAGE(CQuantumKey::Verify(hash, sig2, pubkey1),
        "Imported signature should verify with original public key");
}

BOOST_AUTO_TEST_CASE(quantum_key_export_import_base64)
{
    // Test base64 format
    CQuantumKey key;
    key.MakeNewKey(KeyType::SLH_DSA_192F);
    BOOST_CHECK(key.IsValid());
    
    // Export as base64
    std::string exported = ExportQuantumKey(key, ExportFormat::BASE64);
    BOOST_CHECK(!exported.empty());
    
    // Import from base64
    CQuantumKey imported;
    BOOST_CHECK(ImportQuantumKey(imported, exported, ExportFormat::BASE64));
    BOOST_CHECK(imported.IsValid());
    BOOST_CHECK_EQUAL(imported.GetType(), KeyType::SLH_DSA_192F);
}

BOOST_AUTO_TEST_CASE(quantum_key_export_import_armored)
{
    // Test armored format
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    BOOST_CHECK(key.IsValid());
    
    // Export as armored
    std::string exported = ExportQuantumKey(key, ExportFormat::ARMORED);
    BOOST_CHECK(!exported.empty());
    
    // Check format
    BOOST_CHECK(exported.find("-----BEGIN QUANTUM PRIVATE KEY-----") != std::string::npos);
    BOOST_CHECK(exported.find("-----END QUANTUM PRIVATE KEY-----") != std::string::npos);
    BOOST_CHECK(exported.find("Type: ML-DSA-65") != std::string::npos);
    BOOST_CHECK(exported.find("Checksum: ") != std::string::npos);
    
    // Import from armored
    CQuantumKey imported;
    BOOST_CHECK(ImportQuantumKey(imported, exported, ExportFormat::ARMORED));
    BOOST_CHECK(imported.IsValid());
    BOOST_CHECK_EQUAL(imported.GetType(), KeyType::ML_DSA_65);
}

BOOST_AUTO_TEST_CASE(quantum_pubkey_export_import)
{
    // Test public key export/import
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    CQuantumPubKey pubkey = key.GetPubKey();
    
    // Test hex format
    std::string hexExported = ExportQuantumPubKey(pubkey, ExportFormat::HEX);
    BOOST_CHECK_EQUAL(hexExported.length(), 1952 * 2);
    
    CQuantumPubKey imported;
    BOOST_CHECK(ImportQuantumPubKey(imported, hexExported, ExportFormat::HEX));
    BOOST_CHECK(imported == pubkey);
    
    // Test base64 format
    std::string base64Exported = ExportQuantumPubKey(pubkey, ExportFormat::BASE64);
    BOOST_CHECK(!base64Exported.empty());
    
    CQuantumPubKey imported2;
    BOOST_CHECK(ImportQuantumPubKey(imported2, base64Exported, ExportFormat::BASE64));
    BOOST_CHECK(imported2 == pubkey);
    
    // Test armored format
    std::string armoredExported = ExportQuantumPubKey(pubkey, ExportFormat::ARMORED);
    BOOST_CHECK(armoredExported.find("-----BEGIN QUANTUM PUBLIC KEY-----") != std::string::npos);
    BOOST_CHECK(armoredExported.find("-----END QUANTUM PUBLIC KEY-----") != std::string::npos);
    
    CQuantumPubKey imported3;
    BOOST_CHECK(ImportQuantumPubKey(imported3, armoredExported, ExportFormat::ARMORED));
    BOOST_CHECK(imported3 == pubkey);
}

BOOST_AUTO_TEST_CASE(quantum_key_fingerprint)
{
    // Test fingerprint generation
    CQuantumKey key1;
    key1.MakeNewKey(KeyType::ML_DSA_65);
    CQuantumPubKey pubkey1 = key1.GetPubKey();
    
    std::string fingerprint1 = GetQuantumKeyFingerprint(pubkey1);
    BOOST_CHECK(!fingerprint1.empty());
    BOOST_CHECK(fingerprint1.find("ML-DSA-65:") == 0);
    
    // Different key should have different fingerprint
    CQuantumKey key2;
    key2.MakeNewKey(KeyType::ML_DSA_65);
    CQuantumPubKey pubkey2 = key2.GetPubKey();
    
    std::string fingerprint2 = GetQuantumKeyFingerprint(pubkey2);
    BOOST_CHECK(fingerprint1 != fingerprint2);
    
    // Test SPHINCS+ fingerprint
    CQuantumKey key3;
    key3.MakeNewKey(KeyType::SLH_DSA_192F);
    CQuantumPubKey pubkey3 = key3.GetPubKey();
    
    std::string fingerprint3 = GetQuantumKeyFingerprint(pubkey3);
    BOOST_CHECK(fingerprint3.find("SLH-DSA-SHA2-192F:") == 0);
}

BOOST_AUTO_TEST_CASE(quantum_key_import_invalid)
{
    // Test importing invalid data
    CQuantumKey key;
    
    // Empty data
    BOOST_CHECK(!ImportQuantumKey(key, "", ExportFormat::HEX));
    BOOST_CHECK(!ImportQuantumKey(key, "", ExportFormat::BASE64));
    
    // Invalid hex
    BOOST_CHECK(!ImportQuantumKey(key, "not hex", ExportFormat::HEX));
    
    // Invalid base64
    BOOST_CHECK(!ImportQuantumKey(key, "not@base64!", ExportFormat::BASE64));
    
    // Wrong size for key type
    std::vector<unsigned char> wrongSize(100); // Not a valid key size
    BOOST_CHECK(!ImportQuantumKey(key, HexStr(wrongSize), ExportFormat::HEX));
    
    // Corrupted armored format
    std::string corrupted = "-----BEGIN QUANTUM PRIVATE KEY-----\nType: ML-DSA-65\nChecksum: 12345678\n\nAAA\n-----END QUANTUM PRIVATE KEY-----";
    BOOST_CHECK(!ImportQuantumKey(key, corrupted, ExportFormat::ARMORED));
}

BOOST_AUTO_TEST_CASE(quantum_key_armored_checksum)
{
    // Test that armored format detects corrupted data
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    
    std::string exported = ExportQuantumKey(key, ExportFormat::ARMORED);
    
    // Corrupt the data by changing a character
    size_t dataStart = exported.find("\n\n") + 2;
    size_t dataEnd = exported.find("-----END");
    if (dataStart < exported.length() && dataEnd != std::string::npos) {
        exported[dataStart + 10] = (exported[dataStart + 10] == 'A') ? 'B' : 'A';
    }
    
    // Import should fail due to checksum mismatch
    CQuantumKey imported;
    BOOST_CHECK(!ImportQuantumKey(imported, exported, ExportFormat::ARMORED));
}

BOOST_AUTO_TEST_CASE(quantum_key_round_trip_all_types)
{
    // Test export/import for all key types
    std::vector<KeyType> keyTypes = {KeyType::ML_DSA_65, KeyType::SLH_DSA_192F};
    std::vector<ExportFormat> formats = {ExportFormat::HEX, ExportFormat::BASE64, ExportFormat::ARMORED};
    
    for (KeyType keyType : keyTypes) {
        for (ExportFormat format : formats) {
            CQuantumKey original;
            original.MakeNewKey(keyType);
            BOOST_CHECK(original.IsValid());
            
            // Export
            std::string exported = ExportQuantumKey(original, format);
            BOOST_CHECK(!exported.empty());
            
            // Import
            CQuantumKey imported;
            BOOST_CHECK(ImportQuantumKey(imported, exported, format));
            BOOST_CHECK(imported.IsValid());
            BOOST_CHECK_EQUAL(imported.GetType(), keyType);
            
            // Verify functionality
            uint256 hash = m_rng.rand256();
            std::vector<unsigned char> sig;
            BOOST_CHECK(imported.Sign(hash, sig));
            
            CQuantumPubKey pubkey = imported.GetPubKey();
            BOOST_CHECK(CQuantumKey::Verify(hash, sig, pubkey));
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()