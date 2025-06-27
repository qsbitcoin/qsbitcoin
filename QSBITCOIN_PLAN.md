# QSBitcoin: Quantum-Safe Signature Migration Plan

## Executive Summary

Minimal implementation plan to add quantum-safe signatures to Bitcoin while maintaining full backward compatibility. Focus on signature scheme replacement only, using liboqs for proven NIST-standardized algorithms.

## Core Approach: Minimal Code Changes

1. **Reuse existing infrastructure** - Extend rather than replace
2. **Single abstraction layer** - Minimal changes to consensus code
3. **Soft fork activation** - No immediate breaking changes
4. **Two schemes only** - ML-DSA for general use, SLH-DSA for high security

## Technical Design

### Signature Schemes (via liboqs v0.12.0+)

| Scheme | Purpose | Signature Size | When to Use |
|--------|---------|----------------|-------------|
| **ML-DSA-65** | Standard transactions | 3.3 KB | 99% of cases |
| **SLH-DSA-192f** | High-value storage | 35 KB | Cold storage |
| **ECDSA** | Legacy support | 71 bytes | Migration period |

### Minimal Address Format
```
qsbtc1[version][scheme_flags][pubkey_hash][checksum]

scheme_flags (64 bits for future expansion):
- 0x0000000000000001: ECDSA only (legacy)
- 0x0000000000000002: ML-DSA only (standard)
- 0x0000000000000004: SLH-DSA only (high-security)
- 0x0000000000000003: ML-DSA + ECDSA (migration)
```

### Transaction Changes (Dynamic Size Support)
```cpp
// Flexible signature format for future extensibility
// Support dynamic signature sizes for different algorithms
script_sig: [scheme_id:1 byte][sig_len:varint][signature][pubkey_len:varint][pubkey]

// Signature scheme IDs (extensible)
enum SignatureSchemeID : uint8_t {
    SCHEME_ECDSA = 0x00,        // Legacy ECDSA
    SCHEME_ML_DSA_65 = 0x01,    // ML-DSA-65 (~3.3KB signatures)
    SCHEME_SLH_DSA_192F = 0x02, // SLH-DSA-192f (~49KB signatures)
    // Reserved for future quantum-safe algorithms
    SCHEME_ML_DSA_87 = 0x03,    // Future: ML-DSA-87 (~4.6KB)
    SCHEME_FALCON_512 = 0x04,   // Future: Falcon-512
    SCHEME_RESERVED_05 = 0x05,  // Reserved
    // ... up to 0xFF
};

// Dynamic size handling in CTxIn
// No changes to core transaction structure needed
// Signatures stored in scriptSig with variable length encoding
```

## Implementation Phases

### Phase 0: Repository Setup (Week 1)
**Git Configuration and Fork Management:**
```bash
# 1. Initialize QSBitcoin repository
git init
# IMPORTANT: Always use SSH URLs for git operations, never HTTPS
git remote add origin git@github.com:qsbitcoin/qsbitcoin.git
git remote add upstream https://github.com/bitcoin/bitcoin.git

# 2. Import Bitcoin Core source (choose one approach):
# Option A: Direct import (recommended for fork)
rm -rf bitcoin/.git
git add bitcoin/
git commit -m "Import Bitcoin Core source code"

# Option B: Keep as submodule for tracking
git submodule add https://github.com/bitcoin/bitcoin.git bitcoin

# 3. Push to origin (always use SSH)
git push -u origin master

# 4. Sync with upstream for updates
git fetch upstream
git merge upstream/master --allow-unrelated-histories
```

### Phase 1: Core Integration (Month 1-2)
**Minimal changes to Bitcoin Core:**
```cpp
// 1. Copy liboqs library into project
// liboqs source copied directly to liboqs/ directory

// 2. Create single abstraction class
class SignatureScheme {
    virtual bool Sign(hash, key, sig) = 0;
    virtual bool Verify(hash, pubkey, sig) = 0;
};

// 3. Extend script interpreter (minimal change)
// In script/interpreter.cpp - add two new opcodes:
case OP_CHECKSIG_ML_DSA:
    return VerifyMLDSA(sig, pubkey, hash);
case OP_CHECKSIG_SLH_DSA:
    return VerifySLHDSA(sig, pubkey, hash);
```

