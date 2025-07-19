// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <kernel/chainparams.h>

#include <chainparamsseeds.h>
#include <consensus/amount.h>
#include <consensus/merkle.h>
#include <consensus/params.h>
#include <hash.h>
#include <kernel/messagestartchars.h>
#include <logging.h>
#include <pow.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <uint256.h>
#include <util/chaintype.h>
#include <util/strencodings.h>

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include <arith_uint256.h>

using namespace util::hex_literals;

// Workaround MSVC bug triggering C7595 when calling consteval constructors in
// initializer lists.
// https://developercommunity.visualstudio.com/t/Bogus-C7595-error-on-valid-C20-code/10906093
#if defined(_MSC_VER)
auto consteval_ctor(auto&& input) { return input; }
#else
#define consteval_ctor(input) (input)
#endif

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.version = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893, vtx=1)
 *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: 4a5e1e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "The Times 03/Jan/2009 Chancellor on brink of second bailout for banks";
    const CScript genesisOutputScript = CScript() << "04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f"_hex << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

/**
 * Main network on which people trade goods and services.
 */
class CMainParams : public CChainParams {
public:
    CMainParams() {
        m_chain_type = ChainType::MAIN;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.script_flag_exceptions.emplace( // BIP16 exception
            uint256{"00000000000002dc756eebf4f49723ed8d30cc28a5f108eb94b1ba88ac4f9c22"}, SCRIPT_VERIFY_NONE);
        consensus.script_flag_exceptions.emplace( // Taproot exception
            uint256{"0000000000000000000f14c35b2d841e986ab5441de8c585d5ffe55ea1e395ad"}, SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS);
        consensus.BIP34Height = 227931;
        consensus.BIP34Hash = uint256{"000000000000024b89b42a942fe0d9fea3bb44ab7bd1b19115dd6a759c0808b8"};
        consensus.BIP65Height = 388381; // 000000000000000004c2b624ed5d7756c508d90fd0da2c7c679febfa6c4735f0
        consensus.BIP66Height = 363725; // 00000000000000000379eaa19dce8c9b722d46ae6a57c2f1a988119488b50931
        consensus.CSVHeight = 419328; // 000000000000000004a1b34462cb8aeebd5799177f7a29cf28f2d1961716b5b5
        consensus.SegwitHeight = 481824; // 0000000000000000001c8018d9cb3b742ef25114f27563e3fc4a1902167f9893
        consensus.MinBIP9WarningHeight = 483840; // segwit activation height + miner confirmation window
        consensus.powLimit = uint256{"00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.enforce_BIP94 = false;
        consensus.fPowNoRetargeting = false;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].threshold = 1815; // 90%
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period = 2016;

        // Deployment of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = 1619222400; // April 24th, 2021
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = 1628640000; // August 11th, 2021
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 709632; // Approximately November 12th, 2021
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].threshold = 1815; // 90%
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].period = 2016;

        // Deployment of Quantum-Safe Signatures
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].bit = 3;
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE; // To be activated later
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].threshold = 1815; // 90%
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].period = 2016;

        consensus.nMinimumChainWork = uint256{"0000000000000000000000000000000000000000b1f3b93b65b16d035a82be84"};
        consensus.defaultAssumeValid = uint256{"00000000000000000001b658dd1120e82e66d2790811f89ede9742ada3ed6d77"}; // 886157

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0xf9;
        pchMessageStart[1] = 0xbe;
        pchMessageStart[2] = 0xb4;
        pchMessageStart[3] = 0xd9;
        nDefaultPort = 8333;
        nPruneAfterHeight = 100000;
        m_assumed_blockchain_size = 720;
        m_assumed_chain_state_size = 14;

        genesis = CreateGenesisBlock(1231006505, 2083236893, 0x1d00ffff, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256{"000000000019d6689c085ae165831e934ff763ae46a2a6c172b3f1b60a8ce26f"});
        assert(genesis.hashMerkleRoot == uint256{"4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"});

        // Note that of those which support the service bits prefix, most only support a subset of
        // possible options.
        // This is fine at runtime as we'll fall back to using them as an addrfetch if they don't support the
        // service bits we want, but we should get them updated to support all service bits wanted by any
        // release ASAP to avoid it where possible.
        vSeeds.emplace_back("seed.bitcoin.sipa.be."); // Pieter Wuille, only supports x1, x5, x9, and xd
        vSeeds.emplace_back("dnsseed.bluematt.me."); // Matt Corallo, only supports x9
        vSeeds.emplace_back("dnsseed.bitcoin.dashjr-list-of-p2p-nodes.us."); // Luke Dashjr
        vSeeds.emplace_back("seed.bitcoin.jonasschnelli.ch."); // Jonas Schnelli, only supports x1, x5, x9, and xd
        vSeeds.emplace_back("seed.btc.petertodd.net."); // Peter Todd, only supports x1, x5, x9, and xd
        vSeeds.emplace_back("seed.bitcoin.sprovoost.nl."); // Sjors Provoost
        vSeeds.emplace_back("dnsseed.emzy.de."); // Stephan Oeste
        vSeeds.emplace_back("seed.bitcoin.wiz.biz."); // Jason Maurice
        vSeeds.emplace_back("seed.mainnet.achownodes.xyz."); // Ava Chow, only supports x1, x5, x9, x49, x809, x849, xd, x400, x404, x408, x448, xc08, xc48, x40c

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,0);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,5);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,128);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};

        bech32_hrp = "bc";

        vFixedSeeds = std::vector<uint8_t>(std::begin(chainparams_seed_main), std::end(chainparams_seed_main));

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        m_assumeutxo_data = {
            {
                .height = 840'000,
                .hash_serialized = AssumeutxoHash{uint256{"a2a5521b1b5ab65f67818e5e8eccabb7171a517f9e2382208f77687310768f96"}},
                .m_chain_tx_count = 991032194,
                .blockhash = consteval_ctor(uint256{"0000000000000000000320283a032748cef8227873ff4872689bf23f1cda83a5"}),
            },
            {
                .height = 880'000,
                .hash_serialized = AssumeutxoHash{uint256{"dbd190983eaf433ef7c15f78a278ae42c00ef52e0fd2a54953782175fbadcea9"}},
                .m_chain_tx_count = 1145604538,
                .blockhash = consteval_ctor(uint256{"000000000000000000010b17283c3c400507969a9c2afd1dcf2082ec5cca2880"}),
            }
        };

        chainTxData = ChainTxData{
            // Data from RPC: getchaintxstats 4096 00000000000000000001b658dd1120e82e66d2790811f89ede9742ada3ed6d77
            .nTime    = 1741017141,
            .tx_count = 1161875261,
            .dTxRate  = 4.620728156243148,
        };
    }
};

