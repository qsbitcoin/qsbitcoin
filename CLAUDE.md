# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Context

This is QSBitcoin - a fork of Bitcoin Core implementing quantum-safe signatures using NIST-standardized post-quantum algorithms (ML-DSA-65 and SLH-DSA-192f) via liboqs v0.12.0+. The implementation maintains full backward compatibility through soft fork activation while providing a smooth migration path from ECDSA.

**Implementation Status**: 100% complete (July 5, 2025) - Full quantum signature functionality is operational. Quantum addresses can generate, receive, and spend funds. All critical features implemented and tested on regtest network. Fee structure simplified to match Bitcoin Core.

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

# Important test note: When testing quantum transactions, use QuantumTransactionSignatureChecker
# The standard TransactionSignatureChecker does not support quantum signatures

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
- Unified opcodes: OP_CHECKSIG_EX (0xb3), OP_CHECKSIGVERIFY_EX (0xb4)
- BIP9 soft fork activation (bit 3)

### Core Implementation Components

**Cryptography** (`src/crypto/`):
- `signature_scheme.h` - ISignatureScheme abstraction
- `quantum_key.h/cpp` - CQuantumKey, CQuantumPubKey classes
- `liboqs/` - Integrated OQS library (ML-DSA, SLH-DSA only)

**Script System** (`src/script/`):
- Unified opcodes: OP_CHECKSIG_EX (0xb3), OP_CHECKSIGVERIFY_EX (0xb4)
- SCRIPT_VERIFY_QUANTUM_SIGS flag (bit 21)
- EvalChecksigQuantum() for quantum signature verification
- Algorithm ID in signature data (0x02=ML-DSA, 0x03=SLH-DSA)

**Wallet** (`src/wallet/`):
- Descriptor-based architecture with quantum descriptors (`qpkh`)
- DescriptorScriptPubKeyMan integration complete
- SigningProvider extended with quantum key methods
- Temporary QuantumKeyStore (migration to descriptors pending)

**RPC Commands**:
- `getnewaddress` - Generate addresses (use algorithm parameter: "ecdsa" for standard, "ml-dsa" or "slh-dsa" for quantum)
- `getquantuminfo` - Wallet quantum capabilities
- `estimatesmartfee` - Standard fee estimation (no quantum discounts)
- No special quantum fee commands (fees based on transaction size only)

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
- **Script patterns**: Quantum scripts recognized by OP_CHECKSIG_EX with algorithm ID prefix
- **Weight calculations**: Special factors for quantum signatures (2x-3x vs 4x ECDSA)
- **Activation status**: Testnet/Regtest ALWAYS_ACTIVE, Mainnet NEVER_ACTIVE

### Recently Completed (June 27-July 5, 2025)

1. **Quantum Descriptors**: Full `qpkh()` descriptor implementation in descriptor.cpp
2. **SPKM Integration**: DescriptorScriptPubKeyMan now supports quantum signing
3. **P2WSH Implementation**: All quantum addresses use standard bech32 format
4. **Architecture Transition**: Moved from legacy ScriptPubKeyMan to descriptors
5. **Key Generation Fix** (June 30, 2025): Fixed witness script pubkey mismatch
   - Modified `SetupQuantumDescriptor` to accept pre-generated keys
   - Updated `GetNewQuantumDestination` to pass its key to descriptor setup
   - Added verification step to ensure key retrieval after generation
   - New quantum addresses can now successfully sign transactions
6. **Test Fix** (July 1, 2025): Fixed quantum witness script execution tests
   - Fixed virtual function override issue (SignatureSchemeID vs uint8_t)
   - Updated tests to use QuantumTransactionSignatureChecker instead of TransactionSignatureChecker
   - All quantum transaction tests now pass
7. **Witness Script Fix** (July 2, 2025): Fixed quantum address spending issue
   - Fixed witness script format mismatch preventing quantum addresses from spending
   - SignStep expects script format with push operations, not raw data
   - QPKHDescriptor now uses proper script `<<` operator for witness scripts
   - Both ML-DSA and SLH-DSA addresses can now successfully spend funds
