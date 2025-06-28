# DevNotesTasks.md - QSBitcoin Developer Quick Reference

## Current Status (June 28, 2025)
**Implementation**: ~95% complete - Core quantum signature functionality fully implemented
**Architecture**: Transitioned to descriptor-based wallet system with P2WSH for all quantum addresses

## Quick Commands

```bash
# Clean build
rm -rf build/ && cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON && ninja -C build -j$(nproc)

# Run tests with multiple threads
ctest --test-dir build -j$(nproc) --output-on-failure      # All tests
./build/bin/test_bitcoin -t "*quantum*"                     # All quantum unit tests
./build/bin/test_bitcoin -t "*liboqs*"                      # liboqs integration tests
./build/bin/test_bitcoin --list_content                     # List all available tests
build/test/functional/test_runner.py -j$(nproc)             # All functional tests
build/test/functional/test_runner.py wallet_quantum.py      # Quantum functional test

# Start regtest daemon
./build/bin/bitcoind -regtest -daemon

# Create fresh wallet (always delete old ones first)
rm -rf ~/.bitcoin/regtest/wallets/*
./build/bin/bitcoin-cli -regtest createwallet "test_wallet"

# Generate quantum address
./build/bin/bitcoin-cli -regtest getnewquantumaddress "ML-DSA-65"   # Returns bcrt1q... address
./build/bin/bitcoin-cli -regtest getnewquantumaddress "SLH-DSA-192f" # Returns bcrt1q... address
```

## Critical Architecture Notes

### Quantum Key Types
- **ML-DSA-65**: Standard txs, ~3.3KB sigs (99% use cases)
- **SLH-DSA-192f**: High-value, ~35KB sigs (cold storage)
- **No HD derivation**: BIP32 not supported for quantum keys
- **Non-copyable**: Use unique_ptr for CQuantumKey

### Address Format
- All quantum addresses use standard P2WSH (bech32) format
- **ML-DSA addresses**: Standard bech32 addresses (bc1q... on mainnet, bcrt1q... on regtest)
- **SLH-DSA addresses**: Standard bech32 addresses (bc1q... on mainnet, bcrt1q... on regtest)
- No special prefixes - quantum addresses look identical to regular P2WSH addresses
- Large signatures handled transparently in witness data

### Transaction Format
```cpp
// Witness structure for quantum transactions
witness: [signature][pubkey]
// Script: OP_0 <20-byte-script-hash>
// Witness script: <quantum_pubkey> OP_CHECKSIG_ML_DSA/OP_CHECKSIG_SLH_DSA
```

### Key Implementation Files
- `src/crypto/quantum_key.h` - CQuantumKey, CQuantumPubKey classes
- `src/script/quantum_witness.h` - P2WSH creation functions
- `src/script/descriptor.cpp` - Quantum descriptor support (qpkh)
- `src/wallet/quantum_keystore.cpp` - Temporary global keystore
- `src/wallet/scriptpubkeyman.cpp` - SPKM quantum integration

## Current Issues & Next Steps

### Recently Completed (June 27-28, 2025)
1. ✅ **Quantum Descriptors**: Full `qpkh()` descriptor implementation
2. ✅ **SPKM Integration**: DescriptorScriptPubKeyMan supports quantum signing
3. ✅ **P2WSH Addresses**: All quantum addresses use standard bech32 format
4. ✅ **P2WSH Migration**: All quantum addresses use witness scripts
5. ✅ **Database Persistence**: Quantum keys survive wallet restarts
6. ✅ **Test Suite Updates**: Cleaned up obsolete test scripts, updated functional tests
7. ✅ **SLH-DSA Size Fix**: Corrected signature size from 49KB to 35KB in documentation

### Critical Issue: Wallet Ownership of Quantum Addresses (June 28, 2025)
**Problem**: Wallet shows `ismine: false` for quantum P2WSH addresses
**Root Cause**: Quantum addresses are created outside the descriptor flow, breaking wallet ownership tracking

