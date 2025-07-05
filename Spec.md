# QSBitcoin Technical Specification

## Overview

QSBitcoin is a quantum-safe implementation of Bitcoin Core that adds support for post-quantum cryptographic signatures while maintaining full backward compatibility with the existing Bitcoin network. This specification details all technical changes required to implement a compatible quantum-safe Bitcoin client.

**Key Design Decision (July 2, 2025)**: QSBitcoin uses unified opcodes (OP_CHECKSIG_EX and OP_CHECKSIGVERIFY_EX) for all quantum signature algorithms, with the algorithm identified by the first byte of the signature data. This reduces the number of new opcodes from 4 to 2 and provides better extensibility for future quantum algorithms.

## 1. Cryptographic Primitives

### 1.1 Signature Algorithms

QSBitcoin implements three signature schemes, including legacy ECDSA for compatibility and two NIST-standardized post-quantum signature schemes via liboqs v0.12.0+:

#### ECDSA (Elliptic Curve Digital Signature Algorithm)
- **Public Key Size**: 33 bytes (compressed) or 65 bytes (uncompressed)
- **Private Key Size**: 32 bytes
- **Signature Size**: ~71 bytes (DER-encoded)
- **Security Level**: 128-bit classical, vulnerable to quantum
- **Use Case**: Legacy compatibility, migration period
- **Algorithm ID**: 0x01

#### ML-DSA-65 (Module-Lattice Digital Signature Algorithm)
- **Public Key Size**: 1952 bytes
- **Private Key Size**: 4032 bytes  
- **Signature Size**: ~3309 bytes
- **Security Level**: NIST Level 3
- **Use Case**: Standard transactions (99% of cases)
- **Algorithm ID**: 0x02

#### SLH-DSA-192f (Stateless Hash-Based Digital Signature Algorithm)
- **Public Key Size**: 48 bytes
- **Private Key Size**: 96 bytes
- **Signature Size**: ~35,664 bytes
- **Security Level**: NIST Level 3
- **Use Case**: High-value cold storage
- **Algorithm ID**: 0x03

### 1.2 Key Structure

**CRITICAL DIFFERENCE FROM BITCOIN**: Bitcoin keys (CKey, CPubKey) are copyable and support BIP32 derivation. Quantum keys have fundamentally different properties.

```cpp
class CQuantumKey {
    uint8_t algorithm_id;     // 0x01 (ECDSA), 0x02 (ML-DSA) or 0x03 (SLH-DSA)
    vector<uint8_t> key_data; // Private key material
    // Non-copyable, move-only semantics
};

class CQuantumPubKey {
    uint8_t algorithm_id;
    vector<uint8_t> key_data; // Public key material
};
```

#### Key Implementation Requirements:
1. **Non-Copyable**: Quantum private keys must use move-only semantics (no copy constructor)
2. **No Derivation**: Cannot derive child keys - each address needs fresh randomness
3. **Memory Safety**: Must securely erase private key data on destruction
4. **Size Handling**: Keys are much larger than ECDSA (4KB private, 2KB public for ML-DSA)
5. **RNG Integration**: Quantum keys use Bitcoin Core's cryptographically secure RNG (GetStrongRandBytes) via custom liboqs callback

### 1.3 Signature Scheme Interface

**IMPORTANT DIFFERENCE FROM BITCOIN**: Standard Bitcoin Core hardcodes ECDSA and Schnorr signature validation directly in the script interpreter. QSBitcoin introduces an abstraction layer to support multiple signature algorithms.

```cpp
class ISignatureScheme {
    virtual bool Sign(const uint256& hash, vector<uint8_t>& sig) = 0;
    virtual bool Verify(const uint256& hash, const vector<uint8_t>& sig) = 0;
    virtual size_t GetPublicKeySize() = 0;
    virtual size_t GetPrivateKeySize() = 0;
    virtual size_t GetSignatureSize() = 0;
};
```

#### Implementation Notes for Other Quantum Libraries:
1. **Library Independence**: The ISignatureScheme interface allows you to use any quantum crypto library (not just liboqs). You only need to implement this interface for your chosen algorithms.
2. **Algorithm IDs**: Must use 0x01 for ECDSA, 0x02 for ML-DSA, and 0x03 for SLH-DSA to maintain compatibility
3. **Hash Input**: The `hash` parameter is always a 256-bit message digest (Bitcoin transaction hash)
4. **Signature Format**: Your implementation must produce signatures in the exact binary format expected by the algorithms
5. **RNG Integration**: Configure your quantum library to use Bitcoin Core's GetStrongRandBytes() for key generation to ensure consistent entropy quality across all signature types

