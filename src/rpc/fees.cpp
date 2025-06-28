// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <common/messages.h>
#include <core_io.h>
#include <node/context.h>
#include <policy/feerate.h>
#include <policy/fees.h>
#include <policy/quantum_policy.h>
#include <rpc/protocol.h>
#include <rpc/request.h>
#include <rpc/server.h>
#include <rpc/server_util.h>
#include <rpc/util.h>
#include <txmempool.h>
#include <univalue.h>
#include <validationinterface.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

using common::FeeModeFromString;
using common::FeeModesDetail;
using common::InvalidEstimateModeErrorMessage;
using node::NodeContext;

static RPCHelpMan estimatesmartfee()
{
    return RPCHelpMan{
        "estimatesmartfee",
        "Estimates the approximate fee per kilobyte needed for a transaction to begin\n"
        "confirmation within conf_target blocks if possible and return the number of blocks\n"
        "for which the estimate is valid. Uses virtual transaction size as defined\n"
        "in BIP 141 (witness data is discounted).\n"
        "When signature_type is specified, applies quantum signature fee adjustments.\n",
        {
            {"conf_target", RPCArg::Type::NUM, RPCArg::Optional::NO, "Confirmation target in blocks (1 - 1008)"},
            {"estimate_mode", RPCArg::Type::STR, RPCArg::Default{"economical"}, "The fee estimate mode.\n"
              + FeeModesDetail(std::string("default mode will be used"))},
            {"signature_type", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "If specified, calculate fees for quantum signatures: 'ml-dsa' or 'slh-dsa'"},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::NUM, "feerate", /*optional=*/true, "estimate fee rate in " + CURRENCY_UNIT + "/kvB (only present if no errors were encountered)"},
                {RPCResult::Type::NUM, "quantum_feerate", /*optional=*/true, "adjusted fee rate after quantum discount in " + CURRENCY_UNIT + "/kvB (only if signature_type specified)"},
                {RPCResult::Type::NUM, "discount_factor", /*optional=*/true, "discount factor applied (only if signature_type specified)"},
                {RPCResult::Type::STR, "signature_type", /*optional=*/true, "quantum signature type used for estimation (only if signature_type specified)"},
                {RPCResult::Type::ARR, "errors", /*optional=*/true, "Errors encountered during processing (if there are any)",
                    {
                        {RPCResult::Type::STR, "", "error"},
                    }},
                {RPCResult::Type::NUM, "blocks", "block number where estimate was found\n"
                "The request target will be clamped between 2 and the highest target\n"
                "fee estimation is able to return based on how long it has been running.\n"
                "An error is returned if not enough transactions and blocks\n"
                "have been observed to make an estimate for any number of blocks."},
        }},
        RPCExamples{
            HelpExampleCli("estimatesmartfee", "6") +
            HelpExampleCli("estimatesmartfee", "6 \"economical\" \"ml-dsa\"") +
            HelpExampleRpc("estimatesmartfee", "6") +
            HelpExampleRpc("estimatesmartfee", "6, \"economical\", \"slh-dsa\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            CBlockPolicyEstimator& fee_estimator = EnsureAnyFeeEstimator(request.context);
            const NodeContext& node = EnsureAnyNodeContext(request.context);
            const CTxMemPool& mempool = EnsureMemPool(node);

            CHECK_NONFATAL(mempool.m_opts.signals)->SyncWithValidationInterfaceQueue();
            unsigned int max_target = fee_estimator.HighestTargetTracked(FeeEstimateHorizon::LONG_HALFLIFE);
            unsigned int conf_target = ParseConfirmTarget(request.params[0], max_target);
            bool conservative = false;
            if (!request.params[1].isNull()) {
                FeeEstimateMode fee_mode;
                if (!FeeModeFromString(request.params[1].get_str(), fee_mode)) {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, InvalidEstimateModeErrorMessage());
                }
                if (fee_mode == FeeEstimateMode::CONSERVATIVE) conservative = true;
            }

            // Parse optional quantum signature type
            std::string sig_type;
            double discount_factor = 1.0;
            bool quantum_adjustment = false;
            if (!request.params[2].isNull()) {
                sig_type = request.params[2].get_str();
                if (sig_type == "ml-dsa") {
                    discount_factor = quantum::ML_DSA_FEE_DISCOUNT;
                    quantum_adjustment = true;
                } else if (sig_type == "slh-dsa") {
                    discount_factor = quantum::SLH_DSA_FEE_DISCOUNT;
                    quantum_adjustment = true;
                } else {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid signature type. Use 'ml-dsa' or 'slh-dsa'");
                }
            }

            UniValue result(UniValue::VOBJ);
            UniValue errors(UniValue::VARR);
            FeeCalculation feeCalc;
            CFeeRate feeRate{fee_estimator.estimateSmartFee(conf_target, &feeCalc, conservative)};
            if (feeRate != CFeeRate(0)) {
                CFeeRate min_mempool_feerate{mempool.GetMinFee()};
                CFeeRate min_relay_feerate{mempool.m_opts.min_relay_feerate};
                feeRate = std::max({feeRate, min_mempool_feerate, min_relay_feerate});
                
                CAmount base_fee = feeRate.GetFeePerK();
                result.pushKV("feerate", ValueFromAmount(base_fee));
                
                // Apply quantum adjustments if requested
                if (quantum_adjustment) {
                    CAmount quantum_fee = static_cast<CAmount>(base_fee * quantum::QUANTUM_FEE_MULTIPLIER * discount_factor);
                    quantum_fee = std::max(quantum_fee, base_fee);
                    result.pushKV("quantum_feerate", ValueFromAmount(quantum_fee));
                    result.pushKV("discount_factor", discount_factor);
                    result.pushKV("signature_type", sig_type);
                }
            } else {
                errors.push_back("Insufficient data or no feerate found");
                result.pushKV("errors", std::move(errors));
            }
            result.pushKV("blocks", feeCalc.returnedTarget);
            return result;
        },
    };
}

