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

bool SetupQuantumDescriptor(CWallet& wallet, WalletBatch& batch, quantum::SignatureSchemeID scheme_id, bool internal, std::unique_ptr<quantum::CQuantumKey> provided_key)
{
    LOCK(wallet.cs_wallet);
    LogPrintf("[QUANTUM] SetupQuantumDescriptor called with scheme_id=%d, internal=%d, provided_key=%s\n", 
              (int)scheme_id, internal, provided_key ? "yes" : "no");
    
    // Determine the quantum key type
    ::quantum::KeyType key_type;
    std::string desc_func = "qpkh";
    
    if (scheme_id == quantum::SCHEME_ML_DSA_65) {
        key_type = quantum::KeyType::ML_DSA_65;
        LogPrintf("[QUANTUM] Using ML-DSA-65 key type\n");
    } else if (scheme_id == quantum::SCHEME_SLH_DSA_192F) {
        key_type = quantum::KeyType::SLH_DSA_192F;
        LogPrintf("[QUANTUM] Using SLH-DSA-192f key type\n");
    } else {
        LogPrintf("[QUANTUM] Unknown scheme_id: %d\n", (int)scheme_id);
        return false;
    }
    
    // Use provided key or generate a new one
    std::unique_ptr<quantum::CQuantumKey> qkey;
    if (provided_key) {
        qkey = std::move(provided_key);
        LogPrintf("[QUANTUM] Using provided quantum key\n");
    } else {
        qkey = std::make_unique<quantum::CQuantumKey>();
        qkey->MakeNewKey(key_type);
        if (!qkey->IsValid()) {
            LogPrintf("[QUANTUM] Failed to generate quantum key for descriptor\n");
            return false;
        }
        LogPrintf("[QUANTUM] Generated quantum key successfully\n");
    }
    
    quantum::CQuantumPubKey qpubkey = qkey->GetPubKey();
    std::vector<unsigned char> pubkey_data = qpubkey.GetKeyData();
    std::string pubkey_hex = HexStr(pubkey_data);
    LogPrintf("[QUANTUM] Pubkey data size: %d bytes\n", pubkey_data.size());
    
    // Build quantum descriptor string
    // Format: qpkh(quantum:scheme:pubkey)
    std::string scheme_str = (key_type == ::quantum::KeyType::ML_DSA_65) ? "ml-dsa" : "slh-dsa";
    std::string desc_str = desc_func + "(quantum:" + scheme_str + ":" + pubkey_hex + ")";
    
    LogPrintf("[QUANTUM] Creating quantum descriptor: %s...\n", desc_str.substr(0, 50));
    
    // Parse the descriptor
    FlatSigningProvider keys;
    std::string error;
    auto descs = Parse(desc_str, keys, error, false);
    if (descs.empty() || !descs[0]) {
        LogPrintf("[QUANTUM] Failed to parse quantum descriptor: %s\n", error);
        return false;
    }
    LogPrintf("[QUANTUM] Successfully parsed quantum descriptor\n");
    
    // Create wallet descriptor
    int64_t creation_time = GetTime();
    WalletDescriptor w_desc(std::move(descs[0]), creation_time, 0, 0, 0);
    LogPrintf("[QUANTUM] Created wallet descriptor\n");
    
    // Create and setup the DescriptorScriptPubKeyMan
    auto spk_manager = std::unique_ptr<DescriptorScriptPubKeyMan>(new DescriptorScriptPubKeyMan(wallet, wallet.m_keypool_size));
    LogPrintf("[QUANTUM] Created DescriptorScriptPubKeyMan\n");
    
    // Handle encryption if wallet is encrypted
    if (wallet.IsCrypted()) {
        LogPrintf("[QUANTUM] Wallet is encrypted, handling encryption\n");
        if (wallet.IsLocked()) {
            LogPrintf("[QUANTUM] Wallet is locked!\n");
            throw std::runtime_error("Wallet is locked, cannot setup quantum descriptors");
        }
        if (!spk_manager->CheckDecryptionKey(wallet.vMasterKey) && !spk_manager->Encrypt(wallet.vMasterKey, &batch)) {
            LogPrintf("[QUANTUM] Failed to encrypt quantum descriptors\n");
            throw std::runtime_error("Could not encrypt quantum descriptors");
        }
    }
    
    // Setup the descriptor first
    LogPrintf("[QUANTUM] Setting up quantum descriptor in SPKM\n");
    if (!spk_manager->SetupQuantumDescriptor(batch, w_desc)) {
        LogPrintf("[QUANTUM] SetupQuantumDescriptor failed\n");
        throw std::runtime_error("Setting up quantum descriptor failed");
    }
    
    // Then add the quantum key
    CKeyID keyid = qpubkey.GetID();
    LogPrintf("[QUANTUM] Adding quantum key with keyid=%s to SPKM\n", keyid.ToString());
    if (!spk_manager->AddQuantumKey(keyid, std::move(qkey))) {
        LogPrintf("[QUANTUM] Failed to add quantum key to descriptor SPKM\n");
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
    if (!SetupQuantumDescriptor(wallet, batch, quantum::SCHEME_ML_DSA_65, false, nullptr)) {
        throw std::runtime_error("Failed to setup ML-DSA quantum descriptor");
    }
    LogPrintf("SetupQuantumDescriptors: ML-DSA descriptor created\n");
    
    if (!SetupQuantumDescriptor(wallet, batch, quantum::SCHEME_SLH_DSA_192F, false, nullptr)) {
        throw std::runtime_error("Failed to setup SLH-DSA quantum descriptor");
    }
    LogPrintf("SetupQuantumDescriptors: SLH-DSA descriptor created\n");
    
    LogPrintf("Quantum descriptors setup complete\n");
}

} // namespace wallet