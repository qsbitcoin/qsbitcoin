// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <quantum_address.h>
#include <crypto/quantum_key.h>
#include <test/util/random.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

using namespace quantum;

BOOST_FIXTURE_TEST_SUITE(quantum_address_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(quantum_hash_function)
{
    // Test quantum hash function
    std::vector<unsigned char> data1 = {1, 2, 3, 4, 5};
    std::vector<unsigned char> data2 = {1, 2, 3, 4, 6}; // Different last byte
    
    uint256 hash1 = QuantumHash256(data1);
    uint256 hash2 = QuantumHash256(data2);
    
    // Different inputs should produce different hashes
    BOOST_CHECK(hash1 != hash2);
    
    // Same input should produce same hash
    uint256 hash1_again = QuantumHash256(data1);
    BOOST_CHECK(hash1 == hash1_again);
}

BOOST_AUTO_TEST_CASE(quantum_hash_empty_data)
{
    // Test hashing empty data
    std::vector<unsigned char> empty;
    uint256 hash = QuantumHash256(empty);
    
    // Should produce a valid hash (not zero)
    BOOST_CHECK(!hash.IsNull());
}

BOOST_AUTO_TEST_CASE(quantum_hash_large_data)
{
    // Test hashing large data
    std::vector<unsigned char> large_data(1000000, 0xAB);
    uint256 hash = QuantumHash256(large_data);
    
    // Should produce a valid hash
    BOOST_CHECK(!hash.IsNull());
}

BOOST_AUTO_TEST_SUITE_END()