static RPCHelpMan estimaterawfee()
{
    return RPCHelpMan{
        "estimaterawfee",
        "WARNING: This interface is unstable and may disappear or change!\n"
        "\nWARNING: This is an advanced API call that is tightly coupled to the specific\n"
        "implementation of fee estimation. The parameters it can be called with\n"
        "and the results it returns will change if the internal implementation changes.\n"
        "\nEstimates the approximate fee per kilobyte needed for a transaction to begin\n"
        "confirmation within conf_target blocks if possible. Uses virtual transaction size as\n"
        "defined in BIP 141 (witness data is discounted).\n"
        "When signature_type is specified, applies quantum signature fee adjustments.\n",
        {
            {"conf_target", RPCArg::Type::NUM, RPCArg::Optional::NO, "Confirmation target in blocks (1 - 1008)"},
            {"threshold", RPCArg::Type::NUM, RPCArg::Default{0.95}, "The proportion of transactions in a given feerate range that must have been\n"
            "confirmed within conf_target in order to consider those feerates as high enough and proceed to check\n"
            "lower buckets."},
            {"signature_type", RPCArg::Type::STR, RPCArg::Optional::OMITTED, "If specified, calculate fees for quantum signatures: 'ml-dsa' or 'slh-dsa'"},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "Results are returned for any horizon which tracks blocks up to the confirmation target",
            {
                {RPCResult::Type::OBJ, "short", /*optional=*/true, "estimate for short time horizon",
                    {
                        {RPCResult::Type::NUM, "feerate", /*optional=*/true, "estimate fee rate in " + CURRENCY_UNIT + "/kvB"},
                        {RPCResult::Type::NUM, "quantum_feerate", /*optional=*/true, "adjusted fee rate after quantum discount in " + CURRENCY_UNIT + "/kvB (only if signature_type specified)"},
                        {RPCResult::Type::NUM, "decay", "exponential decay (per block) for historical moving average of confirmation data"},
                        {RPCResult::Type::NUM, "scale", "The resolution of confirmation targets at this time horizon"},
                        {RPCResult::Type::OBJ, "pass", /*optional=*/true, "information about the lowest range of feerates to succeed in meeting the threshold",
                        {
                                {RPCResult::Type::NUM, "startrange", "start of feerate range"},
                                {RPCResult::Type::NUM, "endrange", "end of feerate range"},
                                {RPCResult::Type::NUM, "withintarget", "number of txs over history horizon in the feerate range that were confirmed within target"},
                                {RPCResult::Type::NUM, "totalconfirmed", "number of txs over history horizon in the feerate range that were confirmed at any point"},
                                {RPCResult::Type::NUM, "inmempool", "current number of txs in mempool in the feerate range unconfirmed for at least target blocks"},
                                {RPCResult::Type::NUM, "leftmempool", "number of txs over history horizon in the feerate range that left mempool unconfirmed after target"},
                        }},
                        {RPCResult::Type::OBJ, "fail", /*optional=*/true, "information about the highest range of feerates to fail to meet the threshold",
                        {
                            {RPCResult::Type::ELISION, "", ""},
                        }},
                        {RPCResult::Type::ARR, "errors", /*optional=*/true, "Errors encountered during processing (if there are any)",
                        {
                            {RPCResult::Type::STR, "error", ""},
                        }},
                }},
                {RPCResult::Type::OBJ, "medium", /*optional=*/true, "estimate for medium time horizon",
                {
                    {RPCResult::Type::ELISION, "", ""},
                }},
                {RPCResult::Type::OBJ, "long", /*optional=*/true, "estimate for long time horizon",
                {
                    {RPCResult::Type::ELISION, "", ""},
                }},
            }},
        RPCExamples{
            HelpExampleCli("estimaterawfee", "6 0.9")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            CBlockPolicyEstimator& fee_estimator = EnsureAnyFeeEstimator(request.context);
            const NodeContext& node = EnsureAnyNodeContext(request.context);

            CHECK_NONFATAL(node.validation_signals)->SyncWithValidationInterfaceQueue();
            unsigned int max_target = fee_estimator.HighestTargetTracked(FeeEstimateHorizon::LONG_HALFLIFE);
            unsigned int conf_target = ParseConfirmTarget(request.params[0], max_target);
            double threshold = 0.95;
            if (!request.params[1].isNull()) {
                threshold = request.params[1].get_real();
            }
            if (threshold < 0 || threshold > 1) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid threshold");
            }

            // Parse optional quantum signature type
            std::string sig_type;
            double discount_factor = 1.0;
            bool quantum_adjustment = false;
            if (!request.params[2].isNull()) {
                sig_type = request.params[2].get_str();
                if (sig_type == "ml-dsa") {
                    discount_factor = quantum::ML_DSA_FEE_DISCOUNT;
                    quantum_adjustment = true;
                } else if (sig_type == "slh-dsa") {
                    discount_factor = quantum::SLH_DSA_FEE_DISCOUNT;
                    quantum_adjustment = true;
                } else {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid signature type. Use 'ml-dsa' or 'slh-dsa'");
                }
            }

            UniValue result(UniValue::VOBJ);

            for (const FeeEstimateHorizon horizon : ALL_FEE_ESTIMATE_HORIZONS) {
                CFeeRate feeRate;
                EstimationResult buckets;

                // Only output results for horizons which track the target
                if (conf_target > fee_estimator.HighestTargetTracked(horizon)) continue;

                feeRate = fee_estimator.estimateRawFee(conf_target, threshold, horizon, &buckets);
                UniValue horizon_result(UniValue::VOBJ);
                UniValue errors(UniValue::VARR);
                UniValue passbucket(UniValue::VOBJ);
                passbucket.pushKV("startrange", round(buckets.pass.start));
                passbucket.pushKV("endrange", round(buckets.pass.end));
                passbucket.pushKV("withintarget", round(buckets.pass.withinTarget * 100.0) / 100.0);
                passbucket.pushKV("totalconfirmed", round(buckets.pass.totalConfirmed * 100.0) / 100.0);
                passbucket.pushKV("inmempool", round(buckets.pass.inMempool * 100.0) / 100.0);
                passbucket.pushKV("leftmempool", round(buckets.pass.leftMempool * 100.0) / 100.0);
                UniValue failbucket(UniValue::VOBJ);
                failbucket.pushKV("startrange", round(buckets.fail.start));
                failbucket.pushKV("endrange", round(buckets.fail.end));
                failbucket.pushKV("withintarget", round(buckets.fail.withinTarget * 100.0) / 100.0);
                failbucket.pushKV("totalconfirmed", round(buckets.fail.totalConfirmed * 100.0) / 100.0);
                failbucket.pushKV("inmempool", round(buckets.fail.inMempool * 100.0) / 100.0);
                failbucket.pushKV("leftmempool", round(buckets.fail.leftMempool * 100.0) / 100.0);

                // CFeeRate(0) is used to indicate error as a return value from estimateRawFee
                if (feeRate != CFeeRate(0)) {
                    CAmount base_fee = feeRate.GetFeePerK();
                    horizon_result.pushKV("feerate", ValueFromAmount(base_fee));
                    
                    // Apply quantum adjustments if requested
                    if (quantum_adjustment) {
                        CAmount quantum_fee = static_cast<CAmount>(base_fee * quantum::QUANTUM_FEE_MULTIPLIER * discount_factor);
                        quantum_fee = std::max(quantum_fee, base_fee);
                        horizon_result.pushKV("quantum_feerate", ValueFromAmount(quantum_fee));
                    }
                    
                    horizon_result.pushKV("decay", buckets.decay);
                    horizon_result.pushKV("scale", (int)buckets.scale);
                    horizon_result.pushKV("pass", std::move(passbucket));
                    // buckets.fail.start == -1 indicates that all buckets passed, there is no fail bucket to output
                    if (buckets.fail.start != -1) horizon_result.pushKV("fail", std::move(failbucket));
                } else {
                    // Output only information that is still meaningful in the event of error
                    horizon_result.pushKV("decay", buckets.decay);
                    horizon_result.pushKV("scale", (int)buckets.scale);
                    horizon_result.pushKV("fail", std::move(failbucket));
                    errors.push_back("Insufficient data or no feerate found which meets threshold");
                    horizon_result.pushKV("errors", std::move(errors));
                }
                result.pushKV(StringForFeeEstimateHorizon(horizon), std::move(horizon_result));
            }
            return result;
        },
    };
}