#### Implementation Status
1. **✅ Descriptor language extended** for quantum P2WSH:
   - Added `QPKDescriptor` class that works inside `wsh()`
   - Parser recognizes `qpk()` function for quantum public keys
   - Generates proper witness scripts with quantum opcodes
   
2. **✅ Core implementation complete**:
   - Modified descriptor parser to recognize `qpk()` within `wsh()`
   - `QPKDescriptor::MakeScripts` generates quantum witness scripts
   - Witness scripts properly include quantum pubkey + OP_CHECKSIG_ML_DSA/SLH_DSA
   - Added to ParseScript function with proper context checking

3. **🔧 Integration pending**:
   - Need to update `getnewquantumaddress` to create and import descriptors
   - Wallet descriptor import mechanism needs to be integrated
   - Current workaround: witness scripts stored in global quantum keystore

4. **Current flow**:
   - `getnewquantumaddress` creates P2WSH manually
   - Witness script stored in global quantum keystore with correct script ID
   - `AddScriptPubKey` called to register P2WSH script with descriptor SPKM
   - `PopulateQuantumSigningProvider` retrieves witness script for signing
   - IsMine logic still returns false - script mapping not persisting properly

5. **Discovered Issues**:
   - Descriptor SPKM's `m_map_script_pub_keys` not persisting quantum scripts
   - Database constraint violations when reloading wallet with quantum keys
   - Manual script addition via AddScriptPubKey only works in memory
   - Scripts need to be generated through descriptor expansion for persistence
   - Current approach bypasses the descriptor system's script generation

6. **Root Cause Analysis**:
   - The wallet's descriptor system expects scripts to be generated through descriptor expansion
   - Manually added scripts (via AddScriptPubKey) are not persisted to the database
   - When wallet reloads, it re-expands descriptors but doesn't regenerate quantum scripts
   - Proper solution requires creating and importing `wsh(qpk())` descriptors
   - The descriptor SPKM would then automatically track and persist the P2WSH scripts

### Immediate TODOs
1. **Fix wallet ownership** (CRITICAL - PARTIALLY COMPLETE):
   - ✅ Implemented `QPKDescriptor` class for `qpk()` inside `wsh()`
   - ✅ Parser recognizes quantum descriptors
   - ✅ Updated `getnewquantumaddress` to add scripts to SPKM
   - ✅ Modified IsMine to check for quantum P2WSH addresses
   - ❌ IsMine still returns false - script mapping not persisting
   - ❌ Bitcoind crashes intermittently with quantum operations
   - 🔧 Need proper descriptor import flow instead of manual script addition

2. **Clean up temporary code**:
   - Remove global `g_quantum_keystore` after descriptor fix
   - Remove manual witness script storage
   - Consolidate quantum wallet setup functions

3. **Integration testing**:
   - Test wallet ownership of quantum addresses
   - Verify spending from quantum addresses works
   - Multi-wallet quantum transaction tests
   - Fee estimation validation

4. **Documentation**:
   - Update RPC help text with examples
   - Document quantum descriptor syntax
   - Migration guide for users

### Known Issues
1. **Wallet ownership**: Quantum addresses show `ismine: false` (fixing now)
2. **Temporary keystore**: Still using global keystore alongside descriptors
3. **Test coverage**: Some transaction validation tests need proper context
4. **Migration tools**: ECDSA→quantum migration utilities not implemented

## Key Development Learnings

### Descriptor Integration
- Quantum descriptors (`qpkh()`) work within existing descriptor framework
- QuantumPubkeyProvider handles key parsing and script generation
- SigningProvider extended with GetQuantumKey/HaveQuantumKey methods
- PopulateQuantumSigningProvider bridges keystore to signing provider

### P2WSH Implementation
- Large signatures (35KB for SLH-DSA) require witness scripts
- CreateQuantumWitnessScript() generates appropriate witness script
- Script interpreter updated to handle quantum opcodes in witness
- Transaction signing modified in SignStep for quantum witnesses

