// Copyright (c) 2024-present The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bitcoin-build-config.h> // IWYU pragma: keep

#include <core_io.h>
#include <key_io.h>
#include <rpc/util.h>
#include <script/script.h>
#include <script/solver.h>
#include <util/bip32.h>
#include <util/translation.h>
#include <wallet/receive.h>
#include <wallet/rpc/util.h>
#include <wallet/wallet.h>
#include <wallet/quantum_scriptpubkeyman.h>
#include <crypto/quantum_key.h>
#include <quantum_address.h>
#include <script/quantum_signature.h>
#include <common/signmessage.h>
#include <util/strencodings.h>

#include <univalue.h>

namespace wallet {

using quantum::CQuantumKey;
using quantum::CQuantumPubKey;
using quantum::QuantumAddressType;

RPCHelpMan getnewquantumaddress()
{
    return RPCHelpMan{
        "getnewquantumaddress",
        "Returns a new quantum-safe Bitcoin address for receiving payments.\n"
        "This address uses post-quantum cryptographic algorithms that are resistant to quantum computer attacks.\n"
        "If 'label' is specified, it is added to the address book so payments received with the address will be associated with 'label'.\n",
        {
            {"label", RPCArg::Type::STR, RPCArg::Default{""}, "The label name for the address to be linked to. It can also be set to the empty string \"\" to represent the default label. The label does not need to exist, it will be created if there is no label by the given name."},
            {"address_type", RPCArg::Type::STR, RPCArg::Default{"ml-dsa"}, "The quantum signature algorithm to use. Options are \"ml-dsa\" (ML-DSA-65, recommended for most use) and \"slh-dsa\" (SLH-DSA-192f, for high-security applications)."},
        },
        RPCResult{
            RPCResult::Type::STR, "address", "The new quantum-safe bitcoin address"
        },
        RPCExamples{
            HelpExampleCli("getnewquantumaddress", "")
            + HelpExampleCli("getnewquantumaddress", "\"my_quantum_address\"")
            + HelpExampleCli("getnewquantumaddress", "\"\" \"slh-dsa\"")
            + HelpExampleRpc("getnewquantumaddress", "\"my_quantum_address\", \"ml-dsa\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            std::shared_ptr<CWallet> const pwallet = GetWalletForJSONRPCRequest(request);
            if (!pwallet) return UniValue::VNULL;

            LOCK(pwallet->cs_wallet);

            if (!pwallet->CanGetAddresses()) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Error: This wallet has no available keys");
            }

            // Parse the label first so we don't generate a key if there's an error
            const std::string label{LabelFromValue(request.params[0])};

            // Parse the quantum algorithm type
            quantum::SignatureSchemeID scheme_id = quantum::SCHEME_ML_DSA_65;
            if (!request.params[1].isNull()) {
                std::string algo = request.params[1].get_str();
                if (algo == "ml-dsa") {
                    scheme_id = quantum::SCHEME_ML_DSA_65;
                } else if (algo == "slh-dsa") {
                    scheme_id = quantum::SCHEME_SLH_DSA_192F;
                } else {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Unknown quantum algorithm '%s'. Use 'ml-dsa' or 'slh-dsa'.", algo));
                }
            }

            // Check if wallet has quantum key support
            auto spk_managers = pwallet->GetAllScriptPubKeyMans();
            QuantumScriptPubKeyMan* quantum_spkm = nullptr;
            
            // Find or create a quantum script pubkey manager
            for (auto spkm : spk_managers) {
                quantum_spkm = dynamic_cast<QuantumScriptPubKeyMan*>(spkm);
                if (quantum_spkm) break;
            }
            
            if (!quantum_spkm) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Error: This wallet does not support quantum keys. Please create a new wallet with quantum support.");
            }
            
            // Set the desired address type based on scheme
            QuantumAddressType addr_type = (scheme_id == quantum::SCHEME_ML_DSA_65) ?
                QuantumAddressType::P2QPKH_ML_DSA : QuantumAddressType::P2QPKH_SLH_DSA;
            quantum_spkm->SetQuantumAddressType(addr_type);
            
            // Generate new quantum address
            auto op_dest = quantum_spkm->GetNewDestination(OutputType::LEGACY);
            if (!op_dest) {
                throw JSONRPCError(RPC_WALLET_ERROR, util::ErrorString(op_dest).original);
            }

            // Set the label
            pwallet->SetAddressBook(*op_dest, label, AddressPurpose::RECEIVE);

            // Encode with quantum prefix
            int quantum_type = (scheme_id == quantum::SCHEME_ML_DSA_65) ? 1 : 2;
            std::string address = EncodeQuantumDestination(*op_dest, quantum_type);
            
            return address;
        },
    };
}