/**
 * Testnet (v3): public test network which is reset from time to time.
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        m_chain_type = ChainType::TESTNET;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.script_flag_exceptions.emplace( // BIP16 exception
            uint256{"00000000dd30457c001f4095d208cc1296b0eed002427aa599874af7a432b105"}, SCRIPT_VERIFY_NONE);
        consensus.BIP34Height = 21111;
        consensus.BIP34Hash = uint256{"0000000023b3a96d3484e5abb3755c413e7d41500f8e2a5c3f0dd01299cd8ef8"};
        consensus.BIP65Height = 581885; // 00000000007f6655f22f98e72ed80d8b06dc761d5da09df0fa1dc4be4f861eb6
        consensus.BIP66Height = 330776; // 000000002104c8c45e99a8853285a3b592602a3ccde2b832481da85e9e4ba182
        consensus.CSVHeight = 770112; // 00000000025e930139bac5c6c31a403776da130831ab85be56578f3fa75369bb
        consensus.SegwitHeight = 834624; // 00000000002b980fcd729daaa248fd9316a5200e9b367f4ff2c42453e84201ca
        consensus.MinBIP9WarningHeight = 836640; // segwit activation height + miner confirmation window
        consensus.powLimit = uint256{"00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.enforce_BIP94 = false;
        consensus.fPowNoRetargeting = false;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].threshold = 1512; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period = 2016;

        // Deployment of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = 1619222400; // April 24th, 2021
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = 1628640000; // August 11th, 2021
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].threshold = 1512; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].period = 2016;

        // Deployment of Quantum-Safe Signatures
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].bit = 3;
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE; // Active on testnet for testing
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].threshold = 1512; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].period = 2016;

        consensus.nMinimumChainWork = uint256{"0000000000000000000000000000000000000000000015f5e0c9f13455b0eb17"};
        consensus.defaultAssumeValid = uint256{"00000000000003fc7967410ba2d0a8a8d50daedc318d43e8baf1a9782c236a57"}; // 3974606

        pchMessageStart[0] = 0x0b;
        pchMessageStart[1] = 0x11;
        pchMessageStart[2] = 0x09;
        pchMessageStart[3] = 0x07;
        nDefaultPort = 18333;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 200;
        m_assumed_chain_state_size = 19;

        genesis = CreateGenesisBlock(1296688602, 414098458, 0x1d00ffff, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256{"000000000933ea01ad0ee984209779baaec3ced90fa3f408719526f8d77f4943"});
        assert(genesis.hashMerkleRoot == uint256{"4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"});

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        vSeeds.emplace_back("testnet-seed.bitcoin.jonasschnelli.ch.");
        vSeeds.emplace_back("seed.tbtc.petertodd.net.");
        vSeeds.emplace_back("seed.testnet.bitcoin.sprovoost.nl.");
        vSeeds.emplace_back("testnet-seed.bluematt.me."); // Just a static list of stable node(s), only supports x9
        vSeeds.emplace_back("seed.testnet.achownodes.xyz."); // Ava Chow, only supports x1, x5, x9, x49, x809, x849, xd, x400, x404, x408, x448, xc08, xc48, x40c

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "tb";

        vFixedSeeds = std::vector<uint8_t>(std::begin(chainparams_seed_test), std::end(chainparams_seed_test));

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        m_assumeutxo_data = {
            {
                .height = 2'500'000,
                .hash_serialized = AssumeutxoHash{uint256{"f841584909f68e47897952345234e37fcd9128cd818f41ee6c3ca68db8071be7"}},
                .m_chain_tx_count = 66484552,
                .blockhash = consteval_ctor(uint256{"0000000000000093bcb68c03a9a168ae252572d348a2eaeba2cdf9231d73206f"}),
            }
        };

        chainTxData = ChainTxData{
            // Data from RPC: getchaintxstats 4096 00000000000003fc7967410ba2d0a8a8d50daedc318d43e8baf1a9782c236a57
            .nTime    = 1741042082,
            .tx_count = 475477615,
            .dTxRate  = 17.15933950357594,
        };
    }
};

/**
 * Testnet (v4): public test network which is reset from time to time.
 */
