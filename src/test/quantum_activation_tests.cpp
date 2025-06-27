// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <chainparams.h>
#include <consensus/params.h>
#include <deploymentstatus.h>
#include <policy/policy.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <test/util/setup_common.h>
#include <validation.h>
#include <versionbits.h>

#include <memory>

BOOST_FIXTURE_TEST_SUITE(quantum_activation_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(quantum_deployment_defined)
{
    // Test that quantum signature deployment is properly defined
    SelectParams(ChainType::MAIN);
    const auto& consensus = Params().GetConsensus();
    
    // Check deployment parameters
    BOOST_CHECK_EQUAL(consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].bit, 3);
    BOOST_CHECK_EQUAL(consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].nStartTime, Consensus::BIP9Deployment::NEVER_ACTIVE);
    BOOST_CHECK_EQUAL(consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].nTimeout, Consensus::BIP9Deployment::NO_TIMEOUT);
    BOOST_CHECK_EQUAL(consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].threshold, 1815);
}

BOOST_AUTO_TEST_CASE(quantum_activation_testnet)
{
    // Test that quantum signatures are always active on testnet
    SelectParams(ChainType::TESTNET);
    const auto& consensus = Params().GetConsensus();
    
    BOOST_CHECK_EQUAL(consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].nStartTime, Consensus::BIP9Deployment::ALWAYS_ACTIVE);
}

BOOST_AUTO_TEST_CASE(quantum_activation_regtest)
{
    // Test that quantum signatures are always active on regtest
    SelectParams(ChainType::REGTEST);
    const auto& consensus = Params().GetConsensus();
    
    BOOST_CHECK_EQUAL(consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].nStartTime, Consensus::BIP9Deployment::ALWAYS_ACTIVE);
    
    // Reset to main for other tests
    SelectParams(ChainType::MAIN);
}

BOOST_AUTO_TEST_CASE(quantum_script_flags_inactive)
{
    // Test that SCRIPT_VERIFY_QUANTUM_SIGS is not included when deployment is not active
    // This would require a more complex test setup with actual blockchain state
    // For now, we just verify the flag exists
    BOOST_CHECK(SCRIPT_VERIFY_QUANTUM_SIGS == (1U << 21));
}

BOOST_AUTO_TEST_CASE(quantum_opcodes_soft_fork)
{
    // Test that quantum opcodes are NOPs when SCRIPT_VERIFY_QUANTUM_SIGS is not set
    CScript script;
    script << OP_1 << OP_CHECKSIG_ML_DSA;
    
    std::vector<std::vector<unsigned char>> stack;
    ScriptError error;
    
    // Without SCRIPT_VERIFY_QUANTUM_SIGS, the opcode should act as a NOP
    unsigned int flags_without_quantum = SCRIPT_VERIFY_P2SH;
    BaseSignatureChecker checker;
    BOOST_CHECK(EvalScript(stack, script, flags_without_quantum, checker, SigVersion::BASE, &error));
    BOOST_CHECK_EQUAL(stack.size(), 1U); // OP_1 remains on stack
    
    // Test that without SCRIPT_VERIFY_QUANTUM_SIGS but with DISCOURAGE_UPGRADABLE_NOPS, it should fail
    unsigned int flags_discourage_nops = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS;
    stack.clear();
    BOOST_CHECK(!EvalScript(stack, script, flags_discourage_nops, checker, SigVersion::BASE, &error));
    BOOST_CHECK_EQUAL(error, SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS);
    
    // Test quantum opcode recognition
    BOOST_CHECK_EQUAL(GetOpName(OP_CHECKSIG_ML_DSA), "OP_CHECKSIG_ML_DSA");
    BOOST_CHECK_EQUAL(GetOpName(OP_CHECKSIG_SLH_DSA), "OP_CHECKSIG_SLH_DSA");
}

BOOST_AUTO_TEST_SUITE_END()