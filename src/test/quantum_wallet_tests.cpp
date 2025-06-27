// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bitcoin-build-config.h> // IWYU pragma: keep

#include <wallet/wallet.h>
#include <wallet/test/util.h>
#include <wallet/quantum_keystore.h>
#include <crypto/quantum_key.h>
#include <quantum_address.h>
#include <key_io.h>

#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

namespace wallet {

struct QuantumWalletTestingSetup : public TestingSetup {
    QuantumWalletTestingSetup() : TestingSetup(ChainType::REGTEST)
    {
    }
};

BOOST_FIXTURE_TEST_SUITE(quantum_wallet_tests, QuantumWalletTestingSetup)

BOOST_AUTO_TEST_CASE(quantum_key_generation)
{
    using quantum::CQuantumKey;
    using quantum::CQuantumPubKey;
    
    // Test ML-DSA key generation
    auto ml_dsa_key = std::make_unique<CQuantumKey>();
    ml_dsa_key->MakeNewKey(::quantum::KeyType::ML_DSA_65);
    BOOST_CHECK(ml_dsa_key->IsValid());
    
    CQuantumPubKey ml_dsa_pubkey = ml_dsa_key->GetPubKey();
    BOOST_CHECK(ml_dsa_pubkey.IsValid());
    BOOST_CHECK_EQUAL(ml_dsa_pubkey.GetType(), ::quantum::KeyType::ML_DSA_65);
    
    // Test SLH-DSA key generation
    auto slh_dsa_key = std::make_unique<CQuantumKey>();
    slh_dsa_key->MakeNewKey(::quantum::KeyType::SLH_DSA_192F);
    BOOST_CHECK(slh_dsa_key->IsValid());
    
    CQuantumPubKey slh_dsa_pubkey = slh_dsa_key->GetPubKey();
    BOOST_CHECK(slh_dsa_pubkey.IsValid());
    BOOST_CHECK_EQUAL(slh_dsa_pubkey.GetType(), ::quantum::KeyType::SLH_DSA_192F);
}

BOOST_AUTO_TEST_CASE(quantum_keystore_operations)
{
    using quantum::CQuantumKey;
    using quantum::CQuantumPubKey;
    
    // Create a temporary keystore for testing
    QuantumKeyStore keystore;
    
    // Generate ML-DSA key
    auto key = std::make_unique<CQuantumKey>();
    key->MakeNewKey(::quantum::KeyType::ML_DSA_65);
    CQuantumPubKey pubkey = key->GetPubKey();
    CKeyID keyid = pubkey.GetID();
    
    // Add key to keystore
    BOOST_CHECK(keystore.AddQuantumKey(keyid, std::move(key)));
    
    // Check we have the key
    BOOST_CHECK(keystore.HaveQuantumKey(keyid));
    
    // Get the public key
    CQuantumPubKey retrieved_pubkey;
    BOOST_CHECK(keystore.GetQuantumPubKey(keyid, retrieved_pubkey));
    BOOST_CHECK(retrieved_pubkey == pubkey);
    
    // Get the private key
    const CQuantumKey* retrieved_key = nullptr;
    BOOST_CHECK(keystore.GetQuantumKey(keyid, &retrieved_key));
    BOOST_CHECK(retrieved_key != nullptr);
    BOOST_CHECK(retrieved_key->IsValid());
}

BOOST_AUTO_TEST_CASE(quantum_address_encoding)
{
    using quantum::CQuantumKey;
    using quantum::CQuantumPubKey;
    
    // Test ML-DSA address (Q1 prefix)
    auto ml_dsa_key = std::make_unique<CQuantumKey>();
    ml_dsa_key->MakeNewKey(::quantum::KeyType::ML_DSA_65);
    CQuantumPubKey ml_dsa_pubkey = ml_dsa_key->GetPubKey();
    CKeyID ml_dsa_keyid = ml_dsa_pubkey.GetID();
    
    CTxDestination ml_dsa_dest = PKHash(ml_dsa_keyid);
    std::string ml_dsa_address = EncodeQuantumDestination(ml_dsa_dest, 1); // 1 for ML-DSA
    BOOST_CHECK(ml_dsa_address.substr(0, 2) == "Q1");
    BOOST_CHECK(IsQuantumAddress(ml_dsa_address));
    BOOST_CHECK_EQUAL(GetQuantumAddressType(ml_dsa_address), 1);
    
    // Test SLH-DSA address (Q2 prefix)
    auto slh_dsa_key = std::make_unique<CQuantumKey>();
    slh_dsa_key->MakeNewKey(::quantum::KeyType::SLH_DSA_192F);
    CQuantumPubKey slh_dsa_pubkey = slh_dsa_key->GetPubKey();
    CKeyID slh_dsa_keyid = slh_dsa_pubkey.GetID();
    
    CTxDestination slh_dsa_dest = PKHash(slh_dsa_keyid);
    std::string slh_dsa_address = EncodeQuantumDestination(slh_dsa_dest, 2); // 2 for SLH-DSA
    BOOST_CHECK(slh_dsa_address.substr(0, 2) == "Q2");
    BOOST_CHECK(IsQuantumAddress(slh_dsa_address));
    BOOST_CHECK_EQUAL(GetQuantumAddressType(slh_dsa_address), 2);
}

BOOST_AUTO_TEST_CASE(quantum_address_decoding)
{
    // Create a normal address
    CKey key;
    key.MakeNewKey(true);
    CPubKey pubkey = key.GetPubKey();
    CKeyID keyid = pubkey.GetID();
    CTxDestination dest = PKHash(keyid);
    
    // Encode as quantum address with Q1 prefix
    std::string q1_address = EncodeQuantumDestination(dest, 1);
    BOOST_CHECK(q1_address.substr(0, 2) == "Q1");
    
    // Decode the quantum address
    CTxDestination decoded_dest = DecodeQuantumDestination(q1_address);
    BOOST_CHECK(decoded_dest == dest);
    
    // Encode as quantum address with Q2 prefix
    std::string q2_address = EncodeQuantumDestination(dest, 2);
    BOOST_CHECK(q2_address.substr(0, 2) == "Q2");
    
    // Decode the quantum address
    decoded_dest = DecodeQuantumDestination(q2_address);
    BOOST_CHECK(decoded_dest == dest);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace wallet