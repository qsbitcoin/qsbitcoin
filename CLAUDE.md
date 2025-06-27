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
- **SLH-DSA-192f**: High-value storage, ~49KB signatures (cold storage)
- **ECDSA**: Legacy support, 71 bytes (migration period)

**Address Format**:
- Base58Check encoding with Q prefixes for display
- Q1xxx: ML-DSA addresses (P2QPKH_ML_DSA)
- Q2xxx: SLH-DSA addresses (P2QPKH_SLH_DSA)  
- Q3xxx: Quantum script hash (P2QSH)
- Prefixes transparently added for display, removed internally

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
- `getnewquantumaddress` - Generate quantum addresses
- `validatequantumaddress` - Validate and get algorithm info
- `getquantuminfo` - Wallet quantum capabilities
- `signmessagewithscheme` - Sign messages with quantum schemes
- `estimatequantumfee` - Fee estimation with discounts

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
3. **Q Prefix Display**: Transparent handling of Q1/Q2/Q3 address prefixes
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