## 2. Script System

### 2.1 New Opcodes

The following opcodes are introduced via soft fork by repurposing existing NOP opcodes:

- `OP_CHECKSIG_EX` = 0xb3 (previously OP_NOP4) - Extended checksig for quantum signatures
- `OP_CHECKSIGVERIFY_EX` = 0xb4 (previously OP_NOP5) - Extended checksig and verify for quantum signatures

These unified opcodes support multiple quantum signature algorithms, with the algorithm determined by the first byte of the signature data. This design allows for future extensibility without requiring additional opcodes.

### 2.2 Script Verification Flag

New script verification flag:
- `SCRIPT_VERIFY_QUANTUM_SIGS` = (1U << 21)

This flag enables quantum signature verification when the soft fork is active.

### 2.3 Witness Script Format

All quantum addresses use P2WSH (Pay-to-Witness-Script-Hash) format:

```
Witness Script: <pubkey> OP_CHECKSIG_EX
ScriptPubKey: OP_0 <32-byte-hash>
```

The algorithm is determined from the signature data, not the witness script. This keeps the witness script simple and allows the same script format for all quantum algorithms.

### 2.4 Transaction Input Format

**CRITICAL DIFFERENCE FROM BITCOIN**: Standard Bitcoin uses DER-encoded ECDSA signatures or fixed-size Schnorr signatures. QSBitcoin introduces a new variable-length format.

Quantum signatures in witness data follow this format:
```
[algorithm_id:1 byte][signature_data][sighash_type:1 byte]
```

#### Detailed Format Specification:
- **algorithm_id**: Single byte identifying the signature algorithm
  - 0x02: ML-DSA-65
  - 0x03: SLH-DSA-192f
- **signature_data**: Raw signature bytes from quantum algorithm (size varies by algorithm)
  - ML-DSA-65: ~3309 bytes
  - SLH-DSA-192f: ~35664 bytes
- **sighash_type**: Standard Bitcoin sighash type byte

**Implementation Note**: The algorithm ID in the signature allows the interpreter to determine how to verify the signature without needing different opcodes for each algorithm.

## 3. Address Format

### 3.1 Address Encoding

All quantum addresses use standard bech32 P2WSH encoding:
- **Mainnet**: `bc1q...` (62 characters)
- **Testnet**: `tb1q...` (62 characters)
- **Regtest**: `bcrt1q...` (64 characters)

No special prefixes distinguish quantum addresses from regular P2WSH addresses.

### 3.2 Address Generation

```
1. Generate quantum key pair (ML-DSA or SLH-DSA)
2. Create witness script: <pubkey> OP_CHECKSIG_EX
3. Hash witness script with SHA256
4. Encode as bech32 P2WSH address
```

Note: The algorithm type is not stored in the witness script. It is determined from the signature during verification.

## 4. Consensus Rules

### 4.1 Soft Fork Activation

- **Deployment Name**: `DEPLOYMENT_QUANTUM_SIGS`
- **BIP9 Bit**: 3
- **Activation**:
  - Mainnet: NEVER_ACTIVE (pending deployment parameters)
  - Testnet: ALWAYS_ACTIVE
  - Regtest: ALWAYS_ACTIVE

### 4.2 Transaction Limits

- `MAX_STANDARD_TX_WEIGHT_QUANTUM` = 1,000,000 weight units (1MB)
- `MAX_STANDARD_QUANTUM_SIGS` = 10 signatures per transaction
- `MAX_STANDARD_QUANTUM_SIG_SIZE` = 50,000 bytes per signature

### 4.3 Weight Calculation

**MAJOR DIFFERENCE FROM BITCOIN**: Bitcoin uses a 4x weight factor for all witness data. QSBitcoin introduces variable weight factors to better reflect actual validation costs.

Quantum signatures use special weight factors:
- ML-DSA signatures: 3x weight factor (vs 4x for ECDSA witness data)
- SLH-DSA signatures: 2x weight factor

#### Implementation Formula:
```
if (is_quantum_signature):
    if (algorithm == ML_DSA):
        weight = base_size + (witness_size * 3)
    elif (algorithm == SLH_DSA):
        weight = base_size + (witness_size * 2)
else:
    weight = base_size + (witness_size * 4)  # Standard Bitcoin
```

## 5. Fee Policy

### 5.1 Standard Fee Structure

