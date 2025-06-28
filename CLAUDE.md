# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Context

This is QSBitcoin - a fork of Bitcoin Core implementing quantum-safe signatures using NIST-standardized post-quantum algorithms (ML-DSA-65 and SLH-DSA-192f) via liboqs v0.12.0+. The implementation maintains full backward compatibility through soft fork activation while providing a smooth migration path from ECDSA.

**Implementation Status**: ~92% complete - Core quantum signature functionality is fully implemented including descriptors and wallet integration.

## Build Commands

```bash
# Clean and configure build (prefer ninja for speed)
rm -rf build/
if command -v ninja >/dev/null 2>&1; then
    cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
else
    cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON
fi

# Build with all CPU threads
if [ -f build/build.ninja ]; then
    ninja -C build -j$(nproc)
else
    cmake --build build -j$(nproc)
fi

# Build with specific options
cmake -B build -DBUILD_GUI=ON              # With GUI
cmake -B build -DENABLE_WALLET=OFF         # Without wallet
cmake -B build -DSANITIZERS=address,undefined  # With sanitizers
```

**Build Timeouts**: Use 600000ms (10 min) for full builds, 300000ms (5 min) for incremental builds.

## Testing Commands

```bash
# Unit tests
ctest --test-dir build -j$(nproc) --output-on-failure
build/bin/test_bitcoin --run_test=<test_suite_name>  # Specific test
build/bin/test_bitcoin --list_content                 # List tests

# Run all quantum-specific tests
./build/bin/test_bitcoin -t "*quantum*"
./build/bin/test_bitcoin -t "*liboqs*"

# Functional tests
build/test/functional/test_runner.py -j$(nproc)      # All tests parallel
build/test/functional/test_runner.py <test_name>.py  # Specific test

# Debugging tests
build/test/functional/<test>.py --timeout-factor 0   # Disable timeout
build/test/functional/test_runner.py --nocleanup     # Keep test data
```

## Linting and Code Quality

```bash
# Run all linters
RUST_BACKTRACE=1 cargo run --manifest-path "./test/lint/test_runner/Cargo.toml"

# Code formatting (C++)
contrib/devtools/clang-format-diff.py
```

## High-Level Architecture

### Quantum-Safe Implementation Details

**Signature Schemes** (via liboqs):
- **ML-DSA-65**: Standard transactions, ~3.3KB signatures (99% of cases)
- **SLH-DSA-192f**: High-value storage, ~35KB signatures (cold storage)
- **ECDSA**: Legacy support, 71 bytes (migration period)

**Address Format**:
- Standard bech32 P2WSH encoding for all quantum addresses
- ML-DSA addresses: bc1q... (mainnet) or bcrt1q... (regtest)
- SLH-DSA addresses: bc1q... (mainnet) or bcrt1q... (regtest)
- No special prefixes - quantum addresses look identical to regular P2WSH
- Large signatures handled transparently in witness data

**Transaction Format**:
```cpp
script_sig: [scheme_id:1 byte][sig_len:varint][signature][pubkey_len:varint][pubkey]
```

**Consensus Changes**:
- MAX_STANDARD_TX_WEIGHT_QUANTUM = 1MB (vs 400KB standard)
- Quantum opcodes using repurposed NOPs (OP_NOP4-7)
- BIP9 soft fork activation (bit 3)
- Fee discounts: ML-DSA 10%, SLH-DSA 5%

### Core Implementation Components

**Cryptography** (`src/crypto/`):
- `signature_scheme.h` - ISignatureScheme abstraction
- `quantum_key.h/cpp` - CQuantumKey, CQuantumPubKey classes
- `liboqs/` - Integrated OQS library (ML-DSA, SLH-DSA only)

**Script System** (`src/script/`):
- Quantum opcodes: OP_CHECKSIG_ML_DSA (0xb3), OP_CHECKSIG_SLH_DSA (0xb4)
- SCRIPT_VERIFY_QUANTUM_SIGS flag (bit 21)
- EvalChecksigQuantum() for quantum signature verification

**Wallet** (`src/wallet/`):
- Descriptor-based architecture with quantum descriptors (`qpkh`)
- DescriptorScriptPubKeyMan integration complete
- SigningProvider extended with quantum key methods
- Temporary QuantumKeyStore (migration to descriptors pending)

**RPC Commands**:
- `getnewaddress` - Generate addresses (use algorithm parameter: "ecdsa" for standard, "ml-dsa" or "slh-dsa" for quantum)
- `getquantuminfo` - Wallet quantum capabilities
- `estimatesmartfee` - Fee estimation (use signature_type parameter for quantum discounts)
- `estimatetxfee` - Transaction fee estimation for any signature type ("ecdsa", "ml-dsa", or "slh-dsa")

### Development Workflow

1. **ALWAYS READ FIRST**: Before starting any work, read both QSBITCOIN_PLAN.md and QSBITCOIN_TASKS.md to understand current status and priorities
2. **ALWAYS UPDATE**: After completing any task or discovering new information, update both QSBITCOIN_PLAN.md and QSBITCOIN_TASKS.md - these are living documents that track progress
3. **Always use SSH** for git: `git@github.com:qsbitcoin/qsbitcoin.git`
4. **Clean build** before major changes: `rm -rf build/`
5. **Run tests** after each implementation: `ctest --test-dir build -j$(nproc)`
6. **Use all CPU threads**: Include `-j$(nproc)` in build commands

