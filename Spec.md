# QSBitcoin Technical Specification

## Overview

QSBitcoin is a quantum-safe implementation of Bitcoin Core that adds support for post-quantum cryptographic signatures while maintaining full backward compatibility with the existing Bitcoin network. This specification details all technical changes required to implement a compatible quantum-safe Bitcoin client.

## 1. Cryptographic Primitives

### 1.1 Signature Algorithms

QSBitcoin implements two NIST-standardized post-quantum signature schemes via liboqs v0.12.0+:

#### ML-DSA-65 (Module-Lattice Digital Signature Algorithm)
- **Public Key Size**: 1952 bytes
- **Private Key Size**: 4032 bytes  
- **Signature Size**: ~3309 bytes
- **Security Level**: NIST Level 3
- **Use Case**: Standard transactions (99% of cases)
- **Algorithm ID**: 0x01

#### SLH-DSA-192f (Stateless Hash-Based Digital Signature Algorithm)
- **Public Key Size**: 64 bytes
- **Private Key Size**: 128 bytes
- **Signature Size**: ~35,664 bytes
- **Security Level**: NIST Level 3
- **Use Case**: High-value cold storage
- **Algorithm ID**: 0x02

### 1.2 Key Structure

```cpp
class CQuantumKey {
    uint8_t algorithm_id;     // 0x01 (ML-DSA) or 0x02 (SLH-DSA)
    vector<uint8_t> key_data; // Private key material
    // Non-copyable, move-only semantics
};

class CQuantumPubKey {
    uint8_t algorithm_id;
    vector<uint8_t> key_data; // Public key material
};
```

### 1.3 Signature Scheme Interface

```cpp
class ISignatureScheme {
    virtual bool Sign(const uint256& hash, vector<uint8_t>& sig) = 0;
    virtual bool Verify(const uint256& hash, const vector<uint8_t>& sig) = 0;
    virtual size_t GetPublicKeySize() = 0;
    virtual size_t GetPrivateKeySize() = 0;
    virtual size_t GetSignatureSize() = 0;
};
```

## 2. Script System

### 2.1 New Opcodes

The following opcodes are introduced via soft fork by repurposing existing NOP opcodes:

- `OP_CHECKSIG_ML_DSA` = 0xb3 (previously OP_NOP4)
- `OP_CHECKSIG_SLH_DSA` = 0xb4 (previously OP_NOP5)
- `OP_CHECKSIGVERIFY_ML_DSA` = 0xb5 (previously OP_NOP6)
- `OP_CHECKSIGVERIFY_SLH_DSA` = 0xb6 (previously OP_NOP7)

### 2.2 Script Verification Flag

New script verification flag:
- `SCRIPT_VERIFY_QUANTUM_SIGS` = (1U << 21)

This flag enables quantum signature verification when the soft fork is active.

### 2.3 Witness Script Format

All quantum addresses use P2WSH (Pay-to-Witness-Script-Hash) format:

```
Witness Script: <pubkey> OP_CHECKSIG_[ML_DSA|SLH_DSA]
ScriptPubKey: OP_0 <32-byte-hash>
```

### 2.4 Transaction Input Format

Quantum signatures in witness data follow this format:
```
[scheme_id:1 byte][sig_len:varint][signature][pubkey_len:varint][pubkey]
```

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
2. Create witness script: <pubkey> OP_CHECKSIG_[ML_DSA|SLH_DSA]
3. Hash witness script with SHA256
4. Encode as bech32 P2WSH address
```

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

Quantum signatures use special weight factors:
- ML-DSA signatures: 2x weight factor (vs 4x for ECDSA witness data)
- SLH-DSA signatures: 3x weight factor

## 5. Fee Policy

### 5.1 Fee Discounts

To incentivize quantum-safe adoption:
- ML-DSA signatures: 10% fee discount
- SLH-DSA signatures: 5% fee discount
- Base quantum fee multiplier: 1.5x

### 5.2 Fee Calculation

```
base_fee = calculate_standard_fee(tx_size)
if (has_quantum_signatures):
    fee = base_fee * 1.5 * (1 - discount_rate)
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
| Address Format | P2PKH/P2WPKH | P2WSH | P2WSH |
| Signature Size | 71 bytes | ~3.3KB | ~35KB |
| HD Derivation | Yes | No | No |
| Quantum Safe | No | Yes | Yes |
| Fee Discount | 0% | 10% | 5% |
| Use Case | Legacy | Standard | Cold Storage |

## 13. Reference Implementation

The reference implementation is available at:
- Repository: https://github.com/qsbitcoin/qsbitcoin
- Based on: Bitcoin Core v28.0
- License: MIT

## Appendix A: Test Vectors

[Test vectors for quantum signatures would be included here]

## Appendix B: Activation Parameters

[Specific block heights and deployment windows would be defined here for mainnet activation]