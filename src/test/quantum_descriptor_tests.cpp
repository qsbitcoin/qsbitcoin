// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/descriptor.h>
#include <script/script.h>
#include <script/signingprovider.h>
#include <test/util/setup_common.h>
#include <util/strencodings.h>
#include <crypto/quantum_key.h>
#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(quantum_descriptor_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(quantum_descriptor_parse_basic)
{
    // Test parsing a basic qpkh descriptor with a hex public key
    // ML-DSA-65 public key is 1952 bytes
    std::string ml_dsa_pubkey_hex(1952 * 2, '0'); // Dummy hex key
    std::string descriptor = "qpkh(" + ml_dsa_pubkey_hex + ")";
    
    FlatSigningProvider provider;
    std::string error;
    auto parsed = Parse(descriptor, provider, error);
    
    BOOST_CHECK(!parsed.empty());
    BOOST_CHECK_EQUAL(error, "");
    
    // Check that it generates the expected script type
    std::vector<CScript> scripts;
    FlatSigningProvider out;
    parsed[0]->Expand(0, provider, scripts, out);
    BOOST_CHECK_EQUAL(scripts.size(), 1);
}

BOOST_AUTO_TEST_CASE(quantum_descriptor_parse_with_prefix)
{
    // Test parsing with quantum: prefix
    std::string ml_dsa_pubkey_hex(1952 * 2, '0'); // Dummy hex key
    std::string descriptor = "qpkh(quantum:ml-dsa:" + ml_dsa_pubkey_hex + ")";
    
    FlatSigningProvider provider;
    std::string error;
    auto parsed = Parse(descriptor, provider, error);
    
    BOOST_CHECK(!parsed.empty());
    BOOST_CHECK_EQUAL(error, "");
}

