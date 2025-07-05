// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <policy/quantum_policy.h>
#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <script/quantum_signature.h>
#include <script/script.h>

namespace quantum {

template<typename T>
unsigned int CountQuantumSignatures(const T& tx)
{
    unsigned int count = 0;
    
    for (size_t i = 0; i < tx.vin.size(); i++) {
        const auto& input = tx.vin[i];
        
        // Check scriptSig for quantum signatures (legacy path, rarely used)
        CScript::const_iterator pc = input.scriptSig.begin();
        std::vector<unsigned char> data;
        opcodetype opcode;
        
        while (pc < input.scriptSig.end()) {
            if (!input.scriptSig.GetOp(pc, opcode, data)) {
                break;
            }
            
            // Check if this looks like a quantum signature
            if (data.size() > MIN_QUANTUM_SIG_SIZE_THRESHOLD) { // Quantum signatures are much larger than ECDSA
                QuantumSignature qsig;
                if (ParseQuantumSignature(data, qsig)) {
                    if (qsig.scheme_id == SCHEME_ML_DSA_65 || 
                        qsig.scheme_id == SCHEME_SLH_DSA_192F) {
                        count++;
                    }
                }
            }
        }
        
        // Check witness data for quantum signatures (P2WSH path - primary for quantum)
        if (!input.scriptWitness.IsNull() && input.scriptWitness.stack.size() >= 2) {
            // For P2WSH, the witness stack typically contains:
            // [0]: signature(s)
            // [last]: witness script
            
            // Check if any stack element (except the last which is the witness script) 
            // contains a quantum signature
            for (size_t j = 0; j < input.scriptWitness.stack.size() - 1; j++) {
                const auto& stackElement = input.scriptWitness.stack[j];
                if (stackElement.size() > MIN_QUANTUM_SIG_SIZE_THRESHOLD) {
                    QuantumSignature qsig;
                    if (ParseQuantumSignature(stackElement, qsig)) {
                        if (qsig.scheme_id == SCHEME_ML_DSA_65 || 
                            qsig.scheme_id == SCHEME_SLH_DSA_192F) {
                            count++;
                        }
                    }
                }
            }
            
            // Also check the witness script itself to identify quantum scripts
            // The witness script is the last element in the stack
            const auto& witnessScript = input.scriptWitness.stack.back();
            if (!witnessScript.empty()) {
                CScript script(witnessScript.begin(), witnessScript.end());
                CScript::const_iterator script_pc = script.begin();
                
                // Check for quantum opcodes in the witness script
                // Two possible formats:
                // 1. <algorithm_id:1 byte> <pubkey> OP_CHECKSIG_EX (descriptor format)
                // 2. <pubkey> OP_CHECKSIG_EX (simple format)
                
                // Try to parse first element
                if (script.GetOp(script_pc, opcode, data)) {
                    if (data.size() == 1) {
                        // Format 1: algorithm ID prefix
                        uint8_t algo_id = data[0];
                        if ((algo_id == SCHEME_ML_DSA_65 || algo_id == SCHEME_SLH_DSA_192F) &&
                            script.GetOp(script_pc, opcode, data) && // Get pubkey
                            script.GetOp(script_pc, opcode) && // Get final opcode
                            opcode == OP_CHECKSIG_EX) {
                            // This is a quantum witness script, but signature already counted above
                        }
                    } else if ((data.size() == ML_DSA_65_PUBKEY_SIZE || 
                               data.size() == SLH_DSA_192F_PUBKEY_SIZE) &&
                               script.GetOp(script_pc, opcode) &&
                               opcode == OP_CHECKSIG_EX) {
                        // Format 2: direct pubkey (no algorithm prefix)
                        // This is a quantum witness script, but signature already counted above
                    }
                }
            }
        }
    }
    
    return count;
}

template<typename T>
bool HasQuantumSignatures(const T& tx)
{
    return CountQuantumSignatures(tx) > 0;
}


template<typename T>
bool CheckQuantumSignaturePolicy(const T& tx, std::string& reason)
{
    unsigned int quantum_sig_count = 0;
    size_t largest_sig_size = 0;
    
    for (size_t i = 0; i < tx.vin.size(); i++) {
        const auto& input = tx.vin[i];
        
        // Check scriptSig for quantum signatures (legacy path, rarely used)
        CScript::const_iterator pc = input.scriptSig.begin();
        std::vector<unsigned char> data;
        opcodetype opcode;
        
        while (pc < input.scriptSig.end()) {
            if (!input.scriptSig.GetOp(pc, opcode, data)) {
                break;
            }
            
            if (data.size() > MIN_QUANTUM_SIG_SIZE_THRESHOLD) {
                QuantumSignature qsig;
                if (ParseQuantumSignature(data, qsig)) {
                    if (qsig.scheme_id == SCHEME_ML_DSA_65 || 
                        qsig.scheme_id == SCHEME_SLH_DSA_192F) {
                        quantum_sig_count++;
                        size_t sig_size = qsig.GetSerializedSize();
                        if (sig_size > largest_sig_size) {
                            largest_sig_size = sig_size;
                        }
                        
                        // Check individual signature size
                        if (sig_size > MAX_STANDARD_QUANTUM_SIG_SIZE) {
                            reason = "quantum signature too large";
                            return false;
                        }
                    }
                }
            }
        }
        
        // Check witness data for quantum signatures (P2WSH path - primary for quantum)
        if (!input.scriptWitness.IsNull() && input.scriptWitness.stack.size() >= 2) {
            // Check all stack elements except the last (which is the witness script)
            for (size_t j = 0; j < input.scriptWitness.stack.size() - 1; j++) {
                const auto& stackElement = input.scriptWitness.stack[j];
                if (stackElement.size() > MIN_QUANTUM_SIG_SIZE_THRESHOLD) {
                    QuantumSignature qsig;
                    if (ParseQuantumSignature(stackElement, qsig)) {
                        if (qsig.scheme_id == SCHEME_ML_DSA_65 || 
                            qsig.scheme_id == SCHEME_SLH_DSA_192F) {
                            quantum_sig_count++;
                            size_t sig_size = qsig.GetSerializedSize();
                            if (sig_size > largest_sig_size) {
                                largest_sig_size = sig_size;
                            }
                            
                            // Check individual signature size
                            if (sig_size > MAX_STANDARD_QUANTUM_SIG_SIZE) {
                                reason = "quantum signature too large";
                                return false;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Check total quantum signature count
    if (quantum_sig_count > MAX_STANDARD_QUANTUM_SIGS) {
        reason = "too many quantum signatures";
        return false;
    }
    
    return true;
}

// Explicit template instantiations
template unsigned int CountQuantumSignatures<CTransaction>(const CTransaction& tx);
template unsigned int CountQuantumSignatures<CMutableTransaction>(const CMutableTransaction& tx);
template bool HasQuantumSignatures<CTransaction>(const CTransaction& tx);
template bool HasQuantumSignatures<CMutableTransaction>(const CMutableTransaction& tx);
template bool CheckQuantumSignaturePolicy<CTransaction>(const CTransaction& tx, std::string& reason);
template bool CheckQuantumSignaturePolicy<CMutableTransaction>(const CMutableTransaction& tx, std::string& reason);

} // namespace quantum