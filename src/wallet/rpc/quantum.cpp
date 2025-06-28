// Copyright (c) 2024-present The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bitcoin-build-config.h> // IWYU pragma: keep

#include <core_io.h>
#include <key_io.h>
#include <rpc/util.h>
#include <script/script.h>
#include <script/solver.h>
#include <script/descriptor.h>
#include <script/quantum_witness.h>
#include <util/bip32.h>
#include <util/translation.h>
#include <wallet/receive.h>
#include <wallet/rpc/util.h>
#include <wallet/wallet.h>
#include <wallet/walletdb.h>
#include <wallet/scriptpubkeyman.h>
#include <wallet/quantum_keystore.h>
#include <wallet/quantum_wallet_setup.h>
#include <crypto/quantum_key.h>
#include <quantum_address.h>
#include <script/quantum_signature.h>
#include <common/signmessage.h>
#include <util/strencodings.h>
#include <hash.h>
#include <crypto/sha256.h>

#include <univalue.h>

namespace wallet {

using quantum::CQuantumKey;
using quantum::CQuantumPubKey;

RPCHelpMan getnewquantumaddress()
{
    return RPCHelpMan{
        "getnewquantumaddress",
        "Returns a new quantum-safe Bitcoin address for receiving payments.\n"
        "This address uses post-quantum cryptographic algorithms that are resistant to quantum computer attacks.\n"
        "If 'label' is specified, it is added to the address book so payments received with the address will be associated with 'label'.\n",
        {
            {"label", RPCArg::Type::STR, RPCArg::Default{""}, "The label name for the address to be linked to. It can also be set to the empty string \"\" to represent the default label. The label does not need to exist, it will be created if there is no label by the given name."},
            {"address_type", RPCArg::Type::STR, RPCArg::Optional::NO, "The quantum signature algorithm to use. Must be either \"ml-dsa\" (ML-DSA-65, recommended for most use) or \"slh-dsa\" (SLH-DSA-192f, for high-security applications)."},
        },
        RPCResult{
            RPCResult::Type::STR, "address", "The new quantum-safe bitcoin address (bech32 P2WSH format)"
        },
        RPCExamples{
            HelpExampleCli("getnewquantumaddress", "\"\" \"ml-dsa\"")
            + HelpExampleCli("getnewquantumaddress", "\"my_quantum_address\" \"ml-dsa\"")
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

            // Parse the quantum algorithm type (required parameter)
            std::string algo = request.params[1].get_str();
            quantum::SignatureSchemeID scheme_id;
            if (algo == "ml-dsa") {
                scheme_id = quantum::SCHEME_ML_DSA_65;
            } else if (algo == "slh-dsa") {
                scheme_id = quantum::SCHEME_SLH_DSA_192F;
            } else {
                throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Unknown quantum algorithm '%s'. Must be either 'ml-dsa' or 'slh-dsa'.", algo));
            }

            // For descriptor wallets, we need to find or create a quantum descriptor
            if (!pwallet->IsWalletFlagSet(WALLET_FLAG_DESCRIPTORS)) {
                throw JSONRPCError(RPC_WALLET_ERROR, "Quantum addresses require descriptor wallets");
            }
            
            // Find a quantum descriptor SPKM that can provide addresses
            DescriptorScriptPubKeyMan* quantum_spkm = nullptr;
            std::string target_desc_prefix = (scheme_id == quantum::SCHEME_ML_DSA_65) ? "qpkh(quantum:ml-dsa:" : "qpkh(quantum:slh-dsa:";
            
            for (auto& spkm : pwallet->GetAllScriptPubKeyMans()) {
                auto desc_spkm = dynamic_cast<DescriptorScriptPubKeyMan*>(spkm);
                if (desc_spkm) {
                    // Check if this is a quantum descriptor
                    std::string desc_str;
                    if (desc_spkm->GetDescriptorString(desc_str, false)) {
                        if (desc_str.find(target_desc_prefix) != std::string::npos) {
                            // Found a quantum descriptor of the right type
                            quantum_spkm = desc_spkm;
                            break;
                        }
                    }
                }
            }
            
            // If no quantum descriptor exists, skip for now (temporarily disabled due to hanging)
            // TODO: Fix quantum descriptor creation hanging issue
            if (!quantum_spkm) {
                // Skip descriptor creation for now
            }
            
            // If we still don't have a quantum descriptor, fall back to temporary approach
            if (!quantum_spkm) {
                // Fallback to temporary keystore approach for now
                // Generate new quantum key
                auto key = std::make_unique<CQuantumKey>();
                ::quantum::KeyType key_type = (scheme_id == quantum::SCHEME_ML_DSA_65) ? 
                    ::quantum::KeyType::ML_DSA_65 : ::quantum::KeyType::SLH_DSA_192F;
                
                key->MakeNewKey(key_type);
                if (!key->IsValid()) {
                    throw JSONRPCError(RPC_WALLET_ERROR, "Failed to generate quantum key");
                }
                
                // Get the public key
                CQuantumPubKey pubkey = key->GetPubKey();
                CKeyID keyid = pubkey.GetID();
                
                // Add to descriptor SPKM if possible
                bool added_to_spkm = false;
                DescriptorScriptPubKeyMan* quantum_spkm_used = nullptr;
                for (auto& spkm : pwallet->GetAllScriptPubKeyMans()) {
                    auto desc_spkm = dynamic_cast<DescriptorScriptPubKeyMan*>(spkm);
                    if (desc_spkm) {
                        LogPrintf("Trying to add quantum key to descriptor %s (HD: %s)\n", 
                                 desc_spkm->GetID().ToString(), desc_spkm->IsHDEnabled() ? "yes" : "no");
                        // Try to add to any descriptor SPKM for now
                        if (desc_spkm->AddQuantumKey(keyid, std::move(key))) {
                            LogPrintf("Added quantum key to descriptor %s\n", desc_spkm->GetID().ToString());
                            added_to_spkm = true;
                            quantum_spkm_used = desc_spkm;
                            break;
                        }
                    }
                }
                
                // If we couldn't add to any SPKM, fall back to global keystore
                if (!added_to_spkm) {
                    LogPrintf("Failed to add quantum key to any descriptor SPKM, falling back to global keystore\n");
                    // Make a new key since we moved the previous one
                    auto key2 = std::make_unique<CQuantumKey>();
                    key2->MakeNewKey(key_type);
                    if (!g_quantum_keystore->AddQuantumKey(keyid, std::move(key2))) {
                        throw JSONRPCError(RPC_WALLET_ERROR, "Failed to add quantum key to keystore");
                    }
                }
                
                // For now, we continue with the manual P2WSH approach
                // TODO: Implement proper descriptor import once wallet descriptor management is ready
                
                // Create the witness script
                CScript witnessScript = quantum::CreateQuantumWitnessScript(pubkey);
                
                // Create the P2WSH script for this quantum key
                CScript scriptPubKey = quantum::CreateQuantumP2WSH(pubkey);
                
                // Extract the witness script hash to create a destination
                std::vector<unsigned char> witnessprogram;
                int witnessversion;
                if (!scriptPubKey.IsWitnessProgram(witnessversion, witnessprogram) || 
                    witnessversion != 0 || witnessprogram.size() != 32) {
                    throw JSONRPCError(RPC_WALLET_ERROR, "Failed to create valid P2WSH script");
                }
                
                CTxDestination dest = WitnessV0ScriptHash(uint256(witnessprogram));
                
                // Store the witness script in the global quantum keystore
                // For P2WSH, the script ID is RIPEMD160 of the witness program (SHA256 of witness script)
                uint256 witnesshash;
                CSHA256().Write(witnessScript.data(), witnessScript.size()).Finalize(witnesshash.begin());
                CScriptID scriptID{RIPEMD160(witnesshash)};
                g_quantum_keystore->AddWitnessScript(scriptID, witnessScript);
                
                // Also store the P2WSH script itself for IsMine checks
                CScriptID p2wsh_id(scriptPubKey);
                g_quantum_keystore->AddWitnessScript(p2wsh_id, scriptPubKey);
                
                // Log the descriptor string that would be used
                std::string key_str;
                if (key_type == quantum::KeyType::ML_DSA_65) {
                    key_str = "quantum:ml-dsa:" + HexStr(pubkey.GetKeyData());
                } else if (key_type == quantum::KeyType::SLH_DSA_192F) {
                    key_str = "quantum:slh-dsa:" + HexStr(pubkey.GetKeyData());
                }
                std::string desc_str = "wsh(qpk(" + key_str + "))";
                
                LogPrintf("Added witness script %s for quantum key %s (would use descriptor: %s)\n", 
                         scriptID.ToString(), keyid.ToString(), desc_str);
                
                // If we have a descriptor SPKM, add the script so it's recognized as "mine"
                if (quantum_spkm_used) {
                    quantum_spkm_used->AddScriptPubKey(scriptPubKey);
                    LogPrintf("Added P2WSH script to descriptor SPKM for ownership tracking\n");
                }
                
                // Add to address book
                pwallet->SetAddressBook(dest, label, AddressPurpose::RECEIVE);
                
                // Encode as bech32 address (bc1q...)
                std::string address = EncodeDestination(dest);
                
                LogPrintf("Created quantum address %s with witness script size %d bytes\n", 
                         address, witnessScript.size());
                
                return address;
            }
            
            // Use the quantum descriptor to get a new destination
            auto dest_result = quantum_spkm->GetNewDestination(OutputType::LEGACY);
            if (!dest_result) {
                throw JSONRPCError(RPC_WALLET_ERROR, util::ErrorString(dest_result).original);
            }
            
            // Add to address book
            pwallet->SetAddressBook(*dest_result, label, AddressPurpose::RECEIVE);
            
            // Encode as standard bech32 address
            std::string address = EncodeDestination(*dest_result);
            
            return address;
        },
    };
}

RPCHelpMan validatequantumaddress()
{
    return RPCHelpMan{
        "validatequantumaddress",
        "DEPRECATED: Quantum addresses now use standard bech32 P2WSH format.\n"
        "Use validateaddress instead. This RPC is kept for backward compatibility.\n",
        {
            {"address", RPCArg::Type::STR, RPCArg::Optional::NO, "The bitcoin address to validate"},
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
                {RPCResult::Type::BOOL, "iswitness", /*optional=*/true, "If the address is a witness address"},
                {RPCResult::Type::NUM, "witness_version", /*optional=*/true, "The witness version (0 for P2WSH)"},
                {RPCResult::Type::STR_HEX, "witness_program", /*optional=*/true, "The witness program"},
                {RPCResult::Type::BOOL, "isquantum", /*optional=*/true, "If the address is a quantum address"},
                {RPCResult::Type::STR, "note", /*optional=*/true, "Additional information about the address"},
            }
        },
        RPCExamples{
            HelpExampleCli("validatequantumaddress", "\"bc1qrp33g0q5c5txsp9arysrx4k6zdkfs4nce4xj0gdcccefvpysxf3qccfmv3\"")
            + HelpExampleRpc("validatequantumaddress", "\"bc1qrp33g0q5c5txsp9arysrx4k6zdkfs4nce4xj0gdcccefvpysxf3qccfmv3\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            UniValue ret(UniValue::VOBJ);
            
            std::string addr_str = request.params[0].get_str();
            CTxDestination dest = DecodeDestination(addr_str);
            bool isValid = IsValidDestination(dest);
            
            ret.pushKV("isvalid", isValid);
            
            if (isValid) {
                ret.pushKV("address", EncodeDestination(dest));
                
                // Check if it's a P2WSH address
                if (auto witness_script_hash = std::get_if<WitnessV0ScriptHash>(&dest)) {
                    ret.pushKV("iswitness", true);
                    ret.pushKV("witness_version", 0);
                    ret.pushKV("witness_program", HexStr(*witness_script_hash));
                    
                    // Generate scriptPubKey
                    CScript scriptPubKey = GetScriptForDestination(dest);
                    ret.pushKV("scriptPubKey", HexStr(scriptPubKey));
                    
                    // Note: We can't determine if this is a quantum address just from the P2WSH
                    // since it looks like any other P2WSH address
                    ret.pushKV("note", "Quantum addresses use P2WSH format. Cannot determine if quantum without witness script.");
                } else {
                    ret.pushKV("iswitness", false);
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
            
            // First check descriptor wallets (preferred)
            if (pwallet->IsWalletFlagSet(WALLET_FLAG_DESCRIPTORS)) {
                for (auto& spkm : pwallet->GetAllScriptPubKeyMans()) {
                    // Count quantum keys in each SPKM
                    auto desc_spkm = dynamic_cast<DescriptorScriptPubKeyMan*>(spkm);
                    if (desc_spkm) {
                        quantum_key_count += desc_spkm->GetQuantumKeyCount();
                    }
                }
            }
            
            // Fall back to global keystore for legacy support
            if (quantum_key_count == 0 && g_quantum_keystore) {
                quantum_key_count = static_cast<int>(g_quantum_keystore->GetKeyCount());
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
            + HelpExampleCli("signmessagewithscheme", "\"bcrt1q...\" \"my message\" \"ml-dsa\"") +
            "\nAs a JSON-RPC call\n"
            + HelpExampleRpc("signmessagewithscheme", "\"bcrt1q...\", \"my message\", \"ml-dsa\"")
        },
        [&](const RPCHelpMan& self, const JSONRPCRequest& request) -> UniValue
        {
            std::shared_ptr<CWallet> const pwallet = GetWalletForJSONRPCRequest(request);
            if (!pwallet) return UniValue::VNULL;

            LOCK(pwallet->cs_wallet);

            EnsureWalletIsUnlocked(*pwallet);

            std::string strAddress = request.params[0].get_str();
            std::string strMessage = request.params[1].get_str();
            
            // Decode address (quantum addresses now use standard bech32 format)
            CTxDestination dest = DecodeDestination(strAddress);
            
            if (!IsValidDestination(dest)) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Invalid address");
            }

            // Determine signature scheme
            std::string scheme_str = "auto";
            if (!request.params[2].isNull()) {
                scheme_str = request.params[2].get_str();
            }

            // For P2WSH addresses, we need to get the witness script hash
            const WitnessV0ScriptHash* witness_script_hash = std::get_if<WitnessV0ScriptHash>(&dest);
            const PKHash* pkhash = std::get_if<PKHash>(&dest);
            
            if (!witness_script_hash && !pkhash) {
                throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Address type not supported for message signing");
            }

            // Determine which scheme to use
            ::quantum::SignatureSchemeID scheme_id = ::quantum::SCHEME_ECDSA;
            std::string actual_algo = "ecdsa";
            
            // For P2WSH addresses, we can't auto-detect the scheme without the witness script
            // User must specify the scheme explicitly or we'll try to find a matching key
            
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
                if (!pkhash) {
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "ECDSA signing requires P2PKH address");
                }
                SigningResult res = pwallet->SignMessage(strMessage, *pkhash, signature);
                if (res != SigningResult::OK) {
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, SigningResultString(res));
                }
            } else {
                // Use quantum signing
                // For P2WSH addresses, we need to find a quantum key that matches the witness script hash
                const CQuantumKey* qkey = nullptr;
                bool found = false;
                
                if (witness_script_hash) {
                    // For P2WSH, we need to search for a key that would produce this script hash
                    // This requires checking all quantum keys in the wallet
                    if (pwallet->IsWalletFlagSet(WALLET_FLAG_DESCRIPTORS)) {
                        for (auto& spkm : pwallet->GetAllScriptPubKeyMans()) {
                            auto desc_spkm = dynamic_cast<DescriptorScriptPubKeyMan*>(spkm);
                            if (desc_spkm) {
                                // Get all quantum keys and check if any match
                                std::vector<CKeyID> keyids;
                                desc_spkm->GetQuantumKeyIDs(keyids);
                                
                                for (const auto& keyid : keyids) {
                                    const CQuantumKey* candidate_key = nullptr;
                                    if (desc_spkm->GetQuantumKey(keyid, &candidate_key) && candidate_key) {
                                        // Check if this key's P2WSH matches our address
                                        CQuantumPubKey pubkey = candidate_key->GetPubKey();
                                        CScript witness_script = quantum::CreateQuantumWitnessScript(pubkey);
                                        uint256 hash;
                                        CSHA256().Write(witness_script.data(), witness_script.size()).Finalize(hash.begin());
                                        
                                        if (hash == uint256(*witness_script_hash)) {
                                            qkey = candidate_key;
                                            found = true;
                                            break;
                                        }
                                    }
                                }
                                if (found) break;
                            }
                        }
                    }
                    
                    // Also check global keystore
                    if (!found && g_quantum_keystore) {
                        std::vector<CKeyID> keyids = g_quantum_keystore->GetAllKeyIDs();
                        for (const auto& keyid : keyids) {
                            const CQuantumKey* candidate_key = nullptr;
                            if (g_quantum_keystore->GetQuantumKey(keyid, &candidate_key) && candidate_key) {
                                // Check if this key's P2WSH matches our address
                                CQuantumPubKey pubkey = candidate_key->GetPubKey();
                                CScript witness_script = quantum::CreateQuantumWitnessScript(pubkey);
                                uint256 hash;
                                CSHA256().Write(witness_script.data(), witness_script.size()).Finalize(hash.begin());
                                
                                if (hash == uint256(*witness_script_hash)) {
                                    qkey = candidate_key;
                                    found = true;
                                    break;
                                }
                            }
                        }
                    }
                } else if (pkhash) {
                    // Legacy P2PKH quantum address support (deprecated)
                    CKeyID keyid(static_cast<uint160>(*pkhash));
                    
                    // Check descriptor SPKMs first
                    if (pwallet->IsWalletFlagSet(WALLET_FLAG_DESCRIPTORS)) {
                        for (auto& spkm : pwallet->GetAllScriptPubKeyMans()) {
                            auto desc_spkm = dynamic_cast<DescriptorScriptPubKeyMan*>(spkm);
                            if (desc_spkm && desc_spkm->GetQuantumKey(keyid, &qkey)) {
                                found = true;
                                break;
                            }
                        }
                    }
                    
                    // Fall back to global keystore
                    if (!found && g_quantum_keystore) {
                        found = g_quantum_keystore->GetQuantumKey(keyid, &qkey);
                    }
                }
                
                if (!found || !qkey) {
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Quantum key not found for this address");
                }
                
                // Create the message hash using the standard function
                uint256 message_hash = MessageHash(strMessage);
                
                // Sign the message with quantum key
                std::vector<unsigned char> vchSig;
                if (!qkey->Sign(message_hash, vchSig)) {
                    throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Failed to sign message with quantum key");
                }
                
                // Encode signature as base64
                signature = EncodeBase64(vchSig);
            }
            
            UniValue ret(UniValue::VOBJ);
            ret.pushKV("signature", signature);
            ret.pushKV("algorithm", actual_algo);
            
            return ret;
        },
    };
}

} // namespace wallet