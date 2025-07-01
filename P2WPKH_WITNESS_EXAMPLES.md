# P2WPKH and Witness Script Examples

## Overview

Based on the Bitcoin Core codebase analysis, here's how witness scripts work, particularly for P2WPKH (Pay-to-Witness-PubKey-Hash).

## 1. How P2WPKH (witness v0 pubkey hash) works with OP_CHECKSIG

### P2WPKH Structure

When spending a P2WPKH output, the witness script execution differs from regular script execution:

1. **ScriptPubKey**: `OP_0 <20-byte-pubkey-hash>`
2. **ScriptSig**: Must be empty (for non-malleability)
3. **Witness Stack**: `[signature, pubkey]` (exactly 2 items)

### Execution Flow (from interpreter.cpp)

```cpp
// From VerifyWitnessProgram() in interpreter.cpp:
if (program.size() == WITNESS_V0_KEYHASH_SIZE) {
    // BIP141 P2WPKH: 20-byte witness v0 program (which encodes Hash160(pubkey))
    if (stack.size() != 2) {
        return set_error(serror, SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH); // 2 items in witness
    }
    // The witness script is implicitly:
    exec_script << OP_DUP << OP_HASH160 << program << OP_EQUALVERIFY << OP_CHECKSIG;
    return ExecuteWitnessScript(stack, exec_script, flags, SigVersion::WITNESS_V0, checker, execdata, serror);
}
```

## 2. Typical Witness Stack for P2WPKH

The witness stack for P2WPKH contains exactly 2 elements:
1. **Signature** (71-73 bytes typically for ECDSA)
2. **Public Key** (33 bytes for compressed pubkey)

Example from sign.cpp:
```cpp
// From ProduceSignature() when handling P2WPKH:
if (solved && whichType == TxoutType::WITNESS_V0_KEYHASH)
{
    CScript witnessscript;
    // Create the implicit witness script
    witnessscript << OP_DUP << OP_HASH160 << ToByteVector(result[0]) << OP_EQUALVERIFY << OP_CHECKSIG;
    TxoutType subType;
    // Sign with witness version
    solved = solved && SignStep(provider, creator, witnessscript, result, subType, SigVersion::WITNESS_V0, sigdata);
    // The result vector now contains [signature, pubkey]
    sigdata.scriptWitness.stack = result;
    sigdata.witness = true;
    result.clear();
}
```

## 3. How Witness Script Execution Differs from Regular Script Execution

### Key Differences:

1. **Segregated Data**: Witness data is stored separately from the transaction, not in scriptSig
2. **Implicit Script**: For P2WPKH, the actual script executed is implicit (not stored in the transaction)
3. **Version-Specific Sighash**: Uses BIP143 sighash algorithm (SigVersion::WITNESS_V0)
4. **Strict Rules**: ScriptSig must be empty for native witness programs

### Witness Script Execution Process:

```cpp
// From ExecuteWitnessScript() - executes with witness-specific rules:
// 1. Uses witness stack instead of scriptSig stack
// 2. Different sighash calculation (BIP143)
// 3. Witness-specific validation rules
```

## 4. Examples of Witness Scripts with OP_CHECKSIG

### P2WPKH Example

**Output Being Spent:**
- ScriptPubKey: `OP_0 <20-byte-hash>`
- Example: `0014` + `89abcdefabbaabbaabbaabbaabbaabbaabbaabba`

**Spending Transaction:**
- ScriptSig: (empty)
- Witness: 
  ```
  [
    <signature>,  // ~71 bytes
    <pubkey>      // 33 bytes
  ]
  ```

**Implicit Script Executed:**
```
OP_DUP OP_HASH160 <20-byte-hash> OP_EQUALVERIFY OP_CHECKSIG
```

### P2WSH Example (Pay-to-Witness-Script-Hash)

For P2WSH, the witness stack is different:

```cpp
// From VerifyWitnessProgram():
if (program.size() == WITNESS_V0_SCRIPTHASH_SIZE) {
    // BIP141 P2WSH: 32-byte witness v0 program (which encodes SHA256(script))
    if (stack.size() == 0) {
        return set_error(serror, SCRIPT_ERR_WITNESS_PROGRAM_WITNESS_EMPTY);
    }
    // Last stack item is the witness script
    const valtype& script_bytes = SpanPopBack(stack);
    exec_script = CScript(script_bytes.begin(), script_bytes.end());
    // Verify script hash matches
    uint256 hash_exec_script;
    CSHA256().Write(exec_script.data(), exec_script.size()).Finalize(hash_exec_script.begin());
    if (memcmp(hash_exec_script.begin(), program.data(), 32)) {
        return set_error(serror, SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH);
    }
    // Execute the witness script with remaining stack items
    return ExecuteWitnessScript(stack, exec_script, flags, SigVersion::WITNESS_V0, checker, execdata, serror);
}
```

### P2WSH Witness Stack Pattern:
```
[
  <arg1>,        // Arguments for the script
  <arg2>,
  ...
  <witness_script>  // The actual script (last item)
]
```

## Quantum Signature Integration

The codebase shows quantum signature support in witness scripts:

```cpp
// From SignStep() in sign.cpp:
// Check if this is a quantum witness script
if (sigversion == SigVersion::WITNESS_V0) {
    // Try to match: <pubkey> OP_CHECKSIG_ML_DSA or <pubkey> OP_CHECKSIG_SLH_DSA
    if (scriptPubKey.GetOp(pc, opcode, vchPubKey) && !vchPubKey.empty() &&
        scriptPubKey.GetOp(pc, opcode) && 
        (opcode == OP_CHECKSIG_ML_DSA || opcode == OP_CHECKSIG_SLH_DSA) &&
        pc == scriptPubKey.end()) {
        
        // For P2WSH witness scripts, only push the signature
        // The pubkey is already embedded in the witness script itself
        ret.push_back(std::move(sig));
        return true;
    }
}
```

This shows that quantum signatures are integrated into the witness script system, with special handling for the larger signature sizes.

## Summary

1. **P2WPKH** uses a witness stack with exactly 2 items: [signature, pubkey]
2. The actual script executed is implicit: `OP_DUP OP_HASH160 <hash> OP_EQUALVERIFY OP_CHECKSIG`
3. **P2WSH** uses a witness stack with script arguments plus the script itself as the last item
4. Witness execution uses different sighash calculation (BIP143) and stricter validation rules
5. Quantum signatures are supported in witness scripts with special opcodes (OP_CHECKSIG_ML_DSA, OP_CHECKSIG_SLH_DSA)