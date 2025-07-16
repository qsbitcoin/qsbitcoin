# QSBitcoin Testnet Configuration Guide

This guide outlines all the changes needed to create a new QSBitcoin testnet that starts from genesis.

## Key Parameters to Modify

### 1. Network Magic Bytes (pchMessageStart)
Located in: `src/kernel/chainparams.cpp` in `CTestNetParams` class

Current Bitcoin testnet3:
```cpp
pchMessageStart[0] = 0x0b;
pchMessageStart[1] = 0x11;
pchMessageStart[2] = 0x09;
pchMessageStart[3] = 0x07;
```

Suggested for QSBitcoin testnet:
```cpp
pchMessageStart[0] = 0x51; // 'Q' for Quantum
pchMessageStart[1] = 0x53; // 'S' for Safe
pchMessageStart[2] = 0x42; // 'B' for Bitcoin
pchMessageStart[3] = 0x54; // 'T' for Testnet
```

### 2. Default Ports
Located in: `src/kernel/chainparams.cpp` in `CTestNetParams` class

Current Bitcoin testnet3:
- P2P Port: 18333
- RPC Port: 18332 (defined in `src/chainparamsbase.cpp`)

Suggested for QSBitcoin testnet:
- P2P Port: 28333
- RPC Port: 28332

### 3. Address Prefixes
Located in: `src/kernel/chainparams.cpp` in `CTestNetParams` class

#### Base58 Prefixes (Legacy addresses)
Current Bitcoin testnet3:
```cpp
base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111); // 'm' or 'n'
base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196); // '2'
```

Suggested for QSBitcoin testnet (same as Bitcoin to maintain compatibility):
```cpp
base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);
base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);
```

#### Bech32 HRP (Human Readable Part)
Current Bitcoin testnet3:
```cpp
bech32_hrp = "tb";
```

QSBitcoin HRP convention (2 characters):
```cpp
// Mainnet
bech32_hrp = "qs";  // Quantum Safe

// Testnet
bech32_hrp = "qt";  // Quantum Testnet
```

### 4. Genesis Block
Located in: `src/kernel/chainparams.cpp` in `CTestNetParams` class

You'll need to create a new genesis block with:
- New timestamp message
- New nTime (unix timestamp)
- New nNonce (will need to be mined)
- New genesis hash

Example:
```cpp
const char* pszTimestamp = "QSBitcoin Testnet Genesis - Quantum Safe Since 2025/07/16";
genesis = CreateGenesisBlock(1736985600, 0, 0x1d00ffff, 1, 50 * COIN);
// nNonce will need to be computed to meet difficulty target
```

### 5. DNS Seeds
Located in: `src/kernel/chainparams.cpp` in `CTestNetParams` class

Remove all existing seeds and add your own:
```cpp
vSeeds.clear();
vSeeds.emplace_back("seed1.qsbitcoin-testnet.example.com");
vSeeds.emplace_back("seed2.qsbitcoin-testnet.example.com");
```

### 6. Checkpoints
Located in: `src/kernel/chainparams.cpp` in `CTestNetParams` class

Start with empty checkpoints:
```cpp
consensus.nMinimumChainWork = uint256{};
consensus.defaultAssumeValid = uint256{};
```

### 7. Chain Parameters
Located in: `src/kernel/chainparams.cpp` in `CTestNetParams` class

Key parameters to consider:
```cpp
consensus.nSubsidyHalvingInterval = 210000; // Keep same as Bitcoin
consensus.nPowTargetTimespan = 14 * 24 * 60 * 60; // Two weeks
consensus.nPowTargetSpacing = 10 * 60; // 10 minutes
consensus.fPowAllowMinDifficultyBlocks = true; // Allow minimum difficulty for testing
```

### 8. Quantum Signature Deployment
Already configured in current code:
```cpp
consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].nStartTime = Consensus::BIP9Deployment::ALWAYS_ACTIVE;
```

## Implementation Steps

1. **Create a new chain type** (optional):
   - Add `QSTESTNET` to `ChainType` enum in `src/util/chaintype.h`
   - Create `CQSTestNetParams` class in `src/kernel/chainparams.cpp`

2. **Mine the genesis block**:
   - Set initial nNonce to 0
   - Run a mining loop to find valid nonce
   - Update genesis block hash and merkle root assertions

3. **Update RPC port**:
   - Modify `src/chainparamsbase.cpp` to handle the new RPC port

4. **Update network name strings**:
   - Search for "testnet3" references and add corresponding "qstestnet" entries

5. **Fixed seeds**:
   - Generate new fixed seeds in `src/chainparamsseeds.h` once you have stable nodes

## Testing the New Testnet

1. Build with your changes:
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

2. Start the node:
```bash
./build/bin/bitcoind -qstestnet -debug=all
```

3. Generate initial blocks:
```bash
./build/bin/bitcoin-cli -qstestnet -rpcport=28332 generatetoaddress 100 <your-address>
```

4. Test quantum addresses:
```bash
./build/bin/bitcoin-cli -qstestnet -rpcport=28332 getnewaddress "" "bech32" "ml-dsa"
./build/bin/bitcoin-cli -qstestnet -rpcport=28332 getnewaddress "" "bech32" "slh-dsa"
```

Example addresses will look like:
- ML-DSA: `qt1q...` (instead of `tb1q...`)
- SLH-DSA: `qt1q...` (instead of `tb1q...`)
- Mainnet would use: `qs1q...` (instead of `bc1q...`)

## Additional Considerations

1. **Chain ID**: Consider if you need a unique chain ID to prevent replay attacks
2. **Version bits**: Ensure quantum signatures are always active on your testnet
3. **Minimum chain work**: Start with 0 and update as the chain grows
4. **Block size/weight limits**: Consider if you need different limits for quantum transactions

## Mainnet Configuration

When ready to update mainnet configuration, modify `CMainParams` in `src/kernel/chainparams.cpp`:

```cpp
// Change bech32 HRP for quantum-safe mainnet
bech32_hrp = "qs";  // Instead of "bc"

// Update network magic bytes
pchMessageStart[0] = 0x51; // 'Q'
pchMessageStart[1] = 0x53; // 'S'  
pchMessageStart[2] = 0x4D; // 'M' for Mainnet
pchMessageStart[3] = 0x42; // 'B' for Bitcoin

// Keep quantum signatures deployment as NEVER_ACTIVE until ready
consensus.vDeployments[Consensus::DEPLOYMENT_QUANTUM_SIGS].nStartTime = Consensus::BIP9Deployment::NEVER_ACTIVE;
```

This maintains backward compatibility while clearly distinguishing quantum-safe addresses.