// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/quantum_witness.h>
#include <script/script.h>
#include <crypto/sha256.h>
#include <hash.h>
#include <logging.h>

namespace quantum {

CScript CreateQuantumWitnessScript(const CQuantumPubKey& pubkey)
{
    CScript script;
    
    // Witness script format for quantum with unified opcode:
    // <pubkey> OP_CHECKSIG_EX
    // 
    // The algorithm is determined from the signature data (first byte)
    // which contains the scheme ID:
    // 0x02 = ML-DSA-65
    // 0x03 = SLH-DSA-192f
    
    // Just add public key and unified opcode
    script << pubkey.GetKeyData();
    script << OP_CHECKSIG_EX;
    
    return script;
}

CScript CreateQuantumP2WSH(const CQuantumPubKey& pubkey)
{
    // Create the witness script
    CScript witnessScript = CreateQuantumWitnessScript(pubkey);
    
    // Hash the witness script with SHA256 (not HASH256)
    uint256 hash;
    CSHA256().Write(witnessScript.data(), witnessScript.size()).Finalize(hash.begin());
    
    // Create P2WSH scriptPubKey: OP_0 <32-byte-hash>
    CScript scriptPubKey;
    scriptPubKey << OP_0 << std::vector<unsigned char>(hash.begin(), hash.end());
    
    LogPrintf("Created quantum P2WSH: pubkey_size=%d, witness_script_size=%d, scripthash=%s\n",
              pubkey.GetKeyData().size(), witnessScript.size(), hash.ToString());
    
    return scriptPubKey;
}

bool ExtractQuantumPubKeyFromWitnessScript(const CScript& witnessScript, 
                                           CQuantumPubKey& pubkey)
{
    // Parse the witness script
    // Format: <pubkey> OP_CHECKSIG_EX
    CScript::const_iterator pc = witnessScript.begin();
    CScript::const_iterator end = witnessScript.end();
    
    opcodetype opcode;
    std::vector<unsigned char> vchPubKey;
    
    // First element should be the public key
    if (!witnessScript.GetOp(pc, opcode, vchPubKey)) {
        return false;
    }
    
    // Should be a push operation
    if (opcode > OP_PUSHDATA4) {
        return false;
    }
    
    // Next should be OP_CHECKSIG_EX
    if (pc == end) {
        return false;
    }
    
    opcode = static_cast<opcodetype>(*pc);
    if (opcode != OP_CHECKSIG_EX) {
        return false;
    }
    
    // Determine key type from pubkey size
    KeyType keyType;
    if (vchPubKey.size() == 1952) {
        keyType = KeyType::ML_DSA_65;
    } else if (vchPubKey.size() == 48) {
        keyType = KeyType::SLH_DSA_192F;
    } else {
        LogPrintf("Unknown quantum pubkey size: %d\n", vchPubKey.size());
        return false;
    }
    
    // Create the public key (constructor takes type first, then data)
    pubkey = CQuantumPubKey(keyType, vchPubKey);
    return pubkey.IsValid();
}

bool IsQuantumP2WSH(const CScript& script)
{
    // P2WSH scripts are: OP_0 <32-byte-hash>
    if (script.size() != 34) {
        return false;
    }
    
    CScript::const_iterator pc = script.begin();
    opcodetype opcode;
    std::vector<unsigned char> data;
    
    // First byte should be OP_0
    if (!script.GetOp(pc, opcode)) {
        return false;
    }
    if (opcode != OP_0) {
        return false;
    }
    
    // Next should be 32 bytes
    if (!script.GetOp(pc, opcode, data)) {
        return false;
    }
    if (data.size() != 32) {
        return false;
    }
    
    // Should be at end
    return pc == script.end();
}

std::vector<std::vector<unsigned char>> CreateQuantumWitnessStack(
    const QuantumSignature& qsig,
    const CScript& witnessScript)
{
    std::vector<std::vector<unsigned char>> stack;
    
    // For P2WSH, witness stack contains:
    // 1. Signature (serialized QuantumSignature)
    // 2. Witness script
    //
    // Note: Unlike P2WPKH, we don't need the pubkey separately since
    // it's already in the witness script
    
    std::vector<unsigned char> serializedSig;
    qsig.Serialize(serializedSig);
    
    stack.push_back(serializedSig);
    stack.push_back(std::vector<unsigned char>(witnessScript.begin(), witnessScript.end()));
    
    LogPrintf("Created quantum witness stack: sig_size=%d, witness_script_size=%d\n",
              serializedSig.size(), witnessScript.size());
    
    return stack;
}

bool ParseQuantumWitnessStack(const std::vector<std::vector<unsigned char>>& stack, 
                             QuantumSignature& qsig_out,
                             CScript& witnessScript_out)
{
    // P2WSH stack should have exactly 2 elements for simple quantum scripts
    if (stack.size() != 2) {
        LogPrintf("ParseQuantumWitnessStack: Invalid stack size %d\n", stack.size());
        return false;
    }
    
    // First element is the signature
    if (!qsig_out.Deserialize(stack[0])) {
        LogPrintf("ParseQuantumWitnessStack: Failed to deserialize quantum signature\n");
        return false;
    }
    
    // Second element is the witness script
    witnessScript_out = CScript(stack[1].begin(), stack[1].end());
    
    return true;
}

SignatureSchemeID GetQuantumSchemeFromWitnessScript(const CScript& witnessScript)
{
    // Parse to find the pubkey size which determines the algorithm
    // Format: <pubkey> OP_CHECKSIG_EX
    CScript::const_iterator pc = witnessScript.begin();
    CScript::const_iterator end = witnessScript.end();
    
    opcodetype opcode;
    std::vector<unsigned char> vchPubKey;
    
    // First element should be the pubkey
    if (!witnessScript.GetOp(pc, opcode, vchPubKey)) {
        return SCHEME_ECDSA;
    }
    
    // Determine scheme from pubkey size
    if (vchPubKey.size() == 1952) {
        return SCHEME_ML_DSA_65;
    } else if (vchPubKey.size() == 48) {
        return SCHEME_SLH_DSA_192F;
    } else {
        return SCHEME_ECDSA;
    }
}

} // namespace quantum