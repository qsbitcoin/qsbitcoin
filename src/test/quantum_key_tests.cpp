// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/quantum_key.h>
#include <crypto/oqs_wrapper.h>
#include <key.h>
#include <random.h>
#include <streams.h>
#include <test/util/random.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

#include <vector>

using namespace quantum;

BOOST_FIXTURE_TEST_SUITE(quantum_key_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(quantum_pubkey_ecdsa_conversion)
{
    // Test conversion between CPubKey and CQuantumPubKey for ECDSA
    CKey key;
    key.MakeNewKey(true);
    CPubKey pubkey = key.GetPubKey();
    
    // Convert to quantum pubkey
    CQuantumPubKey qpubkey(pubkey);
    BOOST_CHECK_EQUAL(qpubkey.GetType(), KeyType::ECDSA);
    BOOST_CHECK(qpubkey.IsValid());
    BOOST_CHECK_EQUAL(qpubkey.size(), pubkey.size());
    
    // Convert back
    CPubKey pubkey2;
    BOOST_CHECK(qpubkey.GetCPubKey(pubkey2));
    BOOST_CHECK(pubkey == pubkey2);
    
    // Check ID matches
    BOOST_CHECK(pubkey.GetID() == qpubkey.GetID());
}

BOOST_AUTO_TEST_CASE(quantum_key_ecdsa_compatibility)
{
    // Test that CQuantumKey can wrap CKey properly
    CKey key;
    key.MakeNewKey(true);
    
    CQuantumKey qkey(key);
    BOOST_CHECK(qkey.IsValid());
    BOOST_CHECK_EQUAL(qkey.GetType(), KeyType::ECDSA);
    
    // Test signing
    uint256 hash = m_rng.rand256();
    std::vector<unsigned char> sig1, sig2;
    
    BOOST_CHECK(key.Sign(hash, sig1));
    BOOST_CHECK(qkey.Sign(hash, sig2));
    BOOST_CHECK(sig1 == sig2);
    
    // Test verification
    CPubKey pubkey = key.GetPubKey();
    CQuantumPubKey qpubkey = qkey.GetPubKey();
    
    BOOST_CHECK(pubkey.Verify(hash, sig1));
    BOOST_CHECK(CQuantumKey::Verify(hash, sig2, qpubkey));
}

BOOST_AUTO_TEST_CASE(quantum_key_generation_ml_dsa)
{
    // Skip if ML-DSA is not available
    if (!OQSContext::IsAlgorithmAvailable("ML-DSA-65")) {
        BOOST_TEST_MESSAGE("ML-DSA-65 not available, skipping test");
        return;
    }
    
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    
    BOOST_CHECK(key.IsValid());
    BOOST_CHECK_EQUAL(key.GetType(), KeyType::ML_DSA_65);
    
    CQuantumPubKey pubkey = key.GetPubKey();
    BOOST_CHECK(pubkey.IsValid());
    BOOST_CHECK_EQUAL(pubkey.GetType(), KeyType::ML_DSA_65);
    BOOST_CHECK_EQUAL(pubkey.size(), 1952); // ML-DSA-65 public key size
    
    // Test signing and verification
    uint256 hash = m_rng.rand256();
    std::vector<unsigned char> sig;
    
    BOOST_CHECK(key.Sign(hash, sig));
    BOOST_CHECK(!sig.empty());
    BOOST_CHECK_LE(sig.size(), 3309); // ML-DSA-65 max signature size
    
    BOOST_CHECK(CQuantumKey::Verify(hash, sig, pubkey));
    
    // Test invalid signature
    sig[0] ^= 0x01;
    BOOST_CHECK(!CQuantumKey::Verify(hash, sig, pubkey));
}

BOOST_AUTO_TEST_CASE(quantum_key_generation_slh_dsa)
{
    // Skip if SLH-DSA is not available
    if (!OQSContext::IsAlgorithmAvailable("SPHINCS+-SHA2-192f-simple")) {
        BOOST_TEST_MESSAGE("SPHINCS+-SHA2-192f-simple not available, skipping test");
        return;
    }
    
    CQuantumKey key;
    key.MakeNewKey(KeyType::SLH_DSA_192F);
    
    BOOST_CHECK(key.IsValid());
    BOOST_CHECK_EQUAL(key.GetType(), KeyType::SLH_DSA_192F);
    
    CQuantumPubKey pubkey = key.GetPubKey();
    BOOST_CHECK(pubkey.IsValid());
    BOOST_CHECK_EQUAL(pubkey.GetType(), KeyType::SLH_DSA_192F);
    BOOST_CHECK_EQUAL(pubkey.size(), 48); // SPHINCS+ public key size
    
    // Test signing and verification
    uint256 hash = m_rng.rand256();
    std::vector<unsigned char> sig;
    
    BOOST_CHECK(key.Sign(hash, sig));
    BOOST_CHECK(!sig.empty());
    BOOST_CHECK_LE(sig.size(), 35664); // SPHINCS+ max signature size
    
    BOOST_CHECK(CQuantumKey::Verify(hash, sig, pubkey));
    
    // Test invalid signature
    sig[sig.size() - 1] ^= 0x01;
    BOOST_CHECK(!CQuantumKey::Verify(hash, sig, pubkey));
}

BOOST_AUTO_TEST_CASE(quantum_key_serialization)
{
    // Test key serialization and deserialization
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    
    CQuantumPubKey pubkey = key.GetPubKey();
    secure_vector privkey = key.GetPrivKeyData();
    
    // Load into new key
    CQuantumKey key2;
    BOOST_CHECK(key2.Load(privkey, pubkey));
    BOOST_CHECK(key2.IsValid());
    BOOST_CHECK_EQUAL(key2.GetType(), KeyType::ML_DSA_65);
    
    // Verify same public key
    CQuantumPubKey pubkey2 = key2.GetPubKey();
    BOOST_CHECK(pubkey == pubkey2);
    
    // Verify signatures match
    uint256 hash = m_rng.rand256();
    std::vector<unsigned char> sig1, sig2;
    
    BOOST_CHECK(key.Sign(hash, sig1));
    BOOST_CHECK(key2.Sign(hash, sig2));
    
    // Signatures may differ due to randomness, but both should verify
    BOOST_CHECK(CQuantumKey::Verify(hash, sig1, pubkey));
    BOOST_CHECK(CQuantumKey::Verify(hash, sig2, pubkey));
}

BOOST_AUTO_TEST_CASE(quantum_pubkey_serialization)
{
    // Test CQuantumPubKey serialization
    CQuantumKey key;
    key.MakeNewKey(KeyType::SLH_DSA_192F);
    CQuantumPubKey pubkey = key.GetPubKey();
    
    // Serialize
    DataStream ss{};
    ss << pubkey;
    
    // Deserialize
    CQuantumPubKey pubkey2;
    ss >> pubkey2;
    
    BOOST_CHECK(pubkey == pubkey2);
    BOOST_CHECK_EQUAL(pubkey2.GetType(), KeyType::SLH_DSA_192F);
    BOOST_CHECK(pubkey2.IsValid());
}

BOOST_AUTO_TEST_CASE(quantum_key_comparison)
{
    // Test comparison operators
    CQuantumKey key1, key2, key3;
    key1.MakeNewKey(KeyType::ML_DSA_65);
    key2.MakeNewKey(KeyType::ML_DSA_65);
    key3.MakeNewKey(KeyType::SLH_DSA_192F);
    
    CQuantumPubKey pubkey1 = key1.GetPubKey();
    CQuantumPubKey pubkey2 = key2.GetPubKey();
    CQuantumPubKey pubkey3 = key3.GetPubKey();
    
    // Different keys should be different
    BOOST_CHECK(pubkey1 != pubkey2);
    BOOST_CHECK(pubkey1 != pubkey3);
    BOOST_CHECK(pubkey2 != pubkey3);
    
    // Same key should be equal
    CQuantumPubKey pubkey1_copy = key1.GetPubKey();
    BOOST_CHECK(pubkey1 == pubkey1_copy);
    
    // Test ordering (for use in maps/sets)
    std::set<CQuantumPubKey> pubkey_set;
    pubkey_set.insert(pubkey1);
    pubkey_set.insert(pubkey2);
    pubkey_set.insert(pubkey3);
    BOOST_CHECK_EQUAL(pubkey_set.size(), 3);
}

BOOST_AUTO_TEST_CASE(quantum_key_invalid_operations)
{
    // Test operations on invalid keys
    CQuantumKey invalid_key;
    BOOST_CHECK(!invalid_key.IsValid());
    
    std::vector<unsigned char> sig;
    uint256 hash = m_rng.rand256();
    BOOST_CHECK(!invalid_key.Sign(hash, sig));
    
    CQuantumPubKey invalid_pubkey = invalid_key.GetPubKey();
    BOOST_CHECK(!invalid_pubkey.IsValid());
    
    // Test invalid pubkey operations
    CPubKey cpubkey;
    BOOST_CHECK(!invalid_pubkey.GetCPubKey(cpubkey));
    
    // Test verification with invalid keys
    CQuantumKey valid_key;
    valid_key.MakeNewKey(KeyType::ML_DSA_65);
    CQuantumPubKey valid_pubkey = valid_key.GetPubKey();
    
    BOOST_CHECK(valid_key.Sign(hash, sig));
    BOOST_CHECK(!CQuantumKey::Verify(hash, sig, invalid_pubkey));
    
    std::vector<unsigned char> empty_sig;
    BOOST_CHECK(!CQuantumKey::Verify(hash, empty_sig, valid_pubkey));
}

BOOST_AUTO_TEST_CASE(quantum_key_derivation)
{
    // Test BIP32 derivation (currently only supported for ECDSA)
    CKey master;
    master.MakeNewKey(true);
    CQuantumKey qmaster(master);
    
    ChainCode cc;
    GetRandBytes(cc);
    
    CQuantumKey child;
    ChainCode ccChild;
    BOOST_CHECK(qmaster.Derive(child, ccChild, 0, cc));
    BOOST_CHECK(child.IsValid());
    BOOST_CHECK_EQUAL(child.GetType(), KeyType::ECDSA);
    
    // Quantum key derivation should fail (not yet implemented)
    CQuantumKey qkey;
    qkey.MakeNewKey(KeyType::ML_DSA_65);
    CQuantumKey qchild;
    BOOST_CHECK(!qkey.Derive(qchild, ccChild, 0, cc));
}

BOOST_AUTO_TEST_CASE(extended_quantum_key)
{
    // Test extended key functionality
    CExtQuantumKey ext;
    ext.nDepth = 0;
    memset(ext.vchFingerprint, 0, 4);
    ext.nChild = 0;
    GetRandBytes(ext.chaincode);
    ext.key.MakeNewKey(KeyType::ECDSA);
    
    // Test encoding/decoding
    std::vector<unsigned char> code;
    ext.Encode(code);
    
    CExtQuantumKey ext2;
    BOOST_CHECK_NO_THROW(ext2.Decode(code));
    BOOST_CHECK_EQUAL(ext2.nDepth, ext.nDepth);
    BOOST_CHECK_EQUAL(ext2.nChild, ext.nChild);
    BOOST_CHECK(memcmp(ext2.vchFingerprint, ext.vchFingerprint, 4) == 0);
    BOOST_CHECK(ext2.chaincode == ext.chaincode);
    
    // Test derivation
    CExtQuantumKey child;
    BOOST_CHECK(ext.Derive(child, 1));
    BOOST_CHECK_EQUAL(child.nDepth, 1);
    BOOST_CHECK_EQUAL(child.nChild, 1);
}

BOOST_AUTO_TEST_SUITE_END()