class CTestNet4Params : public CChainParams {
public:
    CTestNet4Params() {
        m_chain_type = ChainType::TESTNET4;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 1;
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256{"00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.enforce_BIP94 = true;
        consensus.fPowNoRetargeting = false;

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].threshold = 1512; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period = 2016;

        // Deployment of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].threshold = 1512; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].period = 2016;

        // Deployment of Quantum-Safe Signatures
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].bit = 3;
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE; // Active on testnet4 for testing
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].threshold = 1512; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].period = 2016;

        consensus.nMinimumChainWork = uint256{"0000000000000000000000000000000000000000000001d6dce8651b6094e4c1"};
        consensus.defaultAssumeValid = uint256{"0000000000003ed4f08dbdf6f7d6b271a6bcffce25675cb40aa9fa43179a89f3"}; // 72600

        pchMessageStart[0] = 0x1c;
        pchMessageStart[1] = 0x16;
        pchMessageStart[2] = 0x3f;
        pchMessageStart[3] = 0x28;
        nDefaultPort = 48333;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 11;
        m_assumed_chain_state_size = 1;

        const char* testnet4_genesis_msg = "03/May/2024 000000000000000000001ebd58c244970b3aa9d783bb001011fbe8ea8e98e00e";
        const CScript testnet4_genesis_script = CScript() << "000000000000000000000000000000000000000000000000000000000000000000"_hex << OP_CHECKSIG;
        genesis = CreateGenesisBlock(testnet4_genesis_msg,
                testnet4_genesis_script,
                1714777860,
                393743547,
                0x1d00ffff,
                1,
                50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256{"00000000da84f2bafbbc53dee25a72ae507ff4914b867c565be350b0da8bf043"});
        assert(genesis.hashMerkleRoot == uint256{"7aa0a7ae1e223414cb807e40cd57e667b718e42aaf9306db9102fe28912b7b4e"});

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        vSeeds.emplace_back("seed.testnet4.bitcoin.sprovoost.nl."); // Sjors Provoost
        vSeeds.emplace_back("seed.testnet4.wiz.biz."); // Jason Maurice

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "tb";

        vFixedSeeds = std::vector<uint8_t>(std::begin(chainparams_seed_testnet4), std::end(chainparams_seed_testnet4));

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        m_assumeutxo_data = {
            {}
        };

        chainTxData = ChainTxData{
            // Data from RPC: getchaintxstats 4096 0000000000003ed4f08dbdf6f7d6b271a6bcffce25675cb40aa9fa43179a89f3
            .nTime    = 1741070246,
            .tx_count = 7653966,
            .dTxRate  = 1.239174414591965,
        };
    }
};