8. **Critical Bug Fixes** (July 2, 2025): Fixed three critical issues found during testing
   - **Buffer Overflow Fix**: Fixed QuantumPubkeyProvider incorrectly assuming CKeyID is 32 bytes (it's 20 bytes)
   - **Null Pointer Fix**: Fixed crash in VerifyScript when ScriptErrorString called with null serror pointer
   - **Algorithm ID Fix**: Fixed quantum signatures missing algorithm ID prefix for P2WSH transactions
     - Sign.cpp now prepends algorithm ID (0x02 for ML-DSA, 0x03 for SLH-DSA) to signatures
     - This matches what EvalChecksigQuantum expects during verification

8. **Opcode Consolidation** (July 2, 2025): Unified quantum opcodes
   - Replaced separate OP_CHECKSIG_ML_DSA/SLH_DSA with unified OP_CHECKSIG_EX
   - Algorithm ID now included in signature data (0x02 for ML-DSA, 0x03 for SLH-DSA)
   - Simplifies script validation and improves extensibility

9. **Fee Structure Simplification** (July 5, 2025): Aligned with Bitcoin Core
   - Removed all quantum fee discounts (10% ML-DSA, 5% SLH-DSA)
   - Removed 1.5x quantum transaction fee multiplier
   - Fees now based purely on transaction size/weight
   - Quantum signatures naturally pay more due to larger size

### Next Critical Steps

1. ✅ **Complete Testing**: Full quantum transaction cycle verified working on regtest
2. **Integration Testing**: Full end-to-end testing on testnet
3. **Performance Testing**: Verify large signature handling under load
4. **Documentation**: Update all documentation with completed implementation details

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
3. **Mine blocks at key points** to ensure transactions are confirmed and UTXOs are properly tracked
4. **Complete test workflow**:

```bash
# Clean start - remove old data
pkill -f "bitcoind.*regtest" || true
rm -rf ~/.bitcoin/regtest/

# Start bitcoind in regtest mode with fallback fee
./build/bin/bitcoind -regtest -daemon -fallbackfee=0.00001
sleep 3  # Wait for startup

# Create fresh wallet
./build/bin/bitcoin-cli -regtest createwallet "test_wallet"

# Generate initial blocks for spendable coins
MINER=$(./build/bin/bitcoin-cli -regtest getnewaddress)
./build/bin/bitcoin-cli -regtest generatetoaddress 101 $MINER

# Create quantum addresses (use standard getnewaddress with algorithm parameter)
MLDSA=$(./build/bin/bitcoin-cli -regtest getnewaddress "" "bech32" "ml-dsa")
SLHDSA=$(./build/bin/bitcoin-cli -regtest getnewaddress "" "bech32" "slh-dsa")
echo "ML-DSA address: $MLDSA"
echo "SLH-DSA address: $SLHDSA"

# Send funds to quantum addresses
./build/bin/bitcoin-cli -regtest sendtoaddress $MLDSA 10.0
./build/bin/bitcoin-cli -regtest sendtoaddress $SLHDSA 10.0

# IMPORTANT: Mine blocks to confirm transactions
./build/bin/bitcoin-cli -regtest generatetoaddress 6 $MINER

# Verify quantum UTXOs are tracked
./build/bin/bitcoin-cli -regtest listunspent | grep -A5 -B5 "bcrt1q"

# Test spending from quantum addresses
DEST=$(./build/bin/bitcoin-cli -regtest getnewaddress)
./build/bin/bitcoin-cli -regtest sendtoaddress $DEST 5.0

# Mine to confirm the spend
./build/bin/bitcoin-cli -regtest generatetoaddress 1 $MINER

# Check quantum wallet info
./build/bin/bitcoin-cli -regtest getquantuminfo

# Test wallet restart (UTXOs should persist)
./build/bin/bitcoin-cli -regtest stop
sleep 2
./build/bin/bitcoind -regtest -daemon -fallbackfee=0.00001
sleep 3
./build/bin/bitcoin-cli -regtest loadwallet "test_wallet"
./build/bin/bitcoin-cli -regtest listunspent  # Should still show quantum UTXOs

# Clean shutdown
./build/bin/bitcoin-cli -regtest stop
```

**Important**: 
- Always mine blocks after sending to quantum addresses to ensure UTXOs are confirmed
- Mine at least 6 blocks for proper confirmation before testing spends
- Always include `-fallbackfee=0.00001` when starting bitcoind in regtest mode
- Quantum addresses use standard `getnewaddress` with algorithm parameter, not `getnewquantumaddress`

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
(gdb) run -regtest -debug=all -fallbackfee=0.00001

# Analyze core dump if available
sudo coredumpctl gdb bitcoind

# Check for assertion failures in debug.log
grep -i "assertion\|error\|fault" ~/.bitcoin/regtest/debug.log
```

### Code Quality Improvements (July 3-5, 2025)

1. **Enum Consolidation**: Unified `SignatureSchemeID` enum
   - Removed duplicate enum definitions
   - Now using single `quantum::SignatureSchemeID` throughout codebase
   - Values: `SCHEME_ECDSA` (0x01), `SCHEME_ML_DSA_65` (0x02), `SCHEME_SLH_DSA_192F` (0x03)

2. **Magic Number Elimination**: Replaced hardcoded values
   - Added `MIN_QUANTUM_SIG_SIZE_THRESHOLD` constant (100 bytes)
   - Used for detecting quantum signatures vs ECDSA signatures

3. **Test Script Cleanup**: Removed 10 obsolete shell scripts
   - All manual test scripts removed in favor of proper unit/functional tests
   - Testing now integrated with standard Bitcoin Core test framework