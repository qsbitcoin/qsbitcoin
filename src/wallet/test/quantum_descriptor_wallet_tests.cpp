// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/wallet.h>
#include <wallet/test/wallet_test_fixture.h>
#include <wallet/quantum_keystore.h>
#include <wallet/quantum_descriptor_util.h>
#include <wallet/scriptpubkeyman.h>
#include <wallet/context.h>
#include <crypto/quantum_key.h>
#include <script/descriptor.h>
#include <test/util/random.h>
#include <key_io.h>
#include <boost/test/unit_test.hpp>
#include <iostream>

namespace wallet {
BOOST_FIXTURE_TEST_SUITE(quantum_descriptor_wallet_tests, WalletTestingSetup)

BOOST_AUTO_TEST_CASE(quantum_descriptor_signing_provider)
{
    // Create a quantum key
    auto quantum_key = std::make_unique<quantum::CQuantumKey>();
    quantum_key->MakeNewKey(quantum::KeyType::ML_DSA_65);
    BOOST_CHECK(quantum_key->IsValid());
    
    quantum::CQuantumPubKey quantum_pubkey = quantum_key->GetPubKey();
    CKeyID keyid = quantum_pubkey.GetID();
    
    // Add to global keystore
    if (!g_quantum_keystore) {
        g_quantum_keystore = std::make_unique<QuantumKeyStore>();
    }
    g_quantum_keystore->AddQuantumKey(keyid, std::move(quantum_key));
    
    // Create quantum descriptor
    std::string pubkey_hex = HexStr(quantum_pubkey.GetKeyData());
    std::string descriptor_str = "qpkh(" + pubkey_hex + ")";
    
    // Parse descriptor
    FlatSigningProvider provider;
    std::string parse_error;
    auto parsed = Parse(descriptor_str, provider, parse_error);
    BOOST_CHECK(!parsed.empty());
    BOOST_CHECK_EQUAL(parse_error, "");
    
    // Get the script from the descriptor
    std::vector<CScript> scripts;
    FlatSigningProvider keys_out;
    parsed[0]->Expand(0, provider, scripts, keys_out);
    BOOST_CHECK_EQUAL(scripts.size(), 1);
    
    const CScript& script = scripts[0];
    
    // Debug: print script hex and check output type
    std::cout << "Script hex: " << HexStr(script) << std::endl;
    auto output_type = parsed[0]->GetOutputType();
    if (output_type) {
        std::cout << "Output type: " << static_cast<int>(*output_type) << std::endl;
    } else {
        std::cout << "No output type" << std::endl;
    }
    
    // Test PopulateQuantumSigningProvider
    // Pass keys_out which contains the witness scripts from descriptor expansion
    PopulateQuantumSigningProvider(script, keys_out, true);
    
    // Check that the quantum key was added
    BOOST_CHECK(keys_out.HaveQuantumKey(keyid));
    
    quantum::CQuantumPubKey retrieved_pubkey;
    BOOST_CHECK(keys_out.GetQuantumPubKey(keyid, retrieved_pubkey));
    BOOST_CHECK(retrieved_pubkey == quantum_pubkey);
    
    // Check private key access
    quantum::CQuantumKey* retrieved_key = nullptr;
    BOOST_CHECK(keys_out.GetQuantumKey(keyid, &retrieved_key));
    BOOST_CHECK(retrieved_key != nullptr);
}

BOOST_AUTO_TEST_CASE(quantum_descriptor_with_signing_provider)
{
    // Create a quantum key
    auto quantum_key = std::make_unique<quantum::CQuantumKey>();
    quantum_key->MakeNewKey(quantum::KeyType::SLH_DSA_192F);
    quantum::CQuantumPubKey quantum_pubkey = quantum_key->GetPubKey();
    CKeyID keyid = quantum_pubkey.GetID();
    
    // Add to global keystore
    if (!g_quantum_keystore) {
        g_quantum_keystore = std::make_unique<QuantumKeyStore>();
    }
    g_quantum_keystore->AddQuantumKey(keyid, std::move(quantum_key));
    
    // Create quantum pubkey hash descriptor
    std::string pubkey_hex = HexStr(quantum_pubkey.GetKeyData());
    std::string descriptor_str = "qpkh(" + pubkey_hex + ")";
    
    // Parse descriptor
    FlatSigningProvider keys;
    std::string parse_error;
    auto parsed = Parse(descriptor_str, keys, parse_error);
    BOOST_CHECK(!parsed.empty());
    BOOST_CHECK_EQUAL(parse_error, "");
    
    // Get the scripts from the descriptor
    std::vector<CScript> scripts;
    FlatSigningProvider out_keys;
    parsed[0]->Expand(0, keys, scripts, out_keys);
    BOOST_CHECK_EQUAL(scripts.size(), 1);
    
    // Test PopulateQuantumSigningProvider with the script
    // Use out_keys which contains the witness scripts
    PopulateQuantumSigningProvider(scripts[0], out_keys, true);
    
    // Check that the quantum key was added
    BOOST_CHECK(out_keys.HaveQuantumKey(keyid));
    
    quantum::CQuantumPubKey retrieved_pubkey;
    BOOST_CHECK(out_keys.GetQuantumPubKey(keyid, retrieved_pubkey));
    BOOST_CHECK(retrieved_pubkey == quantum_pubkey);
    
    // Check private key access
    quantum::CQuantumKey* retrieved_key = nullptr;
    BOOST_CHECK(out_keys.GetQuantumKey(keyid, &retrieved_key));
    BOOST_CHECK(retrieved_key != nullptr);
}

BOOST_AUTO_TEST_SUITE_END()
} // namespace wallet