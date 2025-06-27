// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_QUANTUM_SCRIPTPUBKEYMAN_H
#define BITCOIN_WALLET_QUANTUM_SCRIPTPUBKEYMAN_H

#include <wallet/scriptpubkeyman.h>
#include <crypto/quantum_key.h>
#include <quantum_address.h>
#include <map>

namespace wallet {

// Import quantum types into wallet namespace for convenience
using ::quantum::CQuantumKey;
using ::quantum::CQuantumPubKey;
using ::quantum::QuantumAddressType;

/**
 * QuantumScriptPubKeyMan manages quantum signature keys for the wallet.
 * 
 * This SPKMan handles ML-DSA and SLH-DSA keys for quantum-safe signatures.
 * Unlike HD wallets, quantum keys cannot be derived hierarchically, so each
 * key must be stored individually.
 */
class QuantumScriptPubKeyMan : public ScriptPubKeyMan
{
private:
    //! Map from CKeyID to quantum private keys (using unique_ptr due to non-copyable CQuantumKey)
    std::map<CKeyID, std::unique_ptr<CQuantumKey>> m_quantum_keys GUARDED_BY(cs_key_man);
    
    //! Map from CKeyID to encrypted quantum private keys
    std::map<CKeyID, std::vector<unsigned char>> m_encrypted_keys GUARDED_BY(cs_key_man);
    
    //! Map from CKeyID to quantum public keys
    std::map<CKeyID, CQuantumPubKey> m_quantum_pubkeys GUARDED_BY(cs_key_man);
    
    //! Map from script hash to quantum address info
    std::map<CScriptID, QuantumAddressType> m_quantum_scripts GUARDED_BY(cs_key_man);
    
    //! Currently set address type for new addresses
    QuantumAddressType m_address_type GUARDED_BY(cs_key_man) = QuantumAddressType::P2QPKH_ML_DSA;
    
    //! Mutex for key operations
    mutable RecursiveMutex cs_key_man;
    
    //! Whether keys are encrypted
    bool m_encrypted GUARDED_BY(cs_key_man) = false;
    
    //! Master key for encryption
    CKeyingMaterial m_master_key GUARDED_BY(cs_key_man);
    
    //! Internal keypool
    std::set<CKeyID> m_keypool GUARDED_BY(cs_key_man);
    
    //! Used keypool keys
    std::set<CKeyID> m_used_keys GUARDED_BY(cs_key_man);

public:
    explicit QuantumScriptPubKeyMan(WalletStorage& storage) : ScriptPubKeyMan(storage) {}
    ~QuantumScriptPubKeyMan() override = default;

    // Core functionality
    util::Result<CTxDestination> GetNewDestination(const OutputType type) override;
    isminetype IsMine(const CScript& script) const override;
    
    // Encryption support
    bool CheckDecryptionKey(const CKeyingMaterial& master_key) override;
    bool Encrypt(const CKeyingMaterial& master_key, WalletBatch* batch) override;
    
    // Key pool management
    util::Result<CTxDestination> GetReservedDestination(const OutputType type, bool internal, int64_t& index) override;
    void KeepDestination(int64_t index, const OutputType& type) override;
    void ReturnDestination(int64_t index, bool internal, const CTxDestination& addr) override;
    bool TopUp(unsigned int size = 0) override;
    unsigned int GetKeyPoolSize() const override;
    
    // Signing functionality
    bool SignTransaction(CMutableTransaction& tx, const std::map<COutPoint, Coin>& coins, int sighash, std::map<int, bilingual_str>& input_errors) const override;
    SigningResult SignMessage(const std::string& message, const PKHash& pkhash, std::string& str_sig) const override;
    std::optional<common::PSBTError> FillPSBT(PartiallySignedTransaction& psbt, const PrecomputedTransactionData& txdata, std::optional<int> sighash_type = std::nullopt, bool sign = true, bool bip32derivs = false, int* n_signed = nullptr, bool finalize = true) const override;
    
    // Key management
    bool CanProvide(const CScript& script, SignatureData& sigdata) override { return CanProvideImpl(script, sigdata); }
    bool CanProvideImpl(const CScript& script, SignatureData& sigdata) const;
    bool HavePrivateKeys() const override;
    int64_t GetTimeFirstKey() const override;
    std::unique_ptr<SigningProvider> GetSolvingProvider(const CScript& script) const override;
    
    // Quantum-specific methods
    bool AddQuantumKey(std::unique_ptr<CQuantumKey> key, const CQuantumPubKey& pubkey);
    bool GetQuantumKey(const CKeyID& keyid, const CQuantumKey** key) const;
    bool GetQuantumPubKey(const CKeyID& keyid, CQuantumPubKey& pubkey) const;
    bool HaveQuantumKey(const CKeyID& keyid) const;
    std::set<CKeyID> GetQuantumKeys() const;
    
    // Address type management
    void SetQuantumAddressType(QuantumAddressType type);
    QuantumAddressType GetQuantumAddressType() const;
    
    // Serialization
    bool LoadKey(const CKeyID& keyid, std::unique_ptr<CQuantumKey> key);
    bool LoadPubKey(const CKeyID& keyid, const CQuantumPubKey& pubkey);
    bool LoadScript(const CScriptID& scriptid, QuantumAddressType type);
    
    // Persistence
    bool WriteQuantumKey(const CKeyID& keyid, const CQuantumKey& key, WalletBatch& batch);
    bool WriteQuantumPubKey(const CKeyID& keyid, const CQuantumPubKey& pubkey, WalletBatch& batch);
    bool WriteQuantumScript(const CScriptID& scriptid, QuantumAddressType type, WalletBatch& batch);
};

} // namespace wallet

#endif // BITCOIN_WALLET_QUANTUM_SCRIPTPUBKEYMAN_H