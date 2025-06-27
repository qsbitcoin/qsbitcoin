// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/quantum_key.h>
#include <crypto/quantum_hd.h>
#include <random.h>
#include <test/util/random.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

using namespace quantum;

BOOST_FIXTURE_TEST_SUITE(quantum_hd_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(quantum_hd_basic_derivation)
{
    // Test basic HD derivation for quantum keys
    
    // Generate master key
    CQuantumKey master;
    master.MakeNewKey(KeyType::ML_DSA_65);
    BOOST_CHECK(master.IsValid());
    
    // Create a chain code
    ChainCode cc = m_rng.rand256();
    
    // Derive child key
    CQuantumKey child;
    ChainCode ccChild;
    unsigned int nChild = 0;
    
    BOOST_CHECK(master.Derive(child, ccChild, nChild, cc));
    BOOST_CHECK(child.IsValid());
    BOOST_CHECK_EQUAL(child.GetType(), KeyType::ML_DSA_65);
    
    // Child should be different from parent
    CQuantumPubKey masterPub = master.GetPubKey();
    CQuantumPubKey childPub = child.GetPubKey();
    BOOST_CHECK(masterPub != childPub);
    
    // Test signing with derived key
    uint256 hash = m_rng.rand256();
    std::vector<unsigned char> sig;
    BOOST_CHECK(child.Sign(hash, sig));
    BOOST_CHECK(CQuantumKey::Verify(hash, sig, childPub));
}

BOOST_AUTO_TEST_CASE(quantum_hd_multiple_children)
{
    // Test deriving multiple children
    CQuantumKey master;
    master.MakeNewKey(KeyType::SLH_DSA_192F);
    BOOST_CHECK(master.IsValid());
    
    ChainCode cc = m_rng.rand256();
    
    // Derive multiple children
    std::vector<CQuantumPubKey> childPubkeys;
    for (unsigned int i = 0; i < 5; ++i) {
        CQuantumKey child;
        ChainCode ccChild;
        
        BOOST_CHECK(master.Derive(child, ccChild, i, cc));
        BOOST_CHECK(child.IsValid());
        
        CQuantumPubKey pubkey = child.GetPubKey();
        
        // Verify all children are unique
        for (const auto& existing : childPubkeys) {
            BOOST_CHECK(existing != pubkey);
        }
        
        childPubkeys.push_back(pubkey);
    }
}

BOOST_AUTO_TEST_CASE(quantum_hd_hardened_derivation)
{
    // Test hardened derivation (index >= 0x80000000)
    CQuantumKey master;
    master.MakeNewKey(KeyType::ML_DSA_65);
    BOOST_CHECK(master.IsValid());
    
    ChainCode cc = m_rng.rand256();
    
    // Derive hardened child
    CQuantumKey child;
    ChainCode ccChild;
    unsigned int nChild = 0x80000000; // First hardened key
    
    BOOST_CHECK(master.Derive(child, ccChild, nChild, cc));
    BOOST_CHECK(child.IsValid());
}

BOOST_AUTO_TEST_CASE(quantum_hd_chain_derivation)
{
    // Test deriving a chain of keys (grandchildren)
    CQuantumKey master;
    master.MakeNewKey(KeyType::ML_DSA_65);
    BOOST_CHECK(master.IsValid());
    
    ChainCode cc0 = m_rng.rand256();
    
    // Derive child
    CQuantumKey child;
    ChainCode cc1;
    BOOST_CHECK(master.Derive(child, cc1, 0, cc0));
    
    // Derive grandchild
    CQuantumKey grandchild;
    ChainCode cc2;
    BOOST_CHECK(child.Derive(grandchild, cc2, 1, cc1));
    BOOST_CHECK(grandchild.IsValid());
    
    // All keys should be different
    CQuantumPubKey masterPub = master.GetPubKey();
    CQuantumPubKey childPub = child.GetPubKey();
    CQuantumPubKey grandchildPub = grandchild.GetPubKey();
    
    BOOST_CHECK(masterPub != childPub);
    BOOST_CHECK(childPub != grandchildPub);
    BOOST_CHECK(masterPub != grandchildPub);
}

BOOST_AUTO_TEST_CASE(quantum_hd_master_generation)
{
    // Test master key generation from seed
    std::vector<std::byte> seed(32);
    for (size_t i = 0; i < seed.size(); ++i) {
        seed[i] = static_cast<std::byte>(i);
    }
    
    CQuantumKey master;
    ChainCode ccMaster;
    
    BOOST_CHECK(GenerateQuantumMaster(master, ccMaster, seed, KeyType::ML_DSA_65));
    BOOST_CHECK(master.IsValid());
    BOOST_CHECK_EQUAL(master.GetType(), KeyType::ML_DSA_65);
    
    // Test that we can derive from master
    CQuantumKey child;
    ChainCode ccChild;
    BOOST_CHECK(master.Derive(child, ccChild, 0, ccMaster));
}

BOOST_AUTO_TEST_CASE(quantum_hd_fingerprint)
{
    // Test fingerprint calculation
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    
    CQuantumPubKey pubkey = key.GetPubKey();
    unsigned char fingerprint[4];
    
    GetQuantumKeyFingerprint(pubkey, fingerprint);
    
    // Fingerprint should be first 4 bytes of key ID
    CKeyID id = pubkey.GetID();
    BOOST_CHECK_EQUAL(memcmp(fingerprint, &id, 4), 0);
}

BOOST_AUTO_TEST_CASE(quantum_hd_extended_key)
{
    // Test extended key functionality
    CExtQuantumKey ext;
    ext.nDepth = 0;
    memset(ext.vchFingerprint, 0, 4);
    ext.nChild = 0;
    ext.chaincode = m_rng.rand256();
    ext.key.MakeNewKey(KeyType::ML_DSA_65);
    
    // Derive extended child
    CExtQuantumKey extChild;
    BOOST_CHECK(ext.Derive(extChild, 1));
    
    BOOST_CHECK_EQUAL(extChild.nDepth, 1);
    BOOST_CHECK_EQUAL(extChild.nChild, 1);
    BOOST_CHECK(extChild.key.IsValid());
    
    // Fingerprint should match parent's key ID
    CKeyID parentId = ext.key.GetPubKey().GetID();
    BOOST_CHECK_EQUAL(memcmp(extChild.vchFingerprint, &parentId, 4), 0);
}

BOOST_AUTO_TEST_SUITE_END()