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
    
    for (const auto& input : tx.vin) {
        // Check scriptSig for quantum signatures
        CScript::const_iterator pc = input.scriptSig.begin();
        std::vector<unsigned char> data;
        opcodetype opcode;
        
        while (pc < input.scriptSig.end()) {
            if (!input.scriptSig.GetOp(pc, opcode, data)) {
                break;
            }
            
            // Check if this looks like a quantum signature
            if (data.size() > 100) { // Quantum signatures are much larger than ECDSA
                QuantumSignature qsig;
                if (ParseQuantumSignature(data, qsig)) {
                    if (qsig.scheme_id == SCHEME_ML_DSA_65 || 
                        qsig.scheme_id == SCHEME_SLH_DSA_192F) {
                        count++;
                    }
                }
            }
        }
        
        // TODO: Also check witness data when segwit quantum addresses are implemented
    }
    
    return count;
}

template<typename T>
bool HasQuantumSignatures(const T& tx)
{
    return CountQuantumSignatures(tx) > 0;
}

template<typename T>
CAmount GetQuantumAdjustedFee(CAmount base_fee, const T& tx)
{
    // Count different types of quantum signatures
    unsigned int ml_dsa_count = 0;
    unsigned int slh_dsa_count = 0;
    
    for (const auto& input : tx.vin) {
        CScript::const_iterator pc = input.scriptSig.begin();
        std::vector<unsigned char> data;
        opcodetype opcode;
        
        while (pc < input.scriptSig.end()) {
            if (!input.scriptSig.GetOp(pc, opcode, data)) {
                break;
            }
            
            if (data.size() > 100) {
                QuantumSignature qsig;
                if (ParseQuantumSignature(data, qsig)) {
                    if (qsig.scheme_id == SCHEME_ML_DSA_65) {
                        ml_dsa_count++;
                    } else if (qsig.scheme_id == SCHEME_SLH_DSA_192F) {
                        slh_dsa_count++;
                    }
                }
            }
        }
    }
    
    // No quantum signatures, return base fee
    if (ml_dsa_count == 0 && slh_dsa_count == 0) {
        return base_fee;
    }
    
    // Apply quantum fee multiplier first
    CAmount adjusted_fee = static_cast<CAmount>(base_fee * QUANTUM_FEE_MULTIPLIER);
    
    // Then apply discounts based on signature types
    // Use weighted average of discounts
    double total_sigs = ml_dsa_count + slh_dsa_count;
    double avg_discount = (ml_dsa_count * ML_DSA_FEE_DISCOUNT + 
                          slh_dsa_count * SLH_DSA_FEE_DISCOUNT) / total_sigs;
    
    adjusted_fee = static_cast<CAmount>(adjusted_fee * avg_discount);
    
    // Ensure we don't go below the base fee
    return std::max(adjusted_fee, base_fee);
}

template<typename T>
bool CheckQuantumSignaturePolicy(const T& tx, std::string& reason)
{
    unsigned int quantum_sig_count = 0;
    size_t largest_sig_size = 0;
    
    for (const auto& input : tx.vin) {
        CScript::const_iterator pc = input.scriptSig.begin();
        std::vector<unsigned char> data;
        opcodetype opcode;
        
        while (pc < input.scriptSig.end()) {
            if (!input.scriptSig.GetOp(pc, opcode, data)) {
                break;
            }
            
            if (data.size() > 100) {
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
template CAmount GetQuantumAdjustedFee<CTransaction>(CAmount base_fee, const CTransaction& tx);
template CAmount GetQuantumAdjustedFee<CMutableTransaction>(CAmount base_fee, const CMutableTransaction& tx);
template bool CheckQuantumSignaturePolicy<CTransaction>(const CTransaction& tx, std::string& reason);
template bool CheckQuantumSignaturePolicy<CMutableTransaction>(const CMutableTransaction& tx, std::string& reason);

} // namespace quantum