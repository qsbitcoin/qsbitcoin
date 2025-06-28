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

// getnewquantumaddress has been removed - use getnewaddress with algorithm parameter instead
// validatequantumaddress has been removed - use validateaddress instead

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
            
            // Only count keys in descriptor wallets
            
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


} // namespace wallet