// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/quantum_signature.h>
#include <consensus/consensus.h>
#include <serialize.h>
#include <streams.h>

namespace quantum {

void QuantumSignature::Serialize(std::vector<unsigned char>& vch) const
{
    vch.clear();
    VectorWriter writer{vch, 0};
    
    // Write scheme ID
    writer << scheme_id;
    
    // Write signature length as varint and signature data
    WriteCompactSize(writer, signature.size());
    writer.write(MakeByteSpan(signature));
    
    // Write pubkey length as varint and pubkey data
    WriteCompactSize(writer, pubkey.size());
    writer.write(MakeByteSpan(pubkey));
}

bool QuantumSignature::Deserialize(const std::vector<unsigned char>& vch)
{
    try {
        DataStream stream(vch);
        
        // Read scheme ID
        uint8_t id;
        stream >> id;
        if (id > 0xFF) return false;
        scheme_id = static_cast<SignatureSchemeID>(id);
        
        // Read signature
        uint64_t sig_len = ReadCompactSize(stream);
        if (sig_len > MAX_QUANTUM_SIG_SIZE) return false;
        signature.resize(sig_len);
        stream.read(MakeWritableByteSpan(signature));
        
        // Read pubkey
        uint64_t pubkey_len = ReadCompactSize(stream);
        if (pubkey_len > MAX_QUANTUM_PUBKEY_SIZE_DYNAMIC) return false;
        pubkey.resize(pubkey_len);
        stream.read(MakeWritableByteSpan(pubkey));
        
        // Ensure we consumed all data
        if (!stream.empty()) return false;
        
        return IsValidSize();
    } catch (const std::exception&) {
        return false;
    }
}

size_t QuantumSignature::GetMaxSignatureSize(SignatureSchemeID scheme_id)
{
    switch (scheme_id) {
        case SCHEME_ECDSA:
            return MAX_ECDSA_SIG_SIZE;
        case SCHEME_ML_DSA_65:
            return MAX_ML_DSA_65_SIG_SIZE;
        case SCHEME_SLH_DSA_192F:
            return MAX_SLH_DSA_192F_SIG_SIZE;
        default:
            return MAX_QUANTUM_SIG_SIZE;
    }
}

size_t QuantumSignature::GetMaxPubKeySize(SignatureSchemeID scheme_id)
{
    switch (scheme_id) {
        case SCHEME_ECDSA:
            return MAX_ECDSA_PUBKEY_SIZE;
        case SCHEME_ML_DSA_65:
            return MAX_ML_DSA_65_PUBKEY_SIZE;
        case SCHEME_SLH_DSA_192F:
            return MAX_SLH_DSA_192F_PUBKEY_SIZE;
        default:
            return MAX_QUANTUM_PUBKEY_SIZE_DYNAMIC;
    }
}

bool QuantumSignature::IsValidSize() const
{
    // Validate sizes are within acceptable ranges
    // The exact NIST standard sizes will be enforced by liboqs during verification
    switch (scheme_id) {
        case SCHEME_ML_DSA_65:
            // ML-DSA-65: Check signature is within reasonable range and pubkey is exact
            return signature.size() <= MAX_ML_DSA_65_SIG_SIZE && 
                   signature.size() > 0 &&
                   pubkey.size() == ML_DSA_65_PUBKEY_SIZE;
        
        case SCHEME_SLH_DSA_192F:
            // SLH-DSA-192f: Both sizes should be exact per NIST standard
            return signature.size() == SLH_DSA_192F_SIG_SIZE && 
                   pubkey.size() == SLH_DSA_192F_PUBKEY_SIZE;
        
        case SCHEME_ECDSA:
            // ECDSA signatures vary in size due to DER encoding
            return signature.size() <= MAX_ECDSA_SIG_SIZE && 
                   signature.size() > 0 &&
                   pubkey.size() <= MAX_ECDSA_PUBKEY_SIZE &&
                   pubkey.size() > 0;
        
        default:
            // Unknown schemes use maximum size validation
            size_t max_sig = GetMaxSignatureSize(scheme_id);
            size_t max_pk = GetMaxPubKeySize(scheme_id);
            return signature.size() <= max_sig && pubkey.size() <= max_pk &&
                   signature.size() > 0 && pubkey.size() > 0;
    }
}

size_t QuantumSignature::GetSerializedSize() const
{
    size_t size = 1; // scheme_id
    size += GetSizeOfCompactSize(signature.size()) + signature.size();
    size += GetSizeOfCompactSize(pubkey.size()) + pubkey.size();
    return size;
}

bool ParseQuantumSignature(const std::vector<unsigned char>& data, QuantumSignature& sig_out)
{
    if (data.empty()) return false;
    
    return sig_out.Deserialize(data);
}

std::vector<unsigned char> EncodeQuantumSignature(const QuantumSignature& sig)
{
    std::vector<unsigned char> result;
    sig.Serialize(result);
    return result;
}

int64_t GetQuantumSignatureWeight(const QuantumSignature& sig)
{
    // For quantum signatures, we use a different weight calculation
    // to account for their much larger size while maintaining
    // reasonable fee proportions
    
    size_t serialized_size = sig.GetSerializedSize();
    
    // Apply different weight factors based on signature type
    switch (sig.scheme_id) {
        case SCHEME_ECDSA:
            // Standard weight calculation for ECDSA
            return serialized_size * WITNESS_SCALE_FACTOR;
            
        case SCHEME_ML_DSA_65:
            // ML-DSA signatures get a slight discount to encourage adoption
            // Weight = size * 3 instead of size * 4
            return serialized_size * 3;
            
        case SCHEME_SLH_DSA_192F:
            // SLH-DSA signatures are very large, apply larger discount
            // Weight = size * 2 
            return serialized_size * 2;
            
        default:
            // Unknown schemes use standard weight
            return serialized_size * WITNESS_SCALE_FACTOR;
    }
}

} // namespace quantum