// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <script/script.h>
#include <script/interpreter.h>
#include <script/script_error.h>
#include <crypto/quantum_key.h>
#include <test/util/setup_common.h>
#include <uint256.h>

#include <vector>

using namespace quantum;

BOOST_FIXTURE_TEST_SUITE(script_quantum_tests_simple, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(quantum_key_sign_verify)
{
    // Test basic quantum key signing and verification
    CQuantumKey key;
    key.MakeNewKey(KeyType::ML_DSA_65);
    
    uint256 hash;
    hash.SetNull();
    hash.data()[31] = 1;  // Set the least significant byte to 1
    
    std::vector<unsigned char> sig;
    BOOST_CHECK(key.Sign(hash, sig));
    BOOST_CHECK(!sig.empty());
    
    CQuantumPubKey pubkey = key.GetPubKey();
    BOOST_CHECK(CQuantumKey::Verify(hash, sig, pubkey));
    
    // Wrong hash should fail
    uint256 wrongHash;
    wrongHash.SetNull();
    wrongHash.data()[31] = 2;  // Set the least significant byte to 2
    BOOST_CHECK(!CQuantumKey::Verify(wrongHash, sig, pubkey));
}

BOOST_AUTO_TEST_SUITE_END()