/**
 * QSTestnet: Quantum-Safe Bitcoin test network with custom genesis block.
 */
class CQSTestNetParams : public CChainParams {
public:
    CQSTestNetParams() {
        m_chain_type = ChainType::QSTESTNET;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 1;
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256{"00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.nPowTargetTimespan = 24 * 60 * 60; // one day
        consensus.nPowTargetSpacing = 60; // 1 minute (reduced from 10 minutes)
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.enforce_BIP94 = true;
        consensus.fPowNoRetargeting = false;

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].threshold = 1512; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period = 2016;

        // Deployment of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].threshold = 1512; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].period = 2016;

        // Deployment of Quantum-Safe Signatures
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].bit = 3;
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].threshold = 1512; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].period = 2016;

        consensus.nMinimumChainWork = uint256{};
        consensus.defaultAssumeValid = uint256{};

        pchMessageStart[0] = 0x51; // 'Q'
        pchMessageStart[1] = 0x53; // 'S'
        pchMessageStart[2] = 0x42; // 'B'
        pchMessageStart[3] = 0x54; // 'T'
        nDefaultPort = 28333;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 1;
        m_assumed_chain_state_size = 1;

        const char* pszTimestamp = "QSBitcoin Testnet Genesis - Quantum Safe Since 2025/07/16";
        const CScript genesisOutputScript = CScript() << "04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f"_hex << OP_CHECKSIG;
        genesis = CreateGenesisBlock(pszTimestamp, genesisOutputScript, 1736985600, 4520877, 0x1e0fffff, 1, 50 * COIN);
        
        // Mine genesis block
        bool mine_genesis = false; // Set to true to mine genesis block
        if (mine_genesis) {
            printf("Mining QSTestnet genesis block...\n");
            arith_uint256 target;
            target.SetCompact(genesis.nBits);
            printf("Target: %s\n", target.ToString().c_str());
            double probability = target.getdouble() / std::pow(2.0, 256);
            printf("Difficulty: %.4f\n", 1.0 / probability);
            printf("Expected hashes: %.0f\n\n", 1.0 / probability);
            
            auto start_time = std::chrono::steady_clock::now();
            uint32_t nonce_start = genesis.nNonce;
            uint32_t last_nonce = genesis.nNonce;
            auto last_time = start_time;
            
            while (!CheckProofOfWork(genesis.GetHash(), genesis.nBits, consensus)) {
                ++genesis.nNonce;
                
                // Debug: Show first few hashes
                if (genesis.nNonce < 10) {
                    printf("Nonce %u: Hash = %s\n", genesis.nNonce, genesis.GetHash().ToString().c_str());
                }
                
                // Show progress every 1 million hashes
                if (genesis.nNonce % 1000000 == 0) {
                    auto current_time = std::chrono::steady_clock::now();
                    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
                    auto interval = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_time).count();
                    
                    if (interval > 0) {
                        uint32_t hashes_in_interval = genesis.nNonce - last_nonce;
                        double hashrate = (hashes_in_interval * 1000.0) / interval;
                        
                        // Estimate time remaining based on probability
                        arith_uint256 target;
                        target.SetCompact(genesis.nBits);
                        double probability = target.getdouble() / std::pow(2.0, 256);
                        double expected_hashes = 1.0 / probability;
                        
                        printf("Progress: %u hashes | %.2f H/s | Elapsed: %lds", 
                               genesis.nNonce, hashrate, elapsed);
                        
                        // Only show ETA if we haven't exceeded expected hashes
                        if (genesis.nNonce - nonce_start < expected_hashes) {
                            double estimated_seconds = (expected_hashes - (genesis.nNonce - nonce_start)) / hashrate;
                            printf(" | ETA: %.1f minutes", estimated_seconds / 60.0);
                        } else {
                            // We've exceeded expected hashes, show how much over
                            double percent_over = ((genesis.nNonce - nonce_start) / expected_hashes - 1.0) * 100.0;
                            printf(" | %.0f%% over expected", percent_over);
                        }
                        printf("\n");
                        
                        last_nonce = genesis.nNonce;
                        last_time = current_time;
                    }
                }
                
                if (genesis.nNonce == 0) {
                    ++genesis.nTime;
                    printf("Wrapped nonce, incrementing timestamp to %u\n", genesis.nTime);
                }
            }
            
            auto end_time = std::chrono::steady_clock::now();
            auto total_elapsed = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time).count();
            
            printf("\nGenesis block found!\n");
            printf("Total time: %ld seconds\n", total_elapsed);
            printf("Total hashes: %u\n", genesis.nNonce - nonce_start);
            printf("Average hashrate: %.2f H/s\n", (genesis.nNonce - nonce_start) / (double)total_elapsed);
            printf("\nQSTestnet genesis nonce: %u\n", genesis.nNonce);
            printf("QSTestnet genesis hash: %s\n", genesis.GetHash().ToString().c_str());
            printf("QSTestnet genesis merkle: %s\n", genesis.hashMerkleRoot.ToString().c_str());
        }
        
        // Update these values after mining
        consensus.hashGenesisBlock = genesis.GetHash();
        
        // Verify genesis block hash and merkle root
        assert(consensus.hashGenesisBlock == uint256{"0000091319de1b3007ac149ab0cfe75190a11f3d5df3b4a48f1a6dd8a7e7d32d"});
        assert(genesis.hashMerkleRoot == uint256{"345be921e30c1da4938bc74d1dbe555e2dfd3a3d425c1a90125d7b47750e4d15"});
        
        vFixedSeeds.clear();
        vSeeds.clear();
        
        // Testnet bootstrap nodes
        // These are the initial nodes for the QSTestnet network
        // Format: IP addresses will be converted to seed format by generate-seeds.py
        // For now, we'll add them as DNS seeds for easier management
        vSeeds.emplace_back("192.168.1.102");
        vSeeds.emplace_back("192.168.1.103");
        vSeeds.emplace_back("192.168.1.104");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "qt"; // Quantum Testnet

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;

        m_assumeutxo_data = {
            // Will be populated once the network has grown
        };

        chainTxData = ChainTxData{
            // Data will be populated once the network is active
            0,
            0,
            0,
        };
    }
};

