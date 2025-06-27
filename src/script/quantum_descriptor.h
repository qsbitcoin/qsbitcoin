// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_QUANTUM_DESCRIPTOR_H
#define BITCOIN_SCRIPT_QUANTUM_DESCRIPTOR_H

#include <script/descriptor.h>
#include <crypto/quantum_key.h>

namespace {

/** A quantum public key provider */
class QuantumPubkeyProvider : public PubkeyProvider
{
private:
    quantum::CQuantumPubKey m_pubkey;
    std::string m_str;

public:
    explicit QuantumPubkeyProvider(const quantum::CQuantumPubKey& pubkey, const std::string& str) : m_pubkey(pubkey), m_str(str) {}

    bool GetPubKey(int pos, const SigningProvider& arg, CPubKey& key, KeyOriginInfo& info, const DescriptorCache* read_cache = nullptr, DescriptorCache* write_cache = nullptr) const override
    {
        // Quantum keys are not derivable, so pos must be 0
        if (pos != 0) return false;
        
        // Convert quantum pubkey to regular pubkey format for now
        // In the future, this should return the actual quantum pubkey
        key = CPubKey(m_pubkey.GetKeyData());
        info.clear();
        return true;
    }

    bool IsRange() const override { return false; }
    size_t GetSize() const override { return 1; }
    std::string ToString(StringType type) const override { return m_str; }
    bool GetPrivKey(int pos, const SigningProvider& arg, CKey& key) const override { return false; }
};

/** Parse a quantum public key string */
std::unique_ptr<PubkeyProvider> ParseQuantumPubkey(uint32_t key_exp_index, const Span<const char>& sp, ParseScriptContext ctx, FlatSigningProvider& out, std::string& error);

} // namespace

#endif // BITCOIN_SCRIPT_QUANTUM_DESCRIPTOR_H