### Phase 2: Address & Wallet (Month 3-4)
**Extend existing systems:**
- Add scheme_flags to `CTxDestination`
- Extend `DecodeDestination()` for new format
- Update wallet to generate quantum addresses
- Reuse existing bech32 encoding

### Phase 3: Consensus Rules (Month 5-6)
**Minimal consensus changes:**
- Increase `MAX_STANDARD_TX_WEIGHT` for larger signatures
- Add activation height for soft fork
- Update fee calculations for quantum transactions

### Phase 4: Testing & Deployment (Month 7-9)
- Run on testnet with existing infrastructure
- No changes to mining or network protocol
- Gradual wallet adoption

## Migration Strategy

### Soft Fork Activation (Minimal Disruption)
1. **BIP9-style activation** - IMPLEMENTED ✅
   - Added DEPLOYMENT_QUANTUM_SIGS (bit 3) to consensus parameters
   - Mainnet: NEVER_ACTIVE (to be activated later)
   - Testnet/Regtest: ALWAYS_ACTIVE for testing
2. **Quantum addresses opt-in** - Users migrate at their pace
3. **Legacy addresses work forever** - No forced migration
4. **Fee incentives only** - 10% discount for quantum signatures (not yet implemented)

### Simple Migration Path
```bash
# Users migrate with single command:
bitcoin-cli migratewallet quantum
```

## Key Integration Points

### 1. Script System (Minimal Changes)
```cpp
// Add to script/script.h - IMPLEMENTED ✅
// Using repurposed NOP opcodes for soft fork compatibility
static const unsigned char OP_CHECKSIG_ML_DSA = OP_NOP4;      // 0xb3
static const unsigned char OP_CHECKSIG_SLH_DSA = OP_NOP5;     // 0xb4
static const unsigned char OP_CHECKSIGVERIFY_ML_DSA = OP_NOP6; // 0xb5
static const unsigned char OP_CHECKSIGVERIFY_SLH_DSA = OP_NOP7; // 0xb6

// Added to interpreter.h - IMPLEMENTED ✅
SCRIPT_VERIFY_QUANTUM_SIGS = (1U << 21),  // Enable quantum signature verification

// Modified EvalScript() - added EvalChecksigQuantum() function
```

### 2. Signature Verification
```cpp
// Signature abstraction implemented - IMPLEMENTED ✅
// See src/crypto/signature_scheme.h and quantum_key.h
class ISignatureScheme {
    virtual bool Sign(const uint256& hash, const CKey& key, std::vector<unsigned char>& sig) = 0;
    virtual bool Verify(const uint256& hash, const CPubKey& pubkey, const std::vector<unsigned char>& sig) = 0;
};

// Quantum key implementation - IMPLEMENTED ✅
class CQuantumKey {
    bool Sign(const uint256& hash, std::vector<unsigned char>& vchSig) const;
    static bool Verify(const uint256& hash, const std::vector<unsigned char>& vchSig, const CQuantumPubKey& pubkey);
};
```

### 3. Build System
```cmake
# Build system integration - IMPLEMENTED ✅
# liboqs copied directly to liboqs/ directory
# CMake configuration updated to build only required algorithms:
# - ML-DSA-65 (Dilithium)
# - SLH-DSA-192f (SPHINCS+)
add_subdirectory(liboqs)
target_link_libraries(bitcoind PRIVATE oqs)
```

## Implementation Status