/**
 * Signet: test network with an additional consensus parameter (see BIP325).
 */
class SigNetParams : public CChainParams {
public:
    explicit SigNetParams(const SigNetOptions& options)
    {
        std::vector<uint8_t> bin;
        vFixedSeeds.clear();
        vSeeds.clear();

        if (!options.challenge) {
            bin = "512103ad5e0edad18cb1f0fc0d28a3d4f1f3e445640337489abb10404f2d1e086be430210359ef5021964fe22d6f8e05b2463c9540ce96883fe3b278760f048f5189f2e6c452ae"_hex_v_u8;
            vFixedSeeds = std::vector<uint8_t>(std::begin(chainparams_seed_signet), std::end(chainparams_seed_signet));
            vSeeds.emplace_back("seed.signet.bitcoin.sprovoost.nl.");
            vSeeds.emplace_back("seed.signet.achownodes.xyz."); // Ava Chow, only supports x1, x5, x9, x49, x809, x849, xd, x400, x404, x408, x448, xc08, xc48, x40c

            consensus.nMinimumChainWork = uint256{"000000000000000000000000000000000000000000000000000002b517f3d1a1"};
            consensus.defaultAssumeValid = uint256{"000000895a110f46e59eb82bbc5bfb67fa314656009c295509c21b4999f5180a"}; // 237722
            m_assumed_blockchain_size = 9;
            m_assumed_chain_state_size = 1;
            chainTxData = ChainTxData{
                // Data from RPC: getchaintxstats 4096 000000895a110f46e59eb82bbc5bfb67fa314656009c295509c21b4999f5180a
                .nTime    = 1741019645,
                .tx_count = 16540736,
                .dTxRate  = 1.064918879911595,
            };
        } else {
            bin = *options.challenge;
            consensus.nMinimumChainWork = uint256{};
            consensus.defaultAssumeValid = uint256{};
            m_assumed_blockchain_size = 0;
            m_assumed_chain_state_size = 0;
            chainTxData = ChainTxData{
                0,
                0,
                0,
            };
            LogPrintf("Signet with challenge %s\n", HexStr(bin));
        }

        if (options.seeds) {
            vSeeds = *options.seeds;
        }

        m_chain_type = ChainType::SIGNET;
        consensus.signet_blocks = true;
        consensus.signet_challenge.assign(bin.begin(), bin.end());
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP34Height = 1;
        consensus.BIP34Hash = uint256{};
        consensus.BIP65Height = 1;
        consensus.BIP66Height = 1;
        consensus.CSVHeight = 1;
        consensus.SegwitHeight = 1;
        consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // two weeks
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.enforce_BIP94 = false;
        consensus.fPowNoRetargeting = false;
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256{"00000377ae000000000000000000000000000000000000000000000000000000"};
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].threshold = 1815; // 90%
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period = 2016;

