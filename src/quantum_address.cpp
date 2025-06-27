// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <quantum_address.h>
#include <crypto/quantum_key.h>
#include <base58.h>
#include <hash.h>
#include <script/script.h>
#include <crypto/sha3.h>
#include <util/strencodings.h>

#include <vector>

namespace quantum {

// Quantum signature opcodes are now defined in script/script.h
// OP_CHECKSIG_ML_DSA = OP_NOP4
// OP_CHECKSIG_SLH_DSA = OP_NOP5
// OP_CHECKSIGVERIFY_ML_DSA = OP_NOP6
// OP_CHECKSIGVERIFY_SLH_DSA = OP_NOP7
// TODO: OP_HASH256 should be replaced with SHA3-256 for quantum resistance

uint256 QuantumHash256(const std::vector<unsigned char>& data)
{
    // Use SHA3-256 for quantum resistance
    uint256 result;
    SHA3_256 hasher;
    hasher.Write(data);
    hasher.Finalize(result);
    return result;
}

std::string EncodeQuantumAddress(const CQuantumPubKey& pubkey, QuantumAddressType addrType)
{
    if (!pubkey.IsValid()) {
        return "";
    }
    
    // Determine address type based on key type if not specified
    if (addrType == QuantumAddressType::P2QPKH_ML_DSA || 
        addrType == QuantumAddressType::P2QPKH_SLH_DSA) {
        switch (pubkey.GetType()) {
            case KeyType::ML_DSA_65:
                addrType = QuantumAddressType::P2QPKH_ML_DSA;
                break;
            case KeyType::SLH_DSA_192F:
                addrType = QuantumAddressType::P2QPKH_SLH_DSA;
                break;
            default:
                return ""; // Not a quantum key
        }
    }
    
    // Hash the public key
    uint256 hash = QuantumHash256(pubkey.GetKeyData());
    
    // Create address data: version byte + hash
    std::vector<unsigned char> addrData;
    addrData.push_back(static_cast<unsigned char>(addrType));
    addrData.insert(addrData.end(), hash.begin(), hash.end());
    
    // Encode with Base58Check
    return EncodeBase58Check(addrData);
}

bool DecodeQuantumAddress(const std::string& address, QuantumAddressType& addrType, uint256& hash)
{
    std::vector<unsigned char> addrData;
    if (!DecodeBase58Check(address, addrData, 256)) {  // Max 256 bytes for safety
        return false;
    }
    
    // Check minimum size: version byte + 32-byte hash
    if (addrData.size() != 33) {
        return false;
    }
    
    // Extract version byte
    uint8_t version = addrData[0];
    
    // Validate version byte
    switch (version) {
        case static_cast<uint8_t>(QuantumAddressType::P2QPKH_ML_DSA):
        case static_cast<uint8_t>(QuantumAddressType::P2QPKH_SLH_DSA):
        case static_cast<uint8_t>(QuantumAddressType::P2QSH):
        case static_cast<uint8_t>(QuantumAddressType::P2QWPKH_ML_DSA):
        case static_cast<uint8_t>(QuantumAddressType::P2QWPKH_SLH_DSA):
        case static_cast<uint8_t>(QuantumAddressType::P2QWSH):
            addrType = static_cast<QuantumAddressType>(version);
            break;
        default:
            return false;
    }
    
    // Extract hash
    std::copy(addrData.begin() + 1, addrData.end(), hash.begin());
    
    return true;
}

CScript CreateP2QPKHScript(const uint256& pubkeyHash, KeyType keyType)
{
    CScript script;
    
    // OP_DUP OP_HASH256 <pubkeyhash> OP_EQUALVERIFY OP_CHECKSIGQ
    script << OP_DUP;
    script << OP_HASH256;
    script << std::vector<unsigned char>(pubkeyHash.begin(), pubkeyHash.end());
    script << OP_EQUALVERIFY;
    
    // Use appropriate CHECKSIG opcode based on key type
    switch (keyType) {
        case KeyType::ML_DSA_65:
            script << OP_CHECKSIG_ML_DSA;
            break;
        case KeyType::SLH_DSA_192F:
            script << OP_CHECKSIG_SLH_DSA;
            break;
        default:
            // Fallback to regular CHECKSIG for ECDSA
            script << OP_CHECKSIG;
            break;
    }
    
    return script;
}

CScript CreateP2QSHScript(const uint256& scriptHash)
{
    CScript script;
    
    // OP_HASH256 <scripthash> OP_EQUAL
    script << OP_HASH256;
    script << std::vector<unsigned char>(scriptHash.begin(), scriptHash.end());
    script << OP_EQUAL;
    
    return script;
}

bool ExtractQuantumAddress(const CScript& script, QuantumAddressType& addrType, uint256& hash)
{
    std::vector<unsigned char> scriptData(script.begin(), script.end());
    
    // Check for P2QPKH ML-DSA pattern
    if (scriptData.size() == 37 &&
        scriptData[0] == OP_DUP &&
        scriptData[1] == OP_HASH256 &&
        scriptData[2] == 32 &&  // Push 32 bytes
        scriptData[35] == OP_EQUALVERIFY &&
        scriptData[36] == OP_CHECKSIG_ML_DSA) {
        
        addrType = QuantumAddressType::P2QPKH_ML_DSA;
        std::copy(scriptData.begin() + 3, scriptData.begin() + 35, hash.begin());
        return true;
    }
    
    // Check for P2QPKH SLH-DSA pattern
    if (scriptData.size() == 37 &&
        scriptData[0] == OP_DUP &&
        scriptData[1] == OP_HASH256 &&
        scriptData[2] == 32 &&  // Push 32 bytes
        scriptData[35] == OP_EQUALVERIFY &&
        scriptData[36] == OP_CHECKSIG_SLH_DSA) {
        
        addrType = QuantumAddressType::P2QPKH_SLH_DSA;
        std::copy(scriptData.begin() + 3, scriptData.begin() + 35, hash.begin());
        return true;
    }
    
    // Check for P2QSH pattern
    if (scriptData.size() == 35 &&
        scriptData[0] == OP_HASH256 &&
        scriptData[1] == 32 &&  // Push 32 bytes
        scriptData[34] == OP_EQUAL) {
        
        addrType = QuantumAddressType::P2QSH;
        std::copy(scriptData.begin() + 2, scriptData.begin() + 34, hash.begin());
        return true;
    }
    
    // TODO: Add witness program detection
    
    return false;
}

std::string GetQuantumAddressTypeString(QuantumAddressType type)
{
    switch (type) {
        case QuantumAddressType::P2QPKH_ML_DSA:
            return "P2QPKH-ML-DSA";
        case QuantumAddressType::P2QPKH_SLH_DSA:
            return "P2QPKH-SLH-DSA";
        case QuantumAddressType::P2QSH:
            return "P2QSH";
        case QuantumAddressType::P2QWPKH_ML_DSA:
            return "P2QWPKH-ML-DSA";
        case QuantumAddressType::P2QWPKH_SLH_DSA:
            return "P2QWPKH-SLH-DSA";
        case QuantumAddressType::P2QWSH:
            return "P2QWSH";
        default:
            return "UNKNOWN";
    }
}

KeyType GetKeyTypeForAddress(QuantumAddressType addrType)
{
    switch (addrType) {
        case QuantumAddressType::P2QPKH_ML_DSA:
        case QuantumAddressType::P2QWPKH_ML_DSA:
            return KeyType::ML_DSA_65;
            
        case QuantumAddressType::P2QPKH_SLH_DSA:
        case QuantumAddressType::P2QWPKH_SLH_DSA:
            return KeyType::SLH_DSA_192F;
            
        default:
            return KeyType::ECDSA; // Fallback
    }
}

bool IsValidQuantumAddress(const std::string& address)
{
    QuantumAddressType addrType;
    uint256 hash;
    return DecodeQuantumAddress(address, addrType, hash);
}

CScript CreateQuantumWitnessProgram(unsigned char witversion, KeyType keyType, const uint256& pubkeyHash)
{
    CScript script;
    
    // Witness version
    if (witversion == 0) {
        script << OP_0;
    } else if (witversion >= 1 && witversion <= 16) {
        script << CScript::EncodeOP_N(witversion);
    } else {
        return CScript(); // Invalid version
    }
    
    // Push 32-byte hash
    script << std::vector<unsigned char>(pubkeyHash.begin(), pubkeyHash.end());
    
    // For quantum witness programs, we might want to include key type information
    // This is a design decision that would need careful consideration
    
    return script;
}

} // namespace quantum