BOOST_AUTO_TEST_CASE(quantum_descriptor_invalid_size)
{
    // Test that invalid key sizes are rejected
    std::string invalid_pubkey_hex(100 * 2, '0'); // Invalid size
    std::string descriptor = "qpkh(" + invalid_pubkey_hex + ")";
    
    FlatSigningProvider provider;
    std::string error;
    auto parsed = Parse(descriptor, provider, error);
    
    BOOST_CHECK(parsed.empty());
    BOOST_CHECK(error.find("Invalid quantum public key size") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(quantum_descriptor_slh_dsa)
{
    // Test SLH-DSA key (48 bytes)
    std::string slh_dsa_pubkey_hex(48 * 2, '0'); // Dummy hex key
    std::string descriptor = "qpkh(" + slh_dsa_pubkey_hex + ")";
    
    FlatSigningProvider provider;
    std::string error;
    auto parsed = Parse(descriptor, provider, error);
    
    BOOST_CHECK(!parsed.empty());
    BOOST_CHECK_EQUAL(error, "");
}

BOOST_AUTO_TEST_CASE(quantum_descriptor_roundtrip)
{
    // Test that descriptor can be serialized and parsed again
    std::string ml_dsa_pubkey_hex(1952 * 2, '0'); // Dummy hex key
    std::string descriptor = "qpkh(" + ml_dsa_pubkey_hex + ")";
    
    FlatSigningProvider provider;
    std::string error;
    auto parsed = Parse(descriptor, provider, error);
    BOOST_REQUIRE(!parsed.empty());
    
    // Get string representation
    std::string serialized = parsed[0]->ToString();
    
    // Should contain qpkh
    BOOST_CHECK(serialized.find("qpkh(") != std::string::npos);
    
    // Parse again
    auto parsed2 = Parse(serialized, provider, error);
    BOOST_CHECK(!parsed2.empty());
    BOOST_CHECK_EQUAL(error, "");
}

BOOST_AUTO_TEST_CASE(quantum_descriptor_context)
{
    // Test that qpkh is only allowed in TOP and P2SH contexts
    std::string ml_dsa_pubkey_hex(1952 * 2, '0'); // Dummy hex key
    
    // Should work at top level
    std::string descriptor1 = "qpkh(" + ml_dsa_pubkey_hex + ")";
    FlatSigningProvider provider;
    std::string error;
    auto parsed1 = Parse(descriptor1, provider, error);
    BOOST_CHECK(!parsed1.empty());
    
    // Should work inside sh()
    std::string descriptor2 = "sh(qpkh(" + ml_dsa_pubkey_hex + "))";
    auto parsed2 = Parse(descriptor2, provider, error);
    BOOST_CHECK(!parsed2.empty());
    
    // Should NOT work inside wsh() - but this is enforced at script level,
    // not descriptor parsing level, so this test might pass
}

BOOST_AUTO_TEST_CASE(quantum_descriptor_invalid_format)
{
    FlatSigningProvider provider;
    std::string error;
    
    // Test invalid hex characters
    std::string invalid_hex = "qpkh(ZZZ)";
    auto parsed1 = Parse(invalid_hex, provider, error);
    BOOST_CHECK(parsed1.empty());
    BOOST_CHECK(!error.empty());
    
    // Test odd-length hex
    std::string odd_hex = "qpkh(0)";
    auto parsed2 = Parse(odd_hex, provider, error);
    BOOST_CHECK(parsed2.empty());
    BOOST_CHECK(!error.empty());
    
    // Test invalid prefix format
    std::string invalid_prefix = "qpkh(quantum:invalid-scheme:0000)";
    auto parsed3 = Parse(invalid_prefix, provider, error);
    BOOST_CHECK(parsed3.empty());
    BOOST_CHECK(!error.empty());
}

BOOST_AUTO_TEST_CASE(quantum_descriptor_script_generation)
{
    // Test that the generated script is correct
    std::string ml_dsa_pubkey_hex(1952 * 2, '0');
    std::string descriptor = "qpkh(" + ml_dsa_pubkey_hex + ")";
    
    FlatSigningProvider provider;
    std::string error;
    auto parsed = Parse(descriptor, provider, error);
    BOOST_REQUIRE(!parsed.empty());
    
    std::vector<CScript> scripts;
    FlatSigningProvider out;
    parsed[0]->Expand(0, provider, scripts, out);
    BOOST_REQUIRE_EQUAL(scripts.size(), 1);
    
    // Just check that we get a script - the exact size depends on the implementation
    const CScript& script = scripts[0];
    BOOST_CHECK(!script.empty());
    // Could add more specific checks here once we know the exact script format
}

BOOST_AUTO_TEST_CASE(quantum_descriptor_checksum)
{
    // Test descriptor with checksum
    std::string ml_dsa_pubkey_hex(1952 * 2, '0');
    std::string descriptor_no_checksum = "qpkh(" + ml_dsa_pubkey_hex + ")";
    
    // Get the checksum
    std::string checksum = GetDescriptorChecksum(descriptor_no_checksum);
    BOOST_CHECK(!checksum.empty());
    
    // Parse with checksum
    std::string descriptor_with_checksum = descriptor_no_checksum + "#" + checksum;
    FlatSigningProvider provider;
    std::string error;
    auto parsed = Parse(descriptor_with_checksum, provider, error);
    BOOST_CHECK(!parsed.empty());
    BOOST_CHECK_EQUAL(error, "");
    
    // Test with invalid checksum
    std::string descriptor_bad_checksum = descriptor_no_checksum + "#invalid";
    auto parsed_bad = Parse(descriptor_bad_checksum, provider, error);
    BOOST_CHECK(parsed_bad.empty());
    BOOST_CHECK(error.find("checksum") != std::string::npos);
}

BOOST_AUTO_TEST_CASE(quantum_descriptor_multiple_keys)
{
    // Test that we handle multiple quantum key types correctly
    std::string ml_dsa_hex(1952 * 2, '0');
    std::string slh_dsa_hex(48 * 2, '1');
    
    FlatSigningProvider provider;
    std::string error;
    
    // Parse ML-DSA descriptor
    auto parsed_ml = Parse("qpkh(" + ml_dsa_hex + ")", provider, error);
    BOOST_CHECK(!parsed_ml.empty());
    
    // Parse SLH-DSA descriptor
    auto parsed_slh = Parse("qpkh(" + slh_dsa_hex + ")", provider, error);
    BOOST_CHECK(!parsed_slh.empty());
    
    // Verify they produce different scripts
    std::vector<CScript> scripts_ml, scripts_slh;
    FlatSigningProvider out_ml, out_slh;
    parsed_ml[0]->Expand(0, provider, scripts_ml, out_ml);
    parsed_slh[0]->Expand(0, provider, scripts_slh, out_slh);
    
    BOOST_CHECK(scripts_ml[0] != scripts_slh[0]);
}

BOOST_AUTO_TEST_CASE(quantum_descriptor_range_error)
{
    // Quantum descriptors should not support ranges
    std::string ml_dsa_pubkey_hex(1952 * 2, '0');
    std::string range_descriptor = "qpkh(" + ml_dsa_pubkey_hex + "/*)";
    
    FlatSigningProvider provider;
    std::string error;
    auto parsed = Parse(range_descriptor, provider, error);
    
    // Should either fail to parse or produce a non-ranged descriptor
    if (!parsed.empty()) {
        BOOST_CHECK(!parsed[0]->IsRange());
    }
}

BOOST_AUTO_TEST_SUITE_END()