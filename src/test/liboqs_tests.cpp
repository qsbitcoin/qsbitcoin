// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/oqs_wrapper.h>
#include <random.h>
#include <uint256.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

namespace quantum {

BOOST_FIXTURE_TEST_SUITE(liboqs_tests, BasicTestingSetup)

/**
 * Test OQS algorithm availability
 */
BOOST_AUTO_TEST_CASE(oqs_algorithm_availability)
{
    // Check that our required algorithms are available
    BOOST_CHECK(OQSContext::IsAlgorithmAvailable("ML-DSA-65"));
    BOOST_CHECK(OQSContext::IsAlgorithmAvailable("SPHINCS+-SHA2-192f-simple"));
    
    // Check that disabled algorithms are not available
    BOOST_CHECK(!OQSContext::IsAlgorithmAvailable("Kyber512"));
    BOOST_CHECK(!OQSContext::IsAlgorithmAvailable("Dilithium2"));
    BOOST_CHECK(!OQSContext::IsAlgorithmAvailable("Falcon-512"));
}

/**
 * Test OQSContext creation and properties
 */
BOOST_AUTO_TEST_CASE(oqs_context_creation)
{
    // Test ML-DSA-65 context
    {
        OQSContext ctx("ML-DSA-65");
        BOOST_CHECK_EQUAL(ctx.GetAlgorithmName(), "ML-DSA-65");
        BOOST_CHECK_EQUAL(ctx.GetPublicKeySize(), 1952);
        BOOST_CHECK_EQUAL(ctx.GetSecretKeySize(), 4032);
        BOOST_CHECK_EQUAL(ctx.GetMaxSignatureSize(), 3309);
    }
    
    // Test SPHINCS+ context
    {
        OQSContext ctx("SPHINCS+-SHA2-192f-simple");
        BOOST_CHECK_EQUAL(ctx.GetAlgorithmName(), "SPHINCS+-SHA2-192f-simple");
        BOOST_CHECK_EQUAL(ctx.GetPublicKeySize(), 48);
        BOOST_CHECK_EQUAL(ctx.GetSecretKeySize(), 96);
        BOOST_CHECK_EQUAL(ctx.GetMaxSignatureSize(), 35664);
    }
    
    // Test invalid algorithm
    BOOST_CHECK_THROW(OQSContext("InvalidAlgorithm"), std::runtime_error);
}

/**
 * Test ML-DSA key generation and signing
 */
BOOST_AUTO_TEST_CASE(mldsa_sign_verify)
{
    OQSContext ctx("ML-DSA-65");
    
    // Generate keypair
    std::vector<unsigned char> public_key;
    std::vector<unsigned char> secret_key;
    BOOST_CHECK(ctx.GenerateKeypair(public_key, secret_key));
    BOOST_CHECK_EQUAL(public_key.size(), ctx.GetPublicKeySize());
    BOOST_CHECK_EQUAL(secret_key.size(), ctx.GetSecretKeySize());
    
    // Create test message
    uint256 message;
    GetRandBytes(message);
    
    // Sign message
    std::vector<unsigned char> signature;
    size_t sig_len = 0;
    BOOST_CHECK(ctx.Sign(signature, sig_len, message.begin(), 32, secret_key));
    BOOST_CHECK(!signature.empty());
    BOOST_CHECK_EQUAL(signature.size(), sig_len);
    BOOST_CHECK(sig_len <= ctx.GetMaxSignatureSize());
    
    // Verify signature
    BOOST_CHECK(ctx.Verify(message.begin(), 32, signature.data(), sig_len, public_key));
    
    // Verify with wrong message should fail
    uint256 wrong_message;
    GetRandBytes(wrong_message);
    BOOST_CHECK(!ctx.Verify(wrong_message.begin(), 32, signature.data(), sig_len, public_key));
    
    // Verify with corrupted signature should fail
    if (!signature.empty()) {
        signature[0] ^= 0x01;
        BOOST_CHECK(!ctx.Verify(message.begin(), 32, signature.data(), sig_len, public_key));
        signature[0] ^= 0x01; // Restore
    }
    
    // Verify with wrong public key should fail
    std::vector<unsigned char> wrong_public_key;
    std::vector<unsigned char> wrong_secret_key;
    BOOST_CHECK(ctx.GenerateKeypair(wrong_public_key, wrong_secret_key));
    BOOST_CHECK(!ctx.Verify(message.begin(), 32, signature.data(), sig_len, wrong_public_key));
}

/**
 * Test SPHINCS+ key generation and signing
 */
BOOST_AUTO_TEST_CASE(sphincs_sign_verify)
{
    OQSContext ctx("SPHINCS+-SHA2-192f-simple");
    
    // Generate keypair
    std::vector<unsigned char> public_key;
    std::vector<unsigned char> secret_key;
    BOOST_CHECK(ctx.GenerateKeypair(public_key, secret_key));
    BOOST_CHECK_EQUAL(public_key.size(), ctx.GetPublicKeySize());
    BOOST_CHECK_EQUAL(secret_key.size(), ctx.GetSecretKeySize());
    
    // Create test message
    uint256 message;
    GetRandBytes(message);
    
    // Sign message
    std::vector<unsigned char> signature;
    size_t sig_len = 0;
    BOOST_CHECK(ctx.Sign(signature, sig_len, message.begin(), 32, secret_key));
    BOOST_CHECK(!signature.empty());
    BOOST_CHECK_EQUAL(signature.size(), sig_len);
    BOOST_CHECK(sig_len <= ctx.GetMaxSignatureSize());
    
    // Note: SPHINCS+ signatures are large
    BOOST_TEST_MESSAGE("SPHINCS+ signature size: " << sig_len << " bytes");
    
    // Verify signature
    BOOST_CHECK(ctx.Verify(message.begin(), 32, signature.data(), sig_len, public_key));
    
    // Verify with wrong message should fail
    uint256 wrong_message;
    GetRandBytes(wrong_message);
    BOOST_CHECK(!ctx.Verify(wrong_message.begin(), 32, signature.data(), sig_len, public_key));
}

/**
 * Test OQSContext move semantics
 */
BOOST_AUTO_TEST_CASE(oqs_context_move)
{
    // Test move constructor
    {
        OQSContext ctx1("ML-DSA-65");
        std::string algo = ctx1.GetAlgorithmName();
        size_t pubkey_size = ctx1.GetPublicKeySize();
        
        OQSContext ctx2(std::move(ctx1));
        BOOST_CHECK_EQUAL(ctx2.GetAlgorithmName(), algo);
        BOOST_CHECK_EQUAL(ctx2.GetPublicKeySize(), pubkey_size);
    }
    
    // Test move assignment
    {
        OQSContext ctx1("ML-DSA-65");
        OQSContext ctx2("SPHINCS+-SHA2-192f-simple");
        
        size_t original_size = ctx1.GetPublicKeySize();
        ctx2 = std::move(ctx1);
        BOOST_CHECK_EQUAL(ctx2.GetPublicKeySize(), original_size);
    }
}

/**
 * Test SecureQuantumKey
 */
BOOST_AUTO_TEST_CASE(secure_quantum_key_test)
{
    const size_t key_size = 32;
    
    // Create secure key
    SecureQuantumKey key(key_size);
    BOOST_CHECK_EQUAL(key.size(), key_size);
    BOOST_CHECK(key.data() != nullptr);
    
    // Test that data is initialized (should be random)
    bool all_zero = true;
    for (size_t i = 0; i < key_size; ++i) {
        if (key.data()[i] != 0) {
            all_zero = false;
            break;
        }
    }
    BOOST_CHECK(!all_zero);
    
    // Test ToVector
    std::vector<unsigned char> vec = key.ToVector();
    BOOST_CHECK_EQUAL(vec.size(), key_size);
    BOOST_CHECK(std::memcmp(vec.data(), key.data(), key_size) == 0);
}

/**
 * Test multiple signatures with same key
 */
BOOST_AUTO_TEST_CASE(multiple_signatures)
{
    OQSContext ctx("ML-DSA-65");
    
    // Generate keypair
    std::vector<unsigned char> public_key;
    std::vector<unsigned char> secret_key;
    BOOST_CHECK(ctx.GenerateKeypair(public_key, secret_key));
    
    // Sign multiple messages with same key
    const int num_messages = 10;
    std::vector<uint256> messages(num_messages);
    std::vector<std::vector<unsigned char>> signatures(num_messages);
    std::vector<size_t> sig_lens(num_messages);
    
    for (int i = 0; i < num_messages; ++i) {
        GetRandBytes(messages[i]);
        BOOST_CHECK(ctx.Sign(signatures[i], sig_lens[i], 
                            messages[i].begin(), 32, secret_key));
    }
    
    // Verify all signatures
    for (int i = 0; i < num_messages; ++i) {
        BOOST_CHECK(ctx.Verify(messages[i].begin(), 32, 
                              signatures[i].data(), sig_lens[i], public_key));
        
        // Cross-verify: signature i should not verify message j (i != j)
        for (int j = 0; j < num_messages; ++j) {
            if (i != j) {
                BOOST_CHECK(!ctx.Verify(messages[j].begin(), 32,
                                       signatures[i].data(), sig_lens[i], public_key));
            }
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace quantum