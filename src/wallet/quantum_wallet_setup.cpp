// Copyright (c) 2024-present The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/quantum_wallet_setup.h>
#include <wallet/wallet.h>
#include <wallet/walletdb.h>
#include <wallet/scriptpubkeyman.h>
#include <script/descriptor.h>
#include <crypto/quantum_key.h>
#include <util/time.h>
#include <util/strencodings.h>
#include <logging.h>

namespace wallet {

bool SetupQuantumDescriptor(CWallet& wallet, WalletBatch& batch, quantum::SignatureSchemeID scheme_id, bool internal)
{
    LOCK(wallet.cs_wallet);
    
    // Determine the quantum key type
    ::quantum::KeyType key_type;
    std::string desc_func = "qpkh";
    
    if (scheme_id == quantum::SCHEME_ML_DSA_65) {
        key_type = quantum::KeyType::ML_DSA_65;
    } else if (scheme_id == quantum::SCHEME_SLH_DSA_192F) {
        key_type = quantum::KeyType::SLH_DSA_192F;
    } else {
        return false;
    }
    
    // Generate a quantum key for this descriptor
    auto qkey = std::make_unique<quantum::CQuantumKey>();
    qkey->MakeNewKey(key_type);
    if (!qkey->IsValid()) {
        LogPrintf("Failed to generate quantum key for descriptor\n");
        return false;
    }
    
    quantum::CQuantumPubKey qpubkey = qkey->GetPubKey();
    std::vector<unsigned char> pubkey_data = qpubkey.GetKeyData();
    std::string pubkey_hex = HexStr(pubkey_data);
    
    // Build quantum descriptor string
    // Format: qpkh(quantum:scheme:pubkey)
    std::string scheme_str = (key_type == ::quantum::KeyType::ML_DSA_65) ? "ml-dsa" : "slh-dsa";
    std::string desc_str = desc_func + "(quantum:" + scheme_str + ":" + pubkey_hex + ")";
    
    LogPrintf("Creating quantum descriptor: %s\n", desc_str);
    
    // Parse the descriptor
    FlatSigningProvider keys;
    std::string error;
    auto descs = Parse(desc_str, keys, error, false);
    if (descs.empty() || !descs[0]) {
        LogPrintf("Failed to parse quantum descriptor: %s\n", error);
        return false;
    }
    
    // Create wallet descriptor
    int64_t creation_time = GetTime();
    WalletDescriptor w_desc(std::move(descs[0]), creation_time, 0, 0, 0);
    
    // Create and setup the DescriptorScriptPubKeyMan
    auto spk_manager = std::unique_ptr<DescriptorScriptPubKeyMan>(new DescriptorScriptPubKeyMan(wallet, wallet.m_keypool_size));
    
    // Handle encryption if wallet is encrypted
    if (wallet.IsCrypted()) {
        if (wallet.IsLocked()) {
            throw std::runtime_error("Wallet is locked, cannot setup quantum descriptors");
        }
        if (!spk_manager->CheckDecryptionKey(wallet.vMasterKey) && !spk_manager->Encrypt(wallet.vMasterKey, &batch)) {
            throw std::runtime_error("Could not encrypt quantum descriptors");
        }
    }
    
    // Setup the descriptor first
    if (!spk_manager->SetupQuantumDescriptor(batch, w_desc)) {
        throw std::runtime_error("Setting up quantum descriptor failed");
    }
    
    // Then add the quantum key
    CKeyID keyid = qpubkey.GetID();
    if (!spk_manager->AddQuantumKey(keyid, std::move(qkey))) {
        LogPrintf("Failed to add quantum key to descriptor SPKM\n");
        return false;
    }
    
    // The descriptor will handle script generation when needed
    
    // Add to wallet
    uint256 id = spk_manager->GetID();
    wallet.AddScriptPubKeyMan(id, std::move(spk_manager));
    // Note: We don't call AddActiveScriptPubKeyManWithDb because quantum types
    // are not standard OutputTypes for active management
    
    LogPrintf("Successfully created quantum descriptor SPKM with ID: %s\n", id.ToString());
    
    return true;
}

void SetupQuantumDescriptors(CWallet& wallet, WalletBatch& batch)
{
    LogPrintf("SetupQuantumDescriptors: Starting quantum descriptor setup\n");
    
    // Create quantum descriptors for both ML-DSA and SLH-DSA
    // Only create receive descriptors (internal=false) for now
    if (!SetupQuantumDescriptor(wallet, batch, quantum::SCHEME_ML_DSA_65, false)) {
        throw std::runtime_error("Failed to setup ML-DSA quantum descriptor");
    }
    LogPrintf("SetupQuantumDescriptors: ML-DSA descriptor created\n");
    
    if (!SetupQuantumDescriptor(wallet, batch, quantum::SCHEME_SLH_DSA_192F, false)) {
        throw std::runtime_error("Failed to setup SLH-DSA quantum descriptor");
    }
    LogPrintf("SetupQuantumDescriptors: SLH-DSA descriptor created\n");
    
    LogPrintf("Quantum descriptors setup complete\n");
}

} // namespace wallet