### Critical Documentation Requirements

**QSBITCOIN_PLAN.md** - Technical design and implementation strategy
- Update "Implementation Status" section when completing features
- Add to "Implementation Learnings" when discovering technical details
- Update "Next Critical Steps" to reflect current priorities
- Increment version number and date at bottom when making significant updates

**QSBITCOIN_TASKS.md** - Detailed task tracking and progress
- Mark tasks as completed (🟢) when finished
- Update task descriptions with implementation notes
- Add new tasks when discovering additional work
- Update overall progress percentages
- Document any blockers or issues in task notes

### Critical Implementation Notes

- **No HD derivation** for quantum keys (BIP32 not supported)
- **Non-copyable keys**: Use unique_ptr for CQuantumKey objects
- **Global namespace**: Use `::quantum` to avoid wallet namespace conflicts
- **Pubkey as IV**: Use pubkey hash as IV for deterministic encryption
- **Script patterns**: Quantum scripts recognized by OP_CHECKSIG_ML_DSA/SLH_DSA
- **Weight calculations**: Special factors for quantum signatures (2x-3x vs 4x ECDSA)
- **Activation status**: Testnet/Regtest ALWAYS_ACTIVE, Mainnet NEVER_ACTIVE

### Recently Completed (June 27, 2025)

1. **Quantum Descriptors**: Full `qpkh()` descriptor implementation in descriptor.cpp
2. **SPKM Integration**: DescriptorScriptPubKeyMan now supports quantum signing
3. **P2WSH Implementation**: All quantum addresses use standard bech32 format
4. **Architecture Transition**: Moved from legacy ScriptPubKeyMan to descriptors

### Next Critical Steps

1. **Wallet Migration** (Task 6.5): Move from temporary keystore to descriptor system
2. **Database Updates** (Task 6.6): Update wallet DB for descriptor-based quantum keys
3. **Remove Temporary Code**: Eliminate global QuantumKeyStore after migration
4. **Integration Testing**: Full end-to-end testing on testnet

### Testing Current Implementation

```bash
# Build and run quantum tests
ninja -C build -j$(nproc) && ./build/bin/test_bitcoin -t "*quantum*"

# Key tests to verify
./build/bin/test_bitcoin -t quantum_descriptor_tests     # Descriptor parsing
./build/bin/test_bitcoin -t quantum_descriptor_wallet_tests  # SPKM integration
./build/bin/test_bitcoin -t quantum_wallet_tests         # Wallet functionality
./build/bin/test_bitcoin -t quantum_activation_tests     # Soft fork logic
```

### Manual Testing with bitcoind and bitcoin-cli

When testing with `bitcoind` on regtest network:

1. **Always run bitcoin-cli commands directly** without helper shell scripts
2. **Delete and recreate wallet files on every test** to ensure clean state
3. **Example test workflow**:

```bash
# Start bitcoind in regtest mode
./build/bin/bitcoind -regtest -daemon

# Create fresh wallet (delete all regtest wallets first)
./build/bin/bitcoin-cli -regtest unloadwallet "test_wallet" 2>/dev/null
rm -rf ~/.bitcoin/regtest/wallets/*
./build/bin/bitcoin-cli -regtest createwallet "test_wallet"

# Generate blocks for testing
./build/bin/bitcoin-cli -regtest generatetoaddress 101 $(./build/bin/bitcoin-cli -regtest getnewaddress)

# Test quantum functionality
./build/bin/bitcoin-cli -regtest getnewquantumaddress "ML-DSA-65"
./build/bin/bitcoin-cli -regtest getnewquantumaddress "SLH-DSA-192f"

# Clean shutdown
./build/bin/bitcoin-cli -regtest stop
```

**Important**: Always use fresh wallets for each test session to avoid state contamination from previous tests.

### System Administration and Debugging

When debugging crashes or system-level issues:

1. **Use sudo for privileged operations**:
```bash
# Check system logs for crash information
sudo dmesg | tail -50

# Check for core dumps
sudo coredumpctl list
sudo coredumpctl info <pid>

# Monitor system resources
sudo htop

# Check system journal for bitcoind crashes
sudo journalctl -u bitcoind -n 100
sudo journalctl -xe | grep bitcoin
```

2. **Common permission-required operations**:
- `dmesg` - Kernel ring buffer (requires sudo)
- `coredumpctl` - Core dump analysis (requires sudo)
- System logs in `/var/log/` (may require sudo)
- Process tracing with `strace` (may require sudo for other users' processes)
- Network debugging with `tcpdump` (requires sudo)

3. **Debugging bitcoind crashes**:
```bash
# Enable core dumps for debugging
ulimit -c unlimited

# Run bitcoind with gdb
gdb ./build/bin/bitcoind
(gdb) run -regtest -debug=all

# Analyze core dump if available
sudo coredumpctl gdb bitcoind

# Check for assertion failures in debug.log
grep -i "assertion\|error\|fault" ~/.bitcoin/regtest/debug.log
```