Quantum transactions pay standard fees based on transaction size:
- No special discounts or multipliers
- Larger quantum signatures result in proportionally higher fees
- Fee calculation follows standard Bitcoin Core logic

### 5.2 Fee Calculation

```
fee = calculate_standard_fee(tx_size)
// No special handling for quantum signatures
// Fees based purely on transaction weight/size
```

## 6. Wallet Integration

### 6.1 Descriptor Format

New descriptor type for quantum addresses:
```
qpkh(<quantum_key>, <algorithm>)
```

Where:
- `quantum_key`: Hex-encoded quantum public key
- `algorithm`: "ecdsa", "ml-dsa" or "slh-dsa"

### 6.2 Key Storage

Quantum keys are stored in the wallet database with:
- Algorithm identifier
- Public key data
- Encrypted private key data (using wallet passphrase)
- Creation timestamp
- Key metadata

### 6.3 No HD Derivation

Quantum keys do not support BIP32 hierarchical deterministic derivation. Each quantum address requires a unique key pair.

## 7. RPC Interface

### 7.1 Modified Commands

#### getnewaddress
```json
{
  "method": "getnewaddress",
  "params": {
    "label": "string",
    "address_type": "string",
    "algorithm": "ecdsa|ml-dsa|slh-dsa"  // Optional parameter
  }
}
```

#### estimatesmartfee
```json
{
  "method": "estimatesmartfee",
  "params": {
    "conf_target": number,
    "estimate_mode": "string",
    "signature_type": "ecdsa|ml-dsa|slh-dsa"  // New parameter
  }
}
```

### 7.2 New Commands

#### getquantuminfo
Returns quantum wallet capabilities and statistics:
```json
{
  "enabled": true,
  "algorithms": ["ml-dsa", "slh-dsa"],
  "address_count": {
    "ml-dsa": 5,
    "slh-dsa": 2
  },
  "activation_status": "active"
}
```

#### estimatetxfee
Estimates transaction fee for any signature type:
```json
{
  "method": "estimatetxfee",
  "params": {
    "conf_target": number,
    "signature_type": "ecdsa|ml-dsa|slh-dsa"
  }
}
```

## 8. Network Protocol

### 8.1 Service Flags

New service flag (future use):
- `NODE_QUANTUM` = (1 << 24)

Indicates node supports quantum transactions.

### 8.2 Message Size Limits

Due to larger quantum signatures, the following limits apply:
- Maximum transaction size: 1MB (vs 100KB standard)
- Maximum block size: Unchanged (1MB base + 3MB witness)

## 9. Migration Path

### 9.1 Transition Period

1. Soft fork activates quantum opcodes
2. Both ECDSA and quantum signatures accepted
3. Wallets can generate either address type
4. Users migrate funds at their discretion

### 9.2 Security Considerations

- Quantum addresses are immediately quantum-safe
- ECDSA addresses remain vulnerable to future quantum computers
- No forced migration - users control timing
- Mixed transactions (ECDSA inputs + quantum outputs) supported

## 10. Implementation Requirements

### 10.1 Dependencies

- liboqs v0.12.0+ (only ML-DSA and SLH-DSA algorithms required)
- No additional external dependencies

#### RNG Configuration

QSBitcoin configures liboqs to use Bitcoin Core's RNG:
```cpp
// Custom RNG callback for liboqs
static void BitcoinRandBytes(uint8_t* buffer, size_t size) {
    // GetStrongRandBytes has 32-byte limit, call multiple times for larger requests
    constexpr size_t MAX_BYTES_PER_CALL = 32;
    size_t offset = 0;
    while (offset < size) {
        size_t bytes_to_get = std::min(size - offset, MAX_BYTES_PER_CALL);
        GetStrongRandBytes(std::span<unsigned char>(buffer + offset, bytes_to_get));
        offset += bytes_to_get;
    }
}

// Set during initialization
OQS_randombytes_custom_algorithm(&BitcoinRandBytes);
```

### 10.2 Build Configuration

```cmake
option(ENABLE_QUANTUM "Enable quantum-safe signatures" ON)
find_package(liboqs REQUIRED)
```

### 10.3 Testing Requirements

Implement comprehensive tests for:
- Key generation and serialization
- Signature creation and verification
- Script evaluation with quantum opcodes
- Soft fork activation logic
- Fee calculation with discounts
- Wallet operations (address generation, signing)
- Transaction validation
- Cross-signing (ECDSA to quantum, quantum to ECDSA)

## 11. Security Considerations

### 11.1 Key Management

