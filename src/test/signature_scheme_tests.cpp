// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/signature_scheme.h>
#include <crypto/ecdsa_scheme.h>
#include <crypto/mldsa_scheme.h>
#include <crypto/slhdsa_scheme.h>
#include <key.h>
#include <pubkey.h>
#include <random.h>
#include <uint256.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

namespace quantum {

BOOST_FIXTURE_TEST_SUITE(signature_scheme_tests, BasicTestingSetup)

/**
 * Test the signature scheme registry
 */
BOOST_AUTO_TEST_CASE(signature_registry_test)
{
    auto& registry = SignatureSchemeRegistry::GetInstance();
    
    // ECDSA should always be available
    BOOST_CHECK(registry.IsSchemeRegistered(SignatureSchemeId::ECDSA));
    
    // Get ECDSA scheme
    const ISignatureScheme* ecdsa = registry.GetScheme(SignatureSchemeId::ECDSA);
    BOOST_REQUIRE(ecdsa != nullptr);
    BOOST_CHECK_EQUAL(ecdsa->GetName(), "ECDSA");
    BOOST_CHECK_EQUAL(ecdsa->IsQuantumSafe(), false);
    
    // Check if quantum schemes are registered (depends on liboqs build)
    if (registry.IsSchemeRegistered(SignatureSchemeId::ML_DSA)) {
        const ISignatureScheme* mldsa = registry.GetScheme(SignatureSchemeId::ML_DSA);
        BOOST_REQUIRE(mldsa != nullptr);
        BOOST_CHECK_EQUAL(mldsa->GetName(), "ML-DSA-65");
        BOOST_CHECK_EQUAL(mldsa->IsQuantumSafe(), true);
        BOOST_CHECK_EQUAL(mldsa->GetMaxSignatureSize(), 3309);
    }
    
    if (registry.IsSchemeRegistered(SignatureSchemeId::SLH_DSA)) {
        const ISignatureScheme* slhdsa = registry.GetScheme(SignatureSchemeId::SLH_DSA);
        BOOST_REQUIRE(slhdsa != nullptr);
        BOOST_CHECK_EQUAL(slhdsa->GetName(), "SLH-DSA-SHA2-192f");
        BOOST_CHECK_EQUAL(slhdsa->IsQuantumSafe(), true);
        BOOST_CHECK_EQUAL(slhdsa->GetMaxSignatureSize(), 35664);
    }
    
    // Test getting all registered schemes
    auto schemes = registry.GetRegisteredSchemes();
    BOOST_CHECK(!schemes.empty());
    BOOST_CHECK(std::find(schemes.begin(), schemes.end(), SignatureSchemeId::ECDSA) != schemes.end());
}

/**
 * Test ECDSA scheme wrapper
 */
BOOST_AUTO_TEST_CASE(ecdsa_scheme_test)
{
    ECDSAScheme ecdsa;
    
    // Test metadata
    BOOST_CHECK_EQUAL(ecdsa.GetSchemeId(), SignatureSchemeId::ECDSA);
    BOOST_CHECK_EQUAL(ecdsa.GetName(), "ECDSA");
    BOOST_CHECK_EQUAL(ecdsa.IsQuantumSafe(), false);
    BOOST_CHECK_EQUAL(ecdsa.GetMaxSignatureSize(), 72);
    BOOST_CHECK_EQUAL(ecdsa.GetPublicKeySize(), 33);
    BOOST_CHECK_EQUAL(ecdsa.GetPrivateKeySize(), 32);
    
    // Generate a key pair
    CKey key;
    key.MakeNewKey(true); // compressed
    CPubKey pubkey = key.GetPubKey();
    
    // Create a test message hash
    uint256 hash;
    GetRandBytes(hash);
    
    // Sign the hash
    std::vector<unsigned char> sig;
    BOOST_CHECK(ecdsa.Sign(hash, key, sig));
    BOOST_CHECK(!sig.empty());
    BOOST_CHECK(sig.size() <= ecdsa.GetMaxSignatureSize());
    
    // Verify the signature
    BOOST_CHECK(ecdsa.Verify(hash, pubkey, sig));
    
    // Verify with wrong hash should fail
    uint256 wrong_hash;
    GetRandBytes(wrong_hash);
    BOOST_CHECK(!ecdsa.Verify(wrong_hash, pubkey, sig));
    
    // Verify with wrong pubkey should fail
    CKey wrong_key;
    wrong_key.MakeNewKey(true);
    CPubKey wrong_pubkey = wrong_key.GetPubKey();
    BOOST_CHECK(!ecdsa.Verify(hash, wrong_pubkey, sig));
    
    // Verify with corrupted signature should fail
    if (!sig.empty()) {
        sig[0] ^= 0x01; // Flip a bit
        BOOST_CHECK(!ecdsa.Verify(hash, pubkey, sig));
    }
}

/**
 * Test invalid key handling
 */
BOOST_AUTO_TEST_CASE(invalid_key_test)
{
    ECDSAScheme ecdsa;
    
    // Invalid private key
    CKey invalid_key;
    uint256 hash;
    GetRandBytes(hash);
    std::vector<unsigned char> sig;
    
    BOOST_CHECK(!ecdsa.Sign(hash, invalid_key, sig));
    
    // Invalid public key
    CPubKey invalid_pubkey;
    std::vector<unsigned char> valid_sig{0x30, 0x44}; // Minimal DER signature
    BOOST_CHECK(!ecdsa.Verify(hash, invalid_pubkey, valid_sig));
}

/**
 * Test ML-DSA scheme (if available)
 */
BOOST_AUTO_TEST_CASE(mldsa_scheme_test)
{
    auto& registry = SignatureSchemeRegistry::GetInstance();
    
    if (!registry.IsSchemeRegistered(SignatureSchemeId::ML_DSA)) {
        BOOST_TEST_MESSAGE("ML-DSA not available, skipping test");
        return;
    }
    
    const ISignatureScheme* mldsa = registry.GetScheme(SignatureSchemeId::ML_DSA);
    BOOST_REQUIRE(mldsa != nullptr);
    
    // Test metadata
    BOOST_CHECK_EQUAL(mldsa->GetSchemeId(), SignatureSchemeId::ML_DSA);
    BOOST_CHECK_EQUAL(mldsa->GetName(), "ML-DSA-65");
    BOOST_CHECK_EQUAL(mldsa->IsQuantumSafe(), true);
    BOOST_CHECK_EQUAL(mldsa->GetMaxSignatureSize(), 3309);
    BOOST_CHECK_EQUAL(mldsa->GetPublicKeySize(), 1952);
    BOOST_CHECK_EQUAL(mldsa->GetPrivateKeySize(), 4032);
    
    // TODO: Once CKey is extended to support quantum keys, add full signing/verification tests
    // For now, we can only test the temporary implementation
    
    CKey temp_key;
    temp_key.MakeNewKey(true);
    uint256 hash;
    GetRandBytes(hash);
    std::vector<unsigned char> sig;
    
    // The temporary implementation can sign but not verify
    BOOST_CHECK(mldsa->Sign(hash, temp_key, sig));
    BOOST_CHECK(!sig.empty());
    BOOST_CHECK(sig.size() <= mldsa->GetMaxSignatureSize());
}

/**
 * Test SLH-DSA scheme (if available)
 */
BOOST_AUTO_TEST_CASE(slhdsa_scheme_test)
{
    auto& registry = SignatureSchemeRegistry::GetInstance();
    
    if (!registry.IsSchemeRegistered(SignatureSchemeId::SLH_DSA)) {
        BOOST_TEST_MESSAGE("SLH-DSA not available, skipping test");
        return;
    }
    
    const ISignatureScheme* slhdsa = registry.GetScheme(SignatureSchemeId::SLH_DSA);
    BOOST_REQUIRE(slhdsa != nullptr);
    
    // Test metadata
    BOOST_CHECK_EQUAL(slhdsa->GetSchemeId(), SignatureSchemeId::SLH_DSA);
    BOOST_CHECK_EQUAL(slhdsa->GetName(), "SLH-DSA-SHA2-192f");
    BOOST_CHECK_EQUAL(slhdsa->IsQuantumSafe(), true);
    BOOST_CHECK_EQUAL(slhdsa->GetMaxSignatureSize(), 35664);
    BOOST_CHECK_EQUAL(slhdsa->GetPublicKeySize(), 48);
    BOOST_CHECK_EQUAL(slhdsa->GetPrivateKeySize(), 96);
    
    // TODO: Once CKey is extended to support quantum keys, add full signing/verification tests
    // For now, we can only test the temporary implementation
    
    CKey temp_key;
    temp_key.MakeNewKey(true);
    uint256 hash;
    GetRandBytes(hash);
    std::vector<unsigned char> sig;
    
    // The temporary implementation can sign but not verify
    BOOST_CHECK(slhdsa->Sign(hash, temp_key, sig));
    BOOST_CHECK(!sig.empty());
    BOOST_CHECK(sig.size() <= slhdsa->GetMaxSignatureSize());
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace quantum