        // Activation of Taproot (BIPs 340-342)
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].threshold = 1815; // 90%
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].period = 2016;

        // Deployment of Quantum-Safe Signatures
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].bit = 3;
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE; // Active on signet for testing
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].threshold = 1815; // 90%
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].period = 2016;

        // message start is defined as the first 4 bytes of the sha256d of the block script
        HashWriter h{};
        h << consensus.signet_challenge;
        uint256 hash = h.GetHash();
        std::copy_n(hash.begin(), 4, pchMessageStart.begin());

        nDefaultPort = 38333;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1598918400, 52613770, 0x1e0377ae, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256{"00000008819873e925422c1ff0f99f7cc9bbb232af63a077a480a3633bee1ef6"});
        assert(genesis.hashMerkleRoot == uint256{"4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"});

        m_assumeutxo_data = {
            {
                .height = 160'000,
                .hash_serialized = AssumeutxoHash{uint256{"fe0a44309b74d6b5883d246cb419c6221bcccf0b308c9b59b7d70783dbdf928a"}},
                .m_chain_tx_count = 2289496,
                .blockhash = consteval_ctor(uint256{"0000003ca3c99aff040f2563c2ad8f8ec88bd0fd6b8f0895cfaf1ef90353a62c"}),
            }
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "tb";

        fDefaultConsistencyChecks = false;
        m_is_mockable_chain = false;
    }
};