- Quantum private keys are non-copyable (move-only semantics)
- Keys must be securely erased from memory after use
- No key derivation - each address needs unique entropy
- All quantum keys use Bitcoin Core's unified RNG system (GetStrongRandBytes) ensuring the same entropy quality as ECDSA keys

### 11.2 Signature Malleability

- Quantum signatures are deterministic
- No malleability issues like ECDSA
- Witness commitment prevents tampering

### 11.3 Quantum Attack Timeline

- Current ECDSA signatures: Vulnerable to future quantum computers
- ML-DSA signatures: Secure against known quantum algorithms
- SLH-DSA signatures: Maximum security, hash-based construction

## 12. Compatibility Matrix

| Feature | ECDSA | ML-DSA | SLH-DSA |
|---------|-------|---------|----------|
| Algorithm ID | 0x01 | 0x02 | 0x03 |
| Address Format | P2PKH/P2WPKH | P2WSH | P2WSH |
| Signature Size | 71 bytes | ~3.3KB | ~35KB |
| HD Derivation | Yes | No | No |
| Quantum Safe | No | Yes | Yes |
| Fee Discount | 0% | 10% | 5% |
| Use Case | Legacy | Standard | Cold Storage |

## 13. Critical Implementation Notes

### 13.1 Push Size Limit Bypass

**CRITICAL**: The 520-byte push size limit (MAX_SCRIPT_ELEMENT_SIZE) must be bypassed for quantum signatures when SCRIPT_VERIFY_QUANTUM_SIGS is set. This is handled differently in witness vs non-witness contexts:

1. **In EvalScript**: Check for SCRIPT_VERIFY_QUANTUM_SIGS flag
2. **In ExecuteWitnessScript**: Size check happens at witness parsing
3. **Implementation**: Modified OP_PUSHDATA handling when quantum flag is active

### 13.2 Witness Script Generation

**CRITICAL**: When generating quantum addresses, ensure the witness script uses the exact same public key that will be used for signing. The witness script format must be:

```cpp
CScript witness_script;
witness_script << pubkey.GetKeyData() << OP_CHECKSIG_EX;
// Note: Algorithm ID is NOT included in witness script
// It is extracted from the signature during verification
```

### 13.3 Witness Corruption Prevention

**CRITICAL BUG TO AVOID**: Multiple ScriptPubKeyManagers attempting to sign the same transaction can corrupt witness data by reducing the witness stack size. Solution:

```cpp
// Store original witness before signing attempt
std::vector<CScriptWitness> original_witnesses = /* copy witnesses */;

// Attempt signing
bool result = SignTransaction(tx, ...);

// Check if we actually improved the witness
bool made_progress = false;
for (size_t i = 0; i < tx.vin.size(); ++i) {
    if (new_witness_size > original_witness_size) {
        made_progress = true;
        break;
    }
}

// Restore if no progress
if (!made_progress) {
    /* restore original witnesses */
}
```

### 13.4 Mandatory Script Flags

SCRIPT_VERIFY_QUANTUM_SIGS must be included in:
- STANDARD_SCRIPT_VERIFY_FLAGS (for mempool)
- MANDATORY_SCRIPT_VERIFY_FLAGS (for consensus after activation)

## 14. Implementation Guide for Compatible Forks

### 14.1 Key Differences from Standard Bitcoin

1. **Signature Abstraction Layer**
   - Bitcoin: Direct ECDSA/Schnorr validation in script interpreter
   - QSBitcoin: ISignatureScheme interface for algorithm independence

2. **Script Validation Changes**
   - New function: `EvalChecksigQuantum()` handles unified quantum opcode
   - Extracts algorithm ID from signature data to determine verification method
   - Requires SCRIPT_VERIFY_QUANTUM_SIGS flag when soft fork active
   - Quantum opcodes are NOPs when flag not set (backward compatible)

3. **Serialization Differences**
   - Bitcoin: Fixed-size signatures in witness
   - QSBitcoin: Variable-length with scheme_id prefix
   - Must handle signatures up to 50KB

4. **Wallet Architecture**
   - Bitcoin: BIP32 HD derivation for all keys
   - QSBitcoin: No derivation for quantum keys, each requires fresh entropy
   - Descriptor system extended with `qpkh()` descriptor type

### 14.2 Minimal Implementation Checklist

To create a compatible implementation with a different quantum library:

1. **Implement ISignatureScheme Interface**
   ```cpp
   class YourMLDSAImpl : public ISignatureScheme {
       // Your quantum library's ML-DSA implementation
   };
   ```

