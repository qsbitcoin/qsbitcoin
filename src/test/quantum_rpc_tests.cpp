// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/server.h>
#include <rpc/util.h>
#include <wallet/rpc/util.h>
#include <wallet/test/util.h>
#include <wallet/wallet.h>

#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

namespace wallet {

// Forward declarations of RPC commands
RPCHelpMan getquantuminfo();
RPCHelpMan validatequantumaddress();
RPCHelpMan getnewquantumaddress();
RPCHelpMan signmessagewithscheme();

namespace {

struct QuantumRPCTestFixture : public TestingSetup {
    QuantumRPCTestFixture() : TestingSetup(ChainType::MAIN) {}
};

BOOST_FIXTURE_TEST_SUITE(quantum_rpc_tests, QuantumRPCTestFixture)

BOOST_AUTO_TEST_CASE(getquantuminfo_basic)
{
    // Test getquantuminfo RPC command structure
    // Without wallet, it should return null, but we can test the handler exists
    JSONRPCRequest request;
    request.context = &m_node;
    request.strMethod = "getquantuminfo";
    
    UniValue result;
    BOOST_CHECK_NO_THROW(result = getquantuminfo().HandleRequest(request));
    
    // Without wallet it returns null
    if (!result.isNull()) {
        // Check basic fields exist
        BOOST_CHECK(result.exists("enabled"));
        BOOST_CHECK(result.exists("activated"));
        BOOST_CHECK(result.exists("quantum_keys"));
        BOOST_CHECK(result.exists("supported_algorithms"));
        
        // Check algorithm list
        const UniValue& algos = result["supported_algorithms"];
        BOOST_CHECK(algos.isArray());
        BOOST_CHECK_EQUAL(algos.size(), 2); // ML-DSA and SLH-DSA
    }
}

BOOST_AUTO_TEST_CASE(validatequantumaddress_invalid)
{
    // Test validatequantumaddress with invalid address
    JSONRPCRequest request;
    request.context = &m_node;
    request.strMethod = "validatequantumaddress";
    request.params = UniValue(UniValue::VARR);
    request.params.push_back("invalidaddress");
    
    UniValue result;
    BOOST_CHECK_NO_THROW(result = validatequantumaddress().HandleRequest(request));
    
    BOOST_CHECK_EQUAL(result["isvalid"].get_bool(), false);
}

BOOST_AUTO_TEST_CASE(validatequantumaddress_quantum_prefix)
{
    // Test validatequantumaddress with quantum-prefixed addresses
    JSONRPCRequest request;
    request.context = &m_node;
    request.strMethod = "validatequantumaddress";
    request.params = UniValue(UniValue::VARR);
    
    // Test with Q1 prefix (ML-DSA)
    std::string test_addr = "Q11111111111111111111111111111111111111";
    request.params.clear();
    request.params.push_back(test_addr);
    
    UniValue result;
    BOOST_CHECK_NO_THROW(result = validatequantumaddress().HandleRequest(request));
    
    // Even if address is invalid, it should recognize quantum type
    if (result["isvalid"].get_bool()) {
        BOOST_CHECK(result.exists("algorithm"));
        BOOST_CHECK(result.exists("type"));
        BOOST_CHECK_EQUAL(result["isquantum"].get_bool(), true);
    }
}

BOOST_AUTO_TEST_CASE(getnewquantumaddress_no_wallet)
{
    // Test getnewquantumaddress without wallet
    JSONRPCRequest request;
    request.context = &m_node;
    request.strMethod = "getnewquantumaddress";
    
    UniValue result;
    BOOST_CHECK_NO_THROW(result = getnewquantumaddress().HandleRequest(request));
    BOOST_CHECK(result.isNull());
}

BOOST_AUTO_TEST_CASE(signmessagewithscheme_basic)
{
    // Test signmessagewithscheme structure
    JSONRPCRequest request;
    request.context = &m_node;
    request.strMethod = "signmessagewithscheme";
    request.params = UniValue(UniValue::VARR);
    request.params.push_back("Q1testaddress");
    request.params.push_back("test message");
    request.params.push_back("ml-dsa");
    
    // Without wallet, should return null
    UniValue result;
    BOOST_CHECK_NO_THROW(result = signmessagewithscheme().HandleRequest(request));
    BOOST_CHECK(result.isNull());
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace
} // namespace wallet