RPCHelpMan validatequantumaddress()
{
    return RPCHelpMan{
        "validatequantumaddress",
        "Validates a quantum-safe Bitcoin address and returns information about it.\n",
        {
            {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "The quantum bitcoin address to validate"},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::BOOL, "isvalid", "If the address is valid or not"},
                {RPCResult::Type::STR, "address", /*optional=*/true, "The quantum bitcoin address validated"},
                {RPCResult::Type::STR, "algorithm", /*optional=*/true, "The quantum signature algorithm used (ml-dsa or slh-dsa)"},
                {RPCResult::Type::STR, "type", /*optional=*/true, "The type of address (P2QPKH_ML_DSA, P2QPKH_SLH_DSA, or P2QSH)"},
                {RPCResult::Type::STR_HEX, "scriptPubKey", /*optional=*/true, "The hex-encoded output script generated by the address"},
                {RPCResult::Type::BOOL, "isscript", /*optional=*/true, "If the key is a script"},
                {RPCResult::Type::BOOL, "iswitness", /*optional=*/true, "If the address is a witness address (always false for quantum)"},
                {RPCResult::Type::BOOL, "isquantum", /*optional=*/true, "If the address is a quantum address"},
            }
        },
        RPCExamples{
            HelpExampleCli("validatequantumaddress", "\"Q1abcd...\"")
            + HelpExampleRpc("validatequantumaddress", "\"Q1abcd...\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            UniValue ret(UniValue::VOBJ);
            
            std::string error_msg;
            std::string addr_str = request.params[0].get_str();
            CTxDestination dest = DecodeQuantumDestination(addr_str, error_msg);
            bool isValid = IsValidDestination(dest);
            
            ret.pushKV("isvalid", isValid);
            
            if (isValid) {
                // Re-encode with quantum prefix if it was a quantum address
                if (IsQuantumAddress(addr_str)) {
                    int quantum_type = GetQuantumAddressType(addr_str);
                    ret.pushKV("address", EncodeQuantumDestination(dest, quantum_type));
                } else {
                    ret.pushKV("address", EncodeDestination(dest));
                }
                
                // Check if it's a quantum address by looking at the prefix
                if (IsQuantumAddress(addr_str)) {
                    // Quantum address
                    if (addr_str.substr(0, 2) == "Q1") {
                        ret.pushKV("algorithm", "ml-dsa");
                        ret.pushKV("type", "P2QPKH_ML_DSA");
                    } else if (addr_str.substr(0, 2) == "Q2") {
                        ret.pushKV("algorithm", "slh-dsa");
                        ret.pushKV("type", "P2QPKH_SLH_DSA");
                    } else if (addr_str.substr(0, 2) == "Q3") {
                        ret.pushKV("type", "P2QSH");
                    }
                    
                    // Generate scriptPubKey
                    CScript scriptPubKey = GetScriptForDestination(dest);
                    ret.pushKV("scriptPubKey", HexStr(scriptPubKey));
                    ret.pushKV("isscript", addr_str.substr(0, 2) == "Q3");
                    ret.pushKV("iswitness", false);
                    ret.pushKV("isquantum", true);
                } else {
                    // Not a quantum address
                    ret.pushKV("isquantum", false);
                }
            }
            
            return ret;
        },
    };
}