### Consensus Changes
- MAX_STANDARD_TX_WEIGHT_QUANTUM = 1MB (vs 400KB standard)
- Quantum opcodes: OP_CHECKSIG_ML_DSA (0xb3), OP_CHECKSIG_SLH_DSA (0xb4)
- SCRIPT_VERIFY_QUANTUM_SIGS flag (bit 21)
- Soft fork: Testnet ALWAYS_ACTIVE, Mainnet NEVER_ACTIVE

### Fee Structure
- Base fee × 1.5 × discount factor
- ML-DSA: 10% discount (0.9 factor)
- SLH-DSA: 5% discount (0.95 factor)
- Implemented in validation.cpp PreChecks

## Testing Checklist

### Unit Tests (src/test/)
```bash
# Run all unit tests with multiple threads
ctest --test-dir build -j$(nproc) --output-on-failure

# Core quantum functionality tests
./build/bin/test_bitcoin -t quantum_key_tests              # Key generation
./build/bin/test_bitcoin -t quantum_address_tests          # Address format
./build/bin/test_bitcoin -t quantum_descriptor_tests       # Descriptor parsing
./build/bin/test_bitcoin -t quantum_wallet_tests           # Wallet integration
./build/bin/test_bitcoin -t script_quantum_tests_simple    # Script validation
./build/bin/test_bitcoin -t quantum_activation_tests       # Soft fork activation
./build/bin/test_bitcoin -t quantum_consensus_tests        # Consensus rules
./build/bin/test_bitcoin -t quantum_fee_tests              # Fee calculations
./build/bin/test_bitcoin -t quantum_transaction_tests      # Transaction validation
./build/bin/test_bitcoin -t quantum_wallet_encryption_tests # Wallet encryption
./build/bin/test_bitcoin -t qs_signature_checker_tests     # Signature verification
./build/bin/test_bitcoin -t liboqs_tests                   # liboqs integration
./build/bin/test_bitcoin -t signature_scheme_tests         # Signature abstraction

# Wallet-specific quantum tests
./build/bin/test_bitcoin -t quantum_descriptor_wallet_tests # From src/wallet/test/

# Run all quantum tests with pattern matching
./build/bin/test_bitcoin -t "*quantum*"
./build/bin/test_bitcoin -t "*liboqs*"
./build/bin/test_bitcoin -t "*ml_dsa*"
./build/bin/test_bitcoin -t "*slh_dsa*"
```

### Functional Tests (test/functional/)
```bash
# Run all functional tests in parallel
cd build/test && python3 functional/test_runner.py -j$(nproc)

# Run quantum-specific functional tests
cd build/test && python3 functional/test_runner.py wallet_quantum.py feature_quantum_p2wsh.py

# Run with extended timeout for debugging
cd build/test && python3 functional/test_runner.py wallet_quantum.py --timeout-factor 0

# Keep test data for inspection
cd build/test && python3 functional/test_runner.py --nocleanup wallet_quantum.py
```

### Manual Testing
1. Create wallet with quantum support
2. Generate ML-DSA and SLH-DSA addresses
3. Mine blocks to quantum address
4. Send from quantum address
5. Verify witness structure
6. Test wallet restart persistence

### Test File Locations
- **Unit tests**: `src/test/*_tests.cpp`
- **Wallet tests**: `src/wallet/test/*_tests.cpp`
- **Functional tests**: `test/functional/*.py`
- **Fuzz tests**: `src/test/fuzz/`
- **Manual test scripts**: Project root `test_*.sh`

## Common Debugging

```bash
# Check for crashes
sudo dmesg | tail -50
sudo journalctl -xe | grep bitcoin

# Debug with gdb
gdb ./build/bin/bitcoind
(gdb) run -regtest -debug=all

# Check specific logs
grep -i "quantum\|error" ~/.bitcoin/regtest/debug.log

# Verify wallet state
./build/bin/bitcoin-cli -regtest listunspent
./build/bin/bitcoin-cli -regtest getwalletinfo
```

---
*Last Updated: June 28, 2025*
*Version: 1.2 - Removed obsolete Q-prefix system; quantum addresses now use standard bech32*