/**
 * Regression test: intended for private networks only. Has minimal difficulty to ensure that
 * blocks can be found instantly.
 */
class CRegTestParams : public CChainParams
{
public:
    explicit CRegTestParams(const RegTestOptions& opts)
    {
        m_chain_type = ChainType::REGTEST;
        consensus.signet_blocks = false;
        consensus.signet_challenge.clear();
        consensus.nSubsidyHalvingInterval = 150;
        consensus.BIP34Height = 1; // Always active unless overridden
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1;  // Always active unless overridden
        consensus.BIP66Height = 1;  // Always active unless overridden
        consensus.CSVHeight = 1;    // Always active unless overridden
        consensus.SegwitHeight = 0; // Always active unless overridden
        consensus.MinBIP9WarningHeight = 0;
        consensus.powLimit = uint256{"7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"};
        consensus.nPowTargetTimespan = 24 * 60 * 60; // one day
        consensus.nPowTargetSpacing = 10 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.enforce_BIP94 = opts.enforce_bip94;
        consensus.fPowNoRetargeting = true;

        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].threshold = 108; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].period = 144; // Faster than normal for regtest (144 instead of 2016)

        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].bit = 2;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].threshold = 108; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_TAPROOT].period = 144;

        // Deployment of Quantum-Safe Signatures
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].bit = 3;
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE; // Active on regtest for testing
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].nTimeout = Consensus::BIP9Deployment::NO_TIMEOUT;
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].min_activation_height = 0; // No activation delay
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].threshold = 108; // 75%
        consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].period = 144;

        consensus.nMinimumChainWork = uint256{};
        consensus.defaultAssumeValid = uint256{};

        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;
        nDefaultPort = 18444;
        nPruneAfterHeight = opts.fastprune ? 100 : 1000;
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        for (const auto& [dep, height] : opts.activation_heights) {
            switch (dep) {
            case Consensus::BuriedDeployment::DEPLOYMENT_SEGWIT:
                consensus.SegwitHeight = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_HEIGHTINCB:
                consensus.BIP34Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_DERSIG:
                consensus.BIP66Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_CLTV:
                consensus.BIP65Height = int{height};
                break;
            case Consensus::BuriedDeployment::DEPLOYMENT_CSV:
                consensus.CSVHeight = int{height};
                break;
            }
        }

        for (const auto& [deployment_pos, version_bits_params] : opts.version_bits_parameters) {
            consensus.vDeployments[deployment_pos].nStartTime = version_bits_params.start_time;
            consensus.vDeployments[deployment_pos].nTimeout = version_bits_params.timeout;
            consensus.vDeployments[deployment_pos].min_activation_height = version_bits_params.min_activation_height;
        }

        genesis = CreateGenesisBlock(1296688602, 2, 0x207fffff, 1, 50 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock == uint256{"0f9188f13cb7b2c71f2a335e3a4fc328bf5beb436012afca590b1a11466e2206"});
        assert(genesis.hashMerkleRoot == uint256{"4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b"});

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();
        vSeeds.emplace_back("dummySeed.invalid.");

        fDefaultConsistencyChecks = true;
        m_is_mockable_chain = true;

        m_assumeutxo_data = {
            {   // For use by unit tests
                .height = 110,
                .hash_serialized = AssumeutxoHash{uint256{"b952555c8ab81fec46f3d4253b7af256d766ceb39fb7752b9d18cdf4a0141327"}},
                .m_chain_tx_count = 111,
                .blockhash = consteval_ctor(uint256{"6affe030b7965ab538f820a56ef56c8149b7dc1d1c144af57113be080db7c397"}),
            },
            {
                // For use by fuzz target src/test/fuzz/utxo_snapshot.cpp
                .height = 200,
                .hash_serialized = AssumeutxoHash{uint256{"17dcc016d188d16068907cdeb38b75691a118d43053b8cd6a25969419381d13a"}},
                .m_chain_tx_count = 201,
                .blockhash = consteval_ctor(uint256{"385901ccbd69dff6bbd00065d01fb8a9e464dede7cfe0372443884f9b1dcf6b9"}),
            },
            {
                // For use by test/functional/feature_assumeutxo.py
                .height = 299,
                .hash_serialized = AssumeutxoHash{uint256{"d2b051ff5e8eef46520350776f4100dd710a63447a8e01d917e92e79751a63e2"}},
                .m_chain_tx_count = 334,
                .blockhash = consteval_ctor(uint256{"7cc695046fec709f8c9394b6f928f81e81fd3ac20977bb68760fa1faa7916ea2"}),
            },
        };

        chainTxData = ChainTxData{
            .nTime = 0,
            .tx_count = 0,
            .dTxRate = 0.001, // Set a non-zero rate to make it testable
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};

        bech32_hrp = "bcrt";
    }
};