RPCHelpMan getquantuminfo()
{
    return RPCHelpMan{
        "getquantuminfo",
        "Returns information about quantum-safe signature support in this wallet.\n",
        {},
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::BOOL, "enabled", "Whether quantum signature support is enabled"},
                {RPCResult::Type::BOOL, "activated", "Whether quantum signatures are activated on the network"},
                {RPCResult::Type::NUM, "quantum_keys", "Number of quantum keys in the wallet"},
                {RPCResult::Type::ARR, "supported_algorithms", "List of supported quantum algorithms",
                {
                    {RPCResult::Type::OBJ, "", "",
                    {
                        {RPCResult::Type::STR, "name", "Algorithm name"},
                        {RPCResult::Type::STR, "description", "Algorithm description"},
                        {RPCResult::Type::NUM, "signature_size", "Typical signature size in bytes"},
                        {RPCResult::Type::NUM, "public_key_size", "Public key size in bytes"},
                    }},
                }},
            }
        },
        RPCExamples{
            HelpExampleCli("getquantuminfo", "")
            + HelpExampleRpc("getquantuminfo", "")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            const std::shared_ptr<const CWallet> pwallet = GetWalletForJSONRPCRequest(request);
            if (!pwallet) return UniValue::VNULL;

            LOCK(pwallet->cs_wallet);

            UniValue ret(UniValue::VOBJ);
            
            // Check if quantum signatures are enabled
            ret.pushKV("enabled", true); // Always enabled in QSBitcoin
            
            // Check activation status (simplified for now)
            ret.pushKV("activated", true); // For testing purposes
            
            // Count quantum keys
            int quantum_key_count = 0;
            auto spk_managers = pwallet->GetAllScriptPubKeyMans();
            for (auto spkm : spk_managers) {
                QuantumScriptPubKeyMan* quantum_spkm = dynamic_cast<QuantumScriptPubKeyMan*>(spkm);
                if (quantum_spkm) {
                    quantum_key_count += quantum_spkm->GetKeyPoolSize();
                }
            }
            ret.pushKV("quantum_keys", quantum_key_count);
            
            // List supported algorithms
            UniValue algos(UniValue::VARR);
            
            UniValue ml_dsa(UniValue::VOBJ);
            ml_dsa.pushKV("name", "ml-dsa");
            ml_dsa.pushKV("description", "ML-DSA-65 (Dilithium) - Recommended for general use");
            ml_dsa.pushKV("signature_size", 3293);
            ml_dsa.pushKV("public_key_size", 1952);
            algos.push_back(std::move(ml_dsa));
            
            UniValue slh_dsa(UniValue::VOBJ);
            slh_dsa.pushKV("name", "slh-dsa");
            slh_dsa.pushKV("description", "SLH-DSA-192f (SPHINCS+) - For high-security applications");
            slh_dsa.pushKV("signature_size", 35664);
            slh_dsa.pushKV("public_key_size", 48);
            algos.push_back(std::move(slh_dsa));
            
            ret.pushKV("supported_algorithms", std::move(algos));
            
            return ret;
        },
    };
}

