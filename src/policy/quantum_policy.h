// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POLICY_QUANTUM_POLICY_H
#define BITCOIN_POLICY_QUANTUM_POLICY_H

#include <consensus/consensus.h>
#include <policy/policy.h>
#include <script/quantum_signature.h>

#include <cstdint>

namespace quantum {

/**
 * Quantum signature specific policy constants
 * These constants define the policy rules for transactions using quantum signatures
 */

/** Maximum weight for a standard transaction with quantum signatures
 * This is higher than MAX_STANDARD_TX_WEIGHT to accommodate larger quantum signatures
 * while still preventing DoS attacks. Set to 1MB weight (250KB actual size)
 */
static constexpr int32_t MAX_STANDARD_TX_WEIGHT_QUANTUM = 1000000;

/** Maximum number of quantum signatures in a standard transaction
 * Limited to prevent excessive validation time
 */
static constexpr unsigned int MAX_STANDARD_QUANTUM_SIGS = 10;

/** Maximum size of a single quantum signature in a standard transaction */
static constexpr unsigned int MAX_STANDARD_QUANTUM_SIG_SIZE = 50000; // ~49KB for SLH-DSA

/** Minimum fee rate multiplier for quantum transactions
 * Quantum signatures require more resources to validate, so we require higher fees
 */
static constexpr double QUANTUM_FEE_MULTIPLIER = 1.5;

/** Discount factor for ML-DSA signatures to encourage adoption
 * Applied as a reduction to the calculated fee
 */
static constexpr double ML_DSA_FEE_DISCOUNT = 0.9; // 10% discount

/** Discount factor for SLH-DSA signatures
 * Smaller discount due to much larger size
 */
static constexpr double SLH_DSA_FEE_DISCOUNT = 0.95; // 5% discount

/**
 * Check if a transaction weight is within quantum-safe limits
 * @param[in] weight Transaction weight
 * @param[in] has_quantum_sigs Whether the transaction contains quantum signatures
 * @return true if weight is acceptable
 */
inline bool IsStandardTxWeight(int64_t weight, bool has_quantum_sigs)
{
    if (has_quantum_sigs) {
        return weight <= MAX_STANDARD_TX_WEIGHT_QUANTUM;
    }
    return weight <= MAX_STANDARD_TX_WEIGHT;
}

/**
 * Count quantum signatures in a transaction
 * @param[in] tx Transaction to analyze
 * @return Number of quantum signatures found
 */
template<typename T>
unsigned int CountQuantumSignatures(const T& tx);

/**
 * Check if a transaction has any quantum signatures
 * @param[in] tx Transaction to check
 * @return true if any input uses quantum signatures
 */
template<typename T>
bool HasQuantumSignatures(const T& tx);

/**
 * Calculate adjusted fee for a transaction with quantum signatures
 * @param[in] base_fee Base fee calculated normally
 * @param[in] tx Transaction to analyze
 * @return Adjusted fee considering quantum signature discounts
 */
template<typename T>
CAmount GetQuantumAdjustedFee(CAmount base_fee, const T& tx);

/**
 * Validate quantum signature counts and sizes
 * @param[in] tx Transaction to validate
 * @param[out] reason Failure reason if validation fails
 * @return true if transaction passes quantum signature policy
 */
template<typename T>
bool CheckQuantumSignaturePolicy(const T& tx, std::string& reason);

} // namespace quantum

#endif // BITCOIN_POLICY_QUANTUM_POLICY_H