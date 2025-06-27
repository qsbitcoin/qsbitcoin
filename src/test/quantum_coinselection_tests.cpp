// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/spend.h>
#include <quantum_address.h>
#include <script/quantum_signature.h>
#include <test/util/setup_common.h>
#include <boost/test/unit_test.hpp>

using namespace quantum;

BOOST_FIXTURE_TEST_SUITE(quantum_coinselection_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(quantum_input_size_calculation)
{
    // Test ML-DSA input size calculation
    {
        CQuantumKey key;
        key.MakeNewKey(::quantum::KeyType::ML_DSA_65);
        CQuantumPubKey pubkey = key.GetPubKey();
        
        // Create P2QPKH script
        uint256 hash = ::quantum::QuantumHash256(pubkey.GetKeyData());
        CScript script = ::quantum::CreateP2QPKHScript(hash, ::quantum::KeyType::ML_DSA_65);
        
        // Debug: Check if the script is recognized as quantum
        ::quantum::QuantumAddressType testAddrType;
        uint256 testHash;
        bool isQuantum = ::quantum::ExtractQuantumAddress(script, testAddrType, testHash);
        BOOST_TEST_MESSAGE("Script recognized as quantum: " << isQuantum);
        BOOST_TEST_MESSAGE("Script size: " << script.size());
        BOOST_TEST_MESSAGE("Script hex: " << HexStr(script));
        
        CTxOut txout(CAmount(100000), script);
        
        // Calculate input size - this should use our new quantum size calculation
        int input_size = wallet::CalculateMaximumSignedInputSize(txout, COutPoint(), nullptr, false, nullptr);
        
        // Expected size: 36 (prevout) + 4 (sequence) + varint + (1 + varint + 3366 + varint + 1952)
        // Script size: 1 + ~2 + 3366 + ~2 + 1952 = ~5323
        // Total: 40 + ~2 + 5323 = ~5365
        BOOST_CHECK_GT(input_size, 5000);
        BOOST_CHECK_LT(input_size, 6000);
        
        // Log the actual size for debugging
        BOOST_TEST_MESSAGE("ML-DSA input size: " << input_size);
    }
    
    // Test SLH-DSA input size calculation
    {
        CQuantumKey key;
        key.MakeNewKey(::quantum::KeyType::SLH_DSA_192F);
        CQuantumPubKey pubkey = key.GetPubKey();
        
        // Create P2QPKH script
        uint256 hash = ::quantum::QuantumHash256(pubkey.GetKeyData());
        CScript script = ::quantum::CreateP2QPKHScript(hash, ::quantum::KeyType::SLH_DSA_192F);
        
        CTxOut txout(CAmount(100000), script);
        
        // Calculate input size
        int input_size = wallet::CalculateMaximumSignedInputSize(txout, COutPoint(), nullptr, false, nullptr);
        
        // Expected size: much larger due to 49KB signature
        BOOST_CHECK_GT(input_size, 49000);
        BOOST_CHECK_LT(input_size, 51000);
        
        // Log the actual size for debugging
        BOOST_TEST_MESSAGE("SLH-DSA input size: " << input_size);
    }
    
    // Test that non-quantum scripts still work
    {
        // Standard P2PKH script
        CKey key;
        key.MakeNewKey(true);
        CPubKey pubkey = key.GetPubKey();
        CKeyID keyid = pubkey.GetID();
        
        CScript script = GetScriptForDestination(PKHash(keyid));
        CTxOut txout(CAmount(100000), script);
        
        // This should fall back to the standard calculation
        // Since we don't have a provider, it will return -1 for non-quantum scripts
        int input_size = wallet::CalculateMaximumSignedInputSize(txout, COutPoint(), nullptr, false, nullptr);
        
        // Without a provider, standard ECDSA scripts return -1
        BOOST_CHECK_EQUAL(input_size, -1);
        
        // Log the actual size for debugging
        BOOST_TEST_MESSAGE("ECDSA input size: " << input_size);
    }
}

BOOST_AUTO_TEST_CASE(quantum_script_recognition)
{
    // Test that ExtractQuantumAddress correctly identifies quantum scripts
    {
        // ML-DSA script
        CQuantumKey key;
        key.MakeNewKey(::quantum::KeyType::ML_DSA_65);
        uint256 hash = ::quantum::QuantumHash256(key.GetPubKey().GetKeyData());
        CScript ml_dsa_script = ::quantum::CreateP2QPKHScript(hash, ::quantum::KeyType::ML_DSA_65);
        
        ::quantum::QuantumAddressType addrType;
        uint256 extractedHash;
        bool is_quantum = ::quantum::ExtractQuantumAddress(ml_dsa_script, addrType, extractedHash);
        
        BOOST_CHECK(is_quantum);
        BOOST_CHECK_EQUAL(static_cast<int>(addrType), static_cast<int>(::quantum::QuantumAddressType::P2QPKH_ML_DSA));
        BOOST_CHECK(extractedHash == hash);
    }
    
    {
        // SLH-DSA script
        CQuantumKey key;
        key.MakeNewKey(::quantum::KeyType::SLH_DSA_192F);
        uint256 hash = ::quantum::QuantumHash256(key.GetPubKey().GetKeyData());
        CScript slh_dsa_script = ::quantum::CreateP2QPKHScript(hash, ::quantum::KeyType::SLH_DSA_192F);
        
        ::quantum::QuantumAddressType addrType;
        uint256 extractedHash;
        bool is_quantum = ::quantum::ExtractQuantumAddress(slh_dsa_script, addrType, extractedHash);
        
        BOOST_CHECK(is_quantum);
        BOOST_CHECK_EQUAL(static_cast<int>(addrType), static_cast<int>(::quantum::QuantumAddressType::P2QPKH_SLH_DSA));
        BOOST_CHECK(extractedHash == hash);
    }
    
    {
        // Regular P2PKH script (should not be recognized as quantum)
        CKey key;
        key.MakeNewKey(true);
        CScript regular_script = GetScriptForDestination(PKHash(key.GetPubKey().GetID()));
        
        ::quantum::QuantumAddressType addrType;
        uint256 extractedHash;
        bool is_quantum = ::quantum::ExtractQuantumAddress(regular_script, addrType, extractedHash);
        
        BOOST_CHECK(!is_quantum);
    }
}

BOOST_AUTO_TEST_SUITE_END()