std::unique_ptr<const CChainParams> CChainParams::SigNet(const SigNetOptions& options)
{
    return std::make_unique<const SigNetParams>(options);
}

std::unique_ptr<const CChainParams> CChainParams::RegTest(const RegTestOptions& options)
{
    return std::make_unique<const CRegTestParams>(options);
}

std::unique_ptr<const CChainParams> CChainParams::Main()
{
    return std::make_unique<const CMainParams>();
}

std::unique_ptr<const CChainParams> CChainParams::TestNet()
{
    return std::make_unique<const CTestNetParams>();
}

std::unique_ptr<const CChainParams> CChainParams::TestNet4()
{
    return std::make_unique<const CTestNet4Params>();
}
std::unique_ptr<const CChainParams> CChainParams::QSTestNet()
{
    return std::make_unique<const CQSTestNetParams>();
}

std::vector<int> CChainParams::GetAvailableSnapshotHeights() const
{
    std::vector<int> heights;
    heights.reserve(m_assumeutxo_data.size());

    for (const auto& data : m_assumeutxo_data) {
        heights.emplace_back(data.height);
    }
    return heights;
}

std::optional<ChainType> GetNetworkForMagic(const MessageStartChars& message)
{
    const auto mainnet_msg = CChainParams::Main()->MessageStart();
    const auto testnet_msg = CChainParams::TestNet()->MessageStart();
    const auto testnet4_msg = CChainParams::TestNet4()->MessageStart();
    const auto regtest_msg = CChainParams::RegTest({})->MessageStart();
    const auto signet_msg = CChainParams::SigNet({})->MessageStart();

    if (std::ranges::equal(message, mainnet_msg)) {
        return ChainType::MAIN;
    } else if (std::ranges::equal(message, testnet_msg)) {
        return ChainType::TESTNET;
    } else if (std::ranges::equal(message, testnet4_msg)) {
        return ChainType::TESTNET4;
    } else if (std::ranges::equal(message, regtest_msg)) {
        return ChainType::REGTEST;
    } else if (std::ranges::equal(message, signet_msg)) {
        return ChainType::SIGNET;
    }
    return std::nullopt;
}
