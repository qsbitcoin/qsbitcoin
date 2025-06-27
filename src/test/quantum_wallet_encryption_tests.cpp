// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <crypto/quantum_key.h>
#include <wallet/crypter.h>
#include <wallet/quantum_scriptpubkeyman.h>
#include <test/util/setup_common.h>
#include <util/strencodings.h>

using namespace wallet;
using namespace quantum;

BOOST_FIXTURE_TEST_SUITE(quantum_wallet_encryption_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(encrypt_decrypt_quantum_key_ml_dsa)
{
    // Generate a quantum key
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    BOOST_CHECK(key.IsValid());
    
    // Get public key
    CQuantumPubKey pubkey = key.GetPubKey();
    BOOST_CHECK(pubkey.IsValid());
    
    // Create master key for encryption
    CKeyingMaterial master_key(WALLET_CRYPTO_KEY_SIZE, 0);
    GetStrongRandBytes(master_key);
    
    // Encrypt the key
    std::vector<unsigned char> encrypted_key;
    BOOST_CHECK(EncryptQuantumKey(master_key, key, pubkey, encrypted_key));
    BOOST_CHECK(!encrypted_key.empty());
    
    // Decrypt the key
    CQuantumKey decrypted_key;
    BOOST_CHECK(DecryptQuantumKey(master_key, encrypted_key, pubkey, decrypted_key));
    BOOST_CHECK(decrypted_key.IsValid());
    
    // Verify decrypted key produces same public key
    CQuantumPubKey decrypted_pubkey = decrypted_key.GetPubKey();
    BOOST_CHECK(decrypted_pubkey == pubkey);
    
    // Test signing with decrypted key
    uint256 hash = GetRandHash();
    std::vector<unsigned char> sig1, sig2;
    BOOST_CHECK(key.Sign(hash, sig1));
    BOOST_CHECK(decrypted_key.Sign(hash, sig2));
    
    // Signatures might differ due to randomness, but both should verify
    BOOST_CHECK(CQuantumKey::Verify(hash, sig1, pubkey));
    BOOST_CHECK(CQuantumKey::Verify(hash, sig2, pubkey));
}

BOOST_AUTO_TEST_CASE(encrypt_decrypt_quantum_key_slh_dsa)
{
    // Generate a quantum key
    CQuantumKey key;
    key.MakeNewKey(KeyType::SLH_DSA_192F);
    BOOST_CHECK(key.IsValid());
    
    // Get public key
    CQuantumPubKey pubkey = key.GetPubKey();
    BOOST_CHECK(pubkey.IsValid());
    
    // Create master key for encryption
    CKeyingMaterial master_key(WALLET_CRYPTO_KEY_SIZE, 0);
    GetStrongRandBytes(master_key);
    
    // Encrypt the key
    std::vector<unsigned char> encrypted_key;
    BOOST_CHECK(EncryptQuantumKey(master_key, key, pubkey, encrypted_key));
    BOOST_CHECK(!encrypted_key.empty());
    
    // Decrypt the key
    CQuantumKey decrypted_key;
    BOOST_CHECK(DecryptQuantumKey(master_key, encrypted_key, pubkey, decrypted_key));
    BOOST_CHECK(decrypted_key.IsValid());
    
    // Verify decrypted key produces same public key
    CQuantumPubKey decrypted_pubkey = decrypted_key.GetPubKey();
    BOOST_CHECK(decrypted_pubkey == pubkey);
}

BOOST_AUTO_TEST_CASE(quantum_key_encryption_wrong_master_key)
{
    // Generate a quantum key
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    CQuantumPubKey pubkey = key.GetPubKey();
    
    // Create master key for encryption
    CKeyingMaterial master_key(WALLET_CRYPTO_KEY_SIZE, 0);
    GetStrongRandBytes(master_key);
    
    // Encrypt the key
    std::vector<unsigned char> encrypted_key;
    BOOST_CHECK(EncryptQuantumKey(master_key, key, pubkey, encrypted_key));
    
    // Try to decrypt with wrong master key
    CKeyingMaterial wrong_master_key(WALLET_CRYPTO_KEY_SIZE, 0);
    GetStrongRandBytes(wrong_master_key);
    
    CQuantumKey decrypted_key;
    BOOST_CHECK(!DecryptQuantumKey(wrong_master_key, encrypted_key, pubkey, decrypted_key));
}

BOOST_AUTO_TEST_CASE(quantum_key_encryption_corrupted_data)
{
    // Generate a quantum key
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    CQuantumPubKey pubkey = key.GetPubKey();
    
    // Create master key for encryption
    CKeyingMaterial master_key(WALLET_CRYPTO_KEY_SIZE, 0);
    GetStrongRandBytes(master_key);
    
    // Encrypt the key
    std::vector<unsigned char> encrypted_key;
    BOOST_CHECK(EncryptQuantumKey(master_key, key, pubkey, encrypted_key));
    
    // Corrupt the encrypted data
    if (encrypted_key.size() > 10) {
        encrypted_key[10] ^= 0xFF;
    }
    
    // Try to decrypt corrupted data
    CQuantumKey decrypted_key;
    BOOST_CHECK(!DecryptQuantumKey(master_key, encrypted_key, pubkey, decrypted_key));
}

BOOST_AUTO_TEST_SUITE_END()