### ✅ Completed Components
1. **liboqs Integration** - Successfully integrated v0.12.0
2. **Quantum Key Infrastructure** - CQuantumKey and CQuantumPubKey classes
3. **Address Format** - Base58Check quantum addresses (Q1/Q2/Q3 prefixes)
4. **Script System Extensions** - Quantum opcodes and soft fork activation
5. **Signature Abstraction Layer** - ISignatureScheme interface
6. **Dynamic Signature Format** - Extensible format with varint encoding
7. **Transaction Weight Calculations** - Special weight factors for quantum signatures
8. **Signature Verification Engine** - QuantumSignatureChecker with proper hash computation
9. **Consensus Rules** - Quantum-specific transaction limits and fee structure
10. **Basic Tests** - Unit tests for core functionality
11. **Fee Integration** - Quantum fee adjustments integrated with mempool and validation

### 🟡 In Progress
1. **Key Management** - Wallet integration pending
2. **Activation Parameters** - Need to set testnet/mainnet activation heights
3. **Fee Estimation** - RPC commands for quantum transaction fee estimation

### 🔴 Not Started
1. **Wallet RPC Commands** - User-facing commands
2. **Network Protocol** - May not need changes
3. **Migration Tools** - Wallet migration utilities
4. **Full Integration Testing** - End-to-end testing on testnet

## Security Model

- **No changes to Bitcoin's security assumptions**
- **NIST-standardized algorithms only**
- **Gradual transition preserves network stability**
- **Existing P2P and consensus mechanisms unchanged**

## Timeline

| Phase | Duration | Deliverable |
|-------|----------|-------------|
| Repository Setup | 1 week | Git configured, fork established |
| Integration | 2 months | liboqs integrated, basic signing works |
| Wallet/Address | 2 months | Can create/spend quantum addresses |
| Consensus | 2 months | Soft fork ready for testnet |
| Testing | 3 months | Mainnet ready |

**Total: 9 months to mainnet-ready code**

## Dependencies

- liboqs v0.12.0+ (MIT license)
- Bitcoin Core v25.0+ (existing codebase)
- No other external dependencies

## Risk Mitigation

1. **Minimal code surface** - Fewer changes = fewer bugs
2. **Reuse Bitcoin's testing** - Existing test framework
3. **Soft fork safety** - Can disable if issues found
4. **No protocol changes** - Works with existing network

---

*Focus: Signature migration only. All other quantum-safe improvements deferred to future phases.*

## Implementation Learnings

### Key Decisions Made
1. **Repurposed NOP Opcodes** - Using OP_NOP4-7 for quantum opcodes ensures clean soft fork compatibility
2. **Base58Check over Bech32m** - Simpler implementation, sufficient for MVP
3. **No HD Derivation** - Quantum keys don't support BIP32, simplifying wallet integration
4. **Signature Abstraction** - ISignatureScheme interface allows clean integration of multiple algorithms

### Technical Discoveries
1. **ML-DSA Key Format** - Public key is not simply first bytes of private key (requires OQS_SIG_ml_dsa_65_keypair)
2. **Script Validation** - Quantum opcodes must check SCRIPT_VERIFY_QUANTUM_SIGS flag before execution
3. **Transaction Size** - ML-DSA signatures are ~3.3KB, SLH-DSA are ~49KB - significant impact on block space
4. **Dynamic Signature Support** - Using varint encoding for signature/pubkey lengths enables future algorithm additions without consensus changes
5. **Weight Calculation** - Transaction weight formula needs adjustment for large signatures to maintain fee proportionality

### Next Critical Steps
1. **Activation Heights** - Set testnet/mainnet activation parameters for soft fork
2. **Wallet Integration** - Add quantum key management to wallet
3. **RPC Commands** - Implement user-facing commands for quantum operations
4. **Network Testing** - Deploy to testnet for real-world validation
5. **Fee Estimation RPC** - Add commands to estimate fees for quantum transactions

## Living Document Notice

This plan is a **living document** that will be updated throughout the development process:

- **Update Triggers**: New technical discoveries, security findings
- **Synchronization**: Keep aligned with QSBITCOIN_TASKS.md progress
- **Version Control**: Document significant changes in commit messages
- **Continuous Improvement**: Incorporate lessons learned at each phase completion

*Last Updated: June 27, 2025*  
*Version: 1.3*