static RPCHelpMan estimatetxfee()
{
    return RPCHelpMan{
        "estimatetxfee",
        "Estimates the total fee for a transaction given the number of inputs and outputs.\n"
        "Calculates the transaction size based on the signature type and applies appropriate fees.\n"
        "Supports both standard ECDSA and quantum signatures.\n",
        {
            {"n_inputs", RPCArg::Type::NUM, RPCArg::Optional::NO, "Number of inputs in the transaction"},
            {"n_outputs", RPCArg::Type::NUM, RPCArg::Optional::NO, "Number of outputs in the transaction"},
            {"signature_type", RPCArg::Type::STR, RPCArg::Default{"ecdsa"}, "The signature type: 'ecdsa', 'ml-dsa', or 'slh-dsa'"},
            {"conf_target", RPCArg::Type::NUM, RPCArg::Default{6}, "Confirmation target in blocks (1 - 1008)"},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::NUM, "estimated_size", "estimated transaction size in bytes"},
                {RPCResult::Type::NUM, "estimated_vsize", "estimated virtual size (weight/4)"},
                {RPCResult::Type::NUM, "estimated_weight", "estimated transaction weight"},
                {RPCResult::Type::NUM, "total_fee", /*optional=*/true, "estimated total fee in " + CURRENCY_UNIT},
                {RPCResult::Type::NUM, "feerate", /*optional=*/true, "fee rate used in " + CURRENCY_UNIT + "/kvB"},
                {RPCResult::Type::STR, "signature_type", "signature type used for estimation"},
                {RPCResult::Type::ARR, "errors", /*optional=*/true, "Errors encountered during processing",
                    {
                        {RPCResult::Type::STR, "", "error"},
                    }},
        }},
        RPCExamples{
            HelpExampleCli("estimatetxfee", "2 3") +
            HelpExampleCli("estimatetxfee", "2 3 \"ml-dsa\"") +
            HelpExampleRpc("estimatetxfee", "2, 3, \"slh-dsa\", 10")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            const int n_inputs = request.params[0].getInt<int>();
            const int n_outputs = request.params[1].getInt<int>();
            
            if (n_inputs <= 0 || n_outputs <= 0) {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Number of inputs and outputs must be positive");
            }
            
            // Parse signature type
            std::string sig_type = "ecdsa";
            size_t sig_size = 0;
            size_t pubkey_size = 0;
            bool is_quantum = false;
            double discount_factor = 1.0;
            
            if (!request.params[2].isNull()) {
                sig_type = request.params[2].get_str();
            }
            
            if (sig_type == "ecdsa") {
                sig_size = 71;  // ECDSA signature size (DER encoded)
                pubkey_size = 33;  // Compressed public key
            } else if (sig_type == "ml-dsa") {
                sig_size = 3309;  // ML-DSA-65 signature size
                pubkey_size = 1952;  // ML-DSA-65 public key size
                is_quantum = true;
                discount_factor = quantum::ML_DSA_FEE_DISCOUNT;
            } else if (sig_type == "slh-dsa") {
                sig_size = 49856;  // SLH-DSA-192f signature size
                pubkey_size = 96;   // SLH-DSA-192f public key size
                is_quantum = true;
                discount_factor = quantum::SLH_DSA_FEE_DISCOUNT;
            } else {
                throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid signature type. Use 'ecdsa', 'ml-dsa', or 'slh-dsa'");
            }
            
            // Estimate transaction size
            // Base transaction overhead (version, locktime, input/output counts)
            size_t tx_overhead = 10;
            
            // Input size calculation
            size_t input_size;
            if (is_quantum) {
                // Quantum: outpoint (36) + sequence (4) + scriptSig with quantum signature
                // scriptSig format: [scheme_id:1][sig_len:varint][signature][pubkey_len:varint][pubkey]
                size_t varint_sig_len = sig_size > 252 ? 3 : 1;  // varint encoding for signature length
                size_t varint_pubkey_len = pubkey_size > 252 ? 3 : 1;  // varint encoding for pubkey length
                input_size = 36 + 4 + 1 + varint_sig_len + sig_size + varint_pubkey_len + pubkey_size;
            } else {
                // ECDSA: outpoint (36) + sequence (4) + scriptSig
                // Standard P2PKH scriptSig: [sig_len:1][signature][pubkey_len:1][pubkey]
                input_size = 36 + 4 + 1 + sig_size + 1 + pubkey_size;
            }
            
            // Output size: value (8) + script_len (1) + script (25 for P2PKH)
            size_t output_size = 8 + 1 + 25;
            
            size_t total_size = tx_overhead + (input_size * n_inputs) + (output_size * n_outputs);
            // For P2WSH transactions (quantum), witness data gets discount
            // For P2PKH transactions (standard ECDSA), no witness discount
            size_t weight = total_size * 4;  // Conservative estimate
            size_t vsize = (weight + 3) / 4;
            
            UniValue result(UniValue::VOBJ);
            result.pushKV("estimated_size", (int64_t)total_size);
            result.pushKV("estimated_vsize", (int64_t)vsize);
            result.pushKV("estimated_weight", (int64_t)weight);
            result.pushKV("signature_type", sig_type);
            
            // Get fee rate if requested
            if (!request.params[3].isNull() || request.params[3].isNull()) {
                CBlockPolicyEstimator& fee_estimator = EnsureAnyFeeEstimator(request.context);
                const NodeContext& node = EnsureAnyNodeContext(request.context);
                const CTxMemPool& mempool = EnsureMemPool(node);
                
                unsigned int conf_target = request.params[3].isNull() ? 6 : request.params[3].getInt<int>();
                unsigned int max_target = fee_estimator.HighestTargetTracked(FeeEstimateHorizon::LONG_HALFLIFE);
                conf_target = ParseConfirmTarget(conf_target, max_target);
                
                FeeCalculation feeCalc;
                CFeeRate feeRate{fee_estimator.estimateSmartFee(conf_target, &feeCalc, false)};
                
                if (feeRate != CFeeRate(0)) {
                    CFeeRate min_mempool_feerate{mempool.GetMinFee()};
                    CFeeRate min_relay_feerate{mempool.m_opts.min_relay_feerate};
                    feeRate = std::max({feeRate, min_mempool_feerate, min_relay_feerate});
                    
                    CAmount base_fee_rate = feeRate.GetFeePerK();
                    CAmount effective_fee_rate = base_fee_rate;
                    
                    // Apply quantum fee adjustments if needed
                    if (is_quantum) {
                        effective_fee_rate = static_cast<CAmount>(base_fee_rate * quantum::QUANTUM_FEE_MULTIPLIER * discount_factor);
                        effective_fee_rate = std::max(effective_fee_rate, base_fee_rate);
                    }
                    
                    // Calculate total fee based on vsize
                    CAmount total_fee = (effective_fee_rate * vsize) / 1000;
                    
                    result.pushKV("total_fee", ValueFromAmount(total_fee));
                    result.pushKV("feerate", ValueFromAmount(effective_fee_rate));
                } else {
                    UniValue errors(UniValue::VARR);
                    errors.push_back("Insufficient data or no feerate found");
                    result.pushKV("errors", std::move(errors));
                }
            }
            
            return result;
        },
    };
}

void RegisterFeeRPCCommands(CRPCTable& t)
{
    static const CRPCCommand commands[]{
        {"util", &estimatesmartfee},
        {"hidden", &estimaterawfee},
        {"util", &estimatetxfee},
    };
    for (const auto& c : commands) {
        t.appendCommand(c.name, &c);
    }
}