2. **Script Interpreter Modifications**
   - Add quantum opcode handlers in script/interpreter.cpp
   - Implement `EvalChecksigQuantum()` function
   - Extract algorithm_id from signature (first byte)
   - Use unified opcodes OP_CHECKSIG_EX and OP_CHECKSIGVERIFY_EX

3. **Consensus Rules**
   - Add SCRIPT_VERIFY_QUANTUM_SIGS flag
   - Implement soft fork activation logic
   - Enforce quantum transaction weight limits

4. **RPC Changes**
   - Extend `getnewaddress` with algorithm parameter
   - Add `getquantuminfo` command
   - Update fee estimation for quantum signatures

5. **Wallet Integration**
   - Add quantum key storage (non-HD)
   - Implement `qpkh()` descriptor parsing
   - Extend signing provider for quantum keys

### 14.3 Testing Your Implementation

Essential test cases:
1. Generate quantum addresses and verify bech32 encoding
2. Create and broadcast quantum transactions
3. Verify cross-compatibility with reference implementation
4. Test soft fork activation on regtest
5. Validate fee calculations with discounts

### 14.4 Reference Implementation

The reference implementation is available at:
- Repository: https://github.com/qsbitcoin/qsbitcoin
- Based on: Bitcoin Core v28.0
- License: MIT
- Status: 100% Complete with unified opcodes (as of July 2, 2025)
- Test Coverage: 88 test cases across 16 test files
- Opcode Consolidation: Reduced from 4 quantum opcodes to 2 unified opcodes

#### Implementation Highlights:
- Full quantum signature support (ML-DSA-65, SLH-DSA-192f)
- Complete wallet integration with descriptor support
- Soft fork activation via BIP9
- Push size limit bypass for large signatures
- Witness corruption prevention
- Fee discount mechanism
- Unified RNG system - quantum keys use Bitcoin Core's GetStrongRandBytes()
- Comprehensive test suite

## Appendix A: Core Script Validation Pseudocode

### EvalChecksigQuantum Implementation

```cpp
bool EvalChecksigQuantum(const valtype& sig, const valtype& pubkey, 
                        opcodetype opcode, ScriptError* serror) {
    // 1. Extract algorithm_id from signature (first byte)
    if (sig.size() < 1) return false;
    uint8_t algo_id = sig[0];
    
    // 2. Determine expected algorithm from algo_id
    KeyType expectedKeyType;
    switch (algo_id) {
        case 0x02: // ML-DSA
            expectedKeyType = KeyType::ML_DSA_65;
            break;
        case 0x03: // SLH-DSA
            expectedKeyType = KeyType::SLH_DSA_192F;
            break;
        default:
            return false; // Unknown algorithm
    }
    
    // 3. Extract signature data and hash type
    if (sig.size() < 2) return false;
    uint8_t nHashType = sig.back();
    
    // Remove algorithm ID and hash type to get pure signature
    vector<uint8_t> vchSigNoHashType(sig.begin() + 1, sig.end() - 1);
    
    // 4. Create quantum public key from witness script pubkey
    CQuantumPubKey quantumPubKey;
    if (!quantumPubKey.Set(pubkey, expectedKeyType)) {
        return false;
    }
    
    // 5. Verify signature
    uint256 sighash = SignatureHash(scriptCode, txTo, nIn, nHashType, amount);
    return CQuantumKey::Verify(sighash, vchSigNoHashType, quantumPubKey);
}
```

## Appendix B: Test Vectors

### ML-DSA-65 Test Vector
```
Algorithm: ML-DSA-65 (Dilithium3)
Private Key Size: 4032 bytes
Public Key Size: 1952 bytes  
Signature Size: 3309 bytes (not including sighash byte)
```

### SLH-DSA-192f Test Vector
```
Algorithm: SLH-DSA-192f (SPHINCS+-SHA2-192f-simple)
Private Key Size: 96 bytes
Public Key Size: 48 bytes
Signature Size: 35664 bytes (not including sighash byte)
```

### Example Quantum Address
```
Algorithm: ML-DSA-65
Address (Regtest): bcrt1q...  (64 characters)
Witness Script: <0x02> <1952-byte-pubkey> OP_CHECKSIG_EX
Script Hash: SHA256(witness_script)
```

Note: Full test vectors with hex values are available in the test suite at src/test/quantum_*.cpp

## Appendix C: Activation Parameters

### Mainnet (Future)
- Start Time: TBD
- Timeout: TBD  
- Threshold: 90% of 2016 blocks

### Testnet/Regtest
- Status: ALWAYS_ACTIVE
- No activation required