RPCHelpMan signmessagewithscheme()
{
    return RPCHelpMan{
        "signmessagewithscheme",
        "Sign a message with the private key of an address using a specific signature scheme.\n"
        "This allows signing with quantum-safe algorithms.\n",
        {
            {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "The bitcoin address to use for the private key."},
            {"message", RPCArg::Type::STR, RPCArg::Optional::NO, "The message to create a signature of."},
            {"scheme", RPCArg::Type::STR, RPCArg::Default{"auto"}, "The signature scheme to use: 'ecdsa', 'ml-dsa', 'slh-dsa', or 'auto' (uses the scheme associated with the address)."},
        },
        RPCResult{
            RPCResult::Type::OBJ, "", "",
            {
                {RPCResult::Type::STR_HEX, "signature", "The signature of the message encoded in base 64"},
                {RPCResult::Type::STR, "algorithm", "The algorithm used for signing"},
            }
        },
        RPCExamples{
            "\nUnlock the wallet for 30 seconds\n"
            + HelpExampleCli("walletpassphrase", "\"mypassphrase\" 30") +
            "\nCreate the signature\n"
            + HelpExampleCli("signmessagewithscheme", "\"Q1VaN9zXfBk...\" \"my message\" \"ml-dsa\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("signmessagewithscheme", "\"Q1VaN9zXfBk...\", \"my message\", \"ml-dsa\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            std::shared_ptr<CWallet> const pwallet = GetWalletForJSONRPCRequest(request);
            if (!pwallet) return UniValue::VNULL;

            LOCK(pwallet->cs_wallet);

            EnsureWalletIsUnlocked(*pwallet);

            std::string strAddress = request.params[0].get_str();
            std::string strMessage = request.params[1].get_str();
            
            CTxDestination dest = DecodeDestination(strAddress);
            if (!IsValidDestination(dest)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
            }

            // Determine signature scheme
            std::string scheme_str = "auto";
            if (!request.params[2].isNull()) {
                scheme_str = request.params[2].get_str();
            }

            // Get the private key for this address
            const PKHash* pkhash = std::get_if<PKHash>(&dest);
            if (!pkhash) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Address does not refer to a key");
            }

            // Determine which scheme to use
            ::quantum::SignatureSchemeID scheme_id = ::quantum::SCHEME_ECDSA;
            std::string actual_algo = "ecdsa";
            
            // Check if it's a quantum address
            if (strAddress.length() > 2 && strAddress[0] == 'Q') {
                if (strAddress.substr(0, 2) == "Q1") {
                    scheme_id = ::quantum::SCHEME_ML_DSA_65;
                    actual_algo = "ml-dsa";
                } else if (strAddress.substr(0, 2) == "Q2") {
                    scheme_id = ::quantum::SCHEME_SLH_DSA_192F;
                    actual_algo = "slh-dsa";
                }
            }
            
            // Override with explicit scheme if provided
            if (scheme_str != "auto") {
                if (scheme_str == "ecdsa") {
                    scheme_id = ::quantum::SCHEME_ECDSA;
                    actual_algo = "ecdsa";
                } else if (scheme_str == "ml-dsa") {
                    scheme_id = ::quantum::SCHEME_ML_DSA_65;
                    actual_algo = "ml-dsa";
                } else if (scheme_str == "slh-dsa") {
                    scheme_id = ::quantum::SCHEME_SLH_DSA_192F;
                    actual_algo = "slh-dsa";
                } else {
                    throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Unknown signature scheme '%s'. Use 'ecdsa', 'ml-dsa', 'slh-dsa', or 'auto'.", scheme_str));
                }
            }
            
            // Try to sign based on the scheme
            std::string signature;
            if (scheme_id == ::quantum::SCHEME_ECDSA) {
                // Use standard ECDSA signing
                SigningResult res = pwallet->SignMessage(strMessage, *pkhash, signature);
                if (res != SigningResult::OK) {
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, SigningResultString(res));
                }
            } else {
                // Use quantum signing
                auto spk_managers = pwallet->GetAllScriptPubKeyMans();
                bool was_signed = false;
                
                for (auto spkm : spk_managers) {
                    QuantumScriptPubKeyMan* quantum_spkm = dynamic_cast<QuantumScriptPubKeyMan*>(spkm);
                    if (quantum_spkm) {
                        SigningResult res = quantum_spkm->SignMessage(strMessage, *pkhash, signature);
                        if (res == SigningResult::OK) {
                            was_signed = true;
                            break;
                        }
                    }
                }
                
                if (!was_signed) {
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Private key not available for quantum signing");
                }
            }
            
            UniValue ret(UniValue::VOBJ);
            ret.pushKV("signature", signature);
            ret.pushKV("algorithm", actual_algo);
            
            return ret;
        },
    };
}

} // namespace wallet