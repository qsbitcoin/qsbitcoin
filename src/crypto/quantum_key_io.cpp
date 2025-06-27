// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/quantum_key_io.h>
#include <crypto/quantum_key.h>
#include <hash.h>
#include <util/strencodings.h>
#include <crypto/sha256.h>

#include <sstream>
#include <iomanip>

namespace quantum {

namespace {
    // Headers for armored format
    const std::string ARMOR_HEADER_PRIVATE = "-----BEGIN QUANTUM PRIVATE KEY-----";
    const std::string ARMOR_FOOTER_PRIVATE = "-----END QUANTUM PRIVATE KEY-----";
    const std::string ARMOR_HEADER_PUBLIC = "-----BEGIN QUANTUM PUBLIC KEY-----";
    const std::string ARMOR_FOOTER_PUBLIC = "-----END QUANTUM PUBLIC KEY-----";
    
    // Key type identifiers for armored format
    std::string GetKeyTypeString(KeyType type) {
        switch (type) {
            case KeyType::ML_DSA_65: return "ML-DSA-65";
            case KeyType::SLH_DSA_192F: return "SLH-DSA-SHA2-192F";
            case KeyType::ECDSA: return "ECDSA";
            default: return "UNKNOWN";
        }
    }
    
    KeyType ParseKeyTypeString(const std::string& str) {
        if (str == "ML-DSA-65") return KeyType::ML_DSA_65;
        if (str == "SLH-DSA-SHA2-192F") return KeyType::SLH_DSA_192F;
        if (str == "ECDSA") return KeyType::ECDSA;
        throw std::runtime_error("Unknown key type: " + str);
    }
}

std::string ExportQuantumKey(const CQuantumKey& key, ExportFormat format)
{
    if (!key.IsValid()) {
        return "";
    }
    
    // Get the raw private key data
    secure_vector privkey = key.GetPrivKeyData();
    if (privkey.empty()) {
        return "";
    }
    
    // Get the public key data
    CQuantumPubKey pubkey = key.GetPubKey();
    std::vector<unsigned char> pubkeyData = pubkey.GetKeyData();
    
    // For ML-DSA-65, we need to store both private and public keys together
    // Format: [1 byte key type][2 bytes pubkey size][pubkey data][privkey data]
    std::vector<unsigned char> keyData;
    
    if (key.GetType() == KeyType::ML_DSA_65 && format != ExportFormat::ARMORED) {
        // Add a version/format byte
        keyData.push_back(0x01); // Version 1: includes public key
        
        // Add public key size (little endian)
        uint16_t pubkeySize = static_cast<uint16_t>(pubkeyData.size());
        keyData.push_back(pubkeySize & 0xFF);
        keyData.push_back((pubkeySize >> 8) & 0xFF);
        
        // Add public key data
        keyData.insert(keyData.end(), pubkeyData.begin(), pubkeyData.end());
        
        // Add private key data
        keyData.insert(keyData.end(), privkey.begin(), privkey.end());
    } else {
        // For other types or armored format, use original format
        keyData.assign(privkey.begin(), privkey.end());
    }
    
    switch (format) {
        case ExportFormat::RAW:
            // Not recommended for private keys
            return std::string(keyData.begin(), keyData.end());
            
        case ExportFormat::HEX:
            return HexStr(keyData);
            
        case ExportFormat::BASE64:
            return EncodeBase64(keyData);
            
        case ExportFormat::ARMORED: {
            // Armored format already handles public key separately
            return CreateArmoredFormat(std::vector<unsigned char>(privkey.begin(), privkey.end()), 
                                     key.GetType(), true, pubkeyData);
        }
            
        default:
            return "";
    }
}

bool ImportQuantumKey(CQuantumKey& key, const std::string& data, ExportFormat format)
{
    std::vector<unsigned char> keyData;
    std::vector<unsigned char> pubKeyData;
    KeyType keyType;
    bool isPrivate;
    
    switch (format) {
        case ExportFormat::RAW:
            keyData.assign(data.begin(), data.end());
            // We need to determine the key type from the size
            if (keyData.size() == 4032) {
                keyType = KeyType::ML_DSA_65;
            } else if (keyData.size() == 96) {
                keyType = KeyType::SLH_DSA_192F;
            } else {
                return false;
            }
            break;
            
        case ExportFormat::HEX:
            keyData = ParseHex(data);
            
            // Check if this is the new format with embedded public key
            if (keyData.size() > 3 && keyData[0] == 0x01) {
                // New format: [1 byte version][2 bytes pubkey size][pubkey][privkey]
                uint16_t pubkeySize = keyData[1] | (keyData[2] << 8);
                
                if (keyData.size() >= static_cast<size_t>(3 + pubkeySize)) {
                    // Extract public key
                    pubKeyData.assign(keyData.begin() + 3, keyData.begin() + 3 + pubkeySize);
                    
                    // Extract private key
                    std::vector<unsigned char> privKeyData(keyData.begin() + 3 + pubkeySize, keyData.end());
                    
                    // Determine key type from private key size
                    if (privKeyData.size() == 4032) {
                        keyType = KeyType::ML_DSA_65;
                        keyData = privKeyData;
                    } else {
                        return false;
                    }
                } else {
                    return false;
                }
            } else {
                // Old format: just the private key
                if (keyData.size() == 4032) {
                    keyType = KeyType::ML_DSA_65;
                } else if (keyData.size() == 96) {
                    keyType = KeyType::SLH_DSA_192F;
                } else {
                    return false;
                }
            }
            break;
            
        case ExportFormat::BASE64: {
            auto decoded = DecodeBase64(data);
            if (!decoded) {
                return false;
            }
            keyData = *decoded;
            
            // Check if this is the new format with embedded public key
            if (keyData.size() > 3 && keyData[0] == 0x01) {
                // New format: [1 byte version][2 bytes pubkey size][pubkey][privkey]
                uint16_t pubkeySize = keyData[1] | (keyData[2] << 8);
                
                if (keyData.size() >= static_cast<size_t>(3 + pubkeySize)) {
                    // Extract public key
                    pubKeyData.assign(keyData.begin() + 3, keyData.begin() + 3 + pubkeySize);
                    
                    // Extract private key
                    std::vector<unsigned char> privKeyData(keyData.begin() + 3 + pubkeySize, keyData.end());
                    
                    // Determine key type from private key size
                    if (privKeyData.size() == 4032) {
                        keyType = KeyType::ML_DSA_65;
                        keyData = privKeyData;
                    } else {
                        return false;
                    }
                } else {
                    return false;
                }
            } else {
                // Old format: just the private key
                if (keyData.size() == 4032) {
                    keyType = KeyType::ML_DSA_65;
                } else if (keyData.size() == 96) {
                    keyType = KeyType::SLH_DSA_192F;
                } else {
                    return false;
                }
            }
            break;
        }
            
        case ExportFormat::ARMORED: {
            std::vector<unsigned char> armoredPubKeyData;
            if (!ParseArmoredFormat(data, keyData, keyType, isPrivate, armoredPubKeyData)) {
                return false;
            }
            if (!isPrivate) {
                return false; // Can't import public key as private key
            }
            // If we got public key data from the armored format, use it
            if (!armoredPubKeyData.empty()) {
                // Replace the extracted pubKeyData with the one from armored format
                pubKeyData = armoredPubKeyData;
            }
            break;
        }
            
        default:
            return false;
    }
    
    // Convert to secure_vector
    secure_vector secureKeyData(keyData.begin(), keyData.end());
    
    // Extract the public key from the private key data if we don't already have it
    if (pubKeyData.empty()) {
        switch (keyType) {
        case KeyType::ML_DSA_65:
            // For ML-DSA-65, we cannot extract the public key from the secret key directly
            // The secret key format is: rho || key || tr || s1 || s2 || t0
            // The public key format is: rho || t1
            // Since t1 is not stored in the secret key, we need to regenerate the full key pair
            // For now, we'll create an empty public key and let the key regenerate it
            break;
            
        case KeyType::SLH_DSA_192F:
            // For SPHINCS+, public key is the last 48 bytes of the 96-byte secret key
            if (secureKeyData.size() == 96) {
                pubKeyData.assign(secureKeyData.end() - 48, secureKeyData.end());
            }
            break;
            
        default:
            // For other types, create empty public key
            break;
        }
    }
    
    // Create public key with the extracted data
    CQuantumPubKey pubKey(keyType, pubKeyData);
    
    // Load the key
    return key.Load(secureKeyData, pubKey);
}

std::string ExportQuantumPubKey(const CQuantumPubKey& pubkey, ExportFormat format)
{
    if (!pubkey.IsValid()) {
        return "";
    }
    
    const std::vector<unsigned char>& keyData = pubkey.GetKeyData();
    
    switch (format) {
        case ExportFormat::RAW:
            return std::string(keyData.begin(), keyData.end());
            
        case ExportFormat::HEX:
            return HexStr(keyData);
            
        case ExportFormat::BASE64:
            return EncodeBase64(keyData);
            
        case ExportFormat::ARMORED:
            return CreateArmoredFormat(keyData, pubkey.GetType(), false);
            
        default:
            return "";
    }
}

bool ImportQuantumPubKey(CQuantumPubKey& pubkey, const std::string& data, ExportFormat format)
{
    std::vector<unsigned char> keyData;
    KeyType keyType;
    bool isPrivate;
    
    switch (format) {
        case ExportFormat::RAW:
            keyData.assign(data.begin(), data.end());
            // Determine key type from size
            if (keyData.size() == 1952) {
                keyType = KeyType::ML_DSA_65;
            } else if (keyData.size() == 48) {
                keyType = KeyType::SLH_DSA_192F;
            } else if (keyData.size() == 33 || keyData.size() == 65) {
                keyType = KeyType::ECDSA;
            } else {
                return false;
            }
            break;
            
        case ExportFormat::HEX:
            keyData = ParseHex(data);
            if (keyData.size() == 1952) {
                keyType = KeyType::ML_DSA_65;
            } else if (keyData.size() == 48) {
                keyType = KeyType::SLH_DSA_192F;
            } else if (keyData.size() == 33 || keyData.size() == 65) {
                keyType = KeyType::ECDSA;
            } else {
                return false;
            }
            break;
            
        case ExportFormat::BASE64: {
            auto decoded = DecodeBase64(data);
            if (!decoded) {
                return false;
            }
            keyData = *decoded;
            if (keyData.size() == 1952) {
                keyType = KeyType::ML_DSA_65;
            } else if (keyData.size() == 48) {
                keyType = KeyType::SLH_DSA_192F;
            } else if (keyData.size() == 33 || keyData.size() == 65) {
                keyType = KeyType::ECDSA;
            } else {
                return false;
            }
            break;
        }
            
        case ExportFormat::ARMORED: {
            std::vector<unsigned char> ignoredPubKeyData;
            if (!ParseArmoredFormat(data, keyData, keyType, isPrivate, ignoredPubKeyData)) {
                return false;
            }
            if (isPrivate) {
                return false; // Can't import private key as public key
            }
            break;
        }
            
        default:
            return false;
    }
    
    pubkey = CQuantumPubKey(keyType, keyData);
    return pubkey.IsValid();
}

std::string CreateArmoredFormat(const std::vector<unsigned char>& keyData,
                               KeyType keyType, bool isPrivate,
                               const std::vector<unsigned char>& pubKeyData)
{
    std::stringstream ss;
    
    // Header
    ss << (isPrivate ? ARMOR_HEADER_PRIVATE : ARMOR_HEADER_PUBLIC) << "\n";
    
    // Key type
    ss << "Type: " << GetKeyTypeString(keyType) << "\n";
    
    // For private keys, include public key data if provided
    if (isPrivate && !pubKeyData.empty()) {
        ss << "PublicKey: " << EncodeBase64(pubKeyData) << "\n";
    }
    
    // Calculate checksum (SHA256 truncated to 4 bytes)
    uint256 hash = Hash(keyData);
    uint32_t checksum = ReadLE32(hash.begin());
    ss << "Checksum: " << HexStr(std::span<unsigned char>((unsigned char*)&checksum, 4)) << "\n";
    ss << "\n";
    
    // Base64 encoded data (wrapped at 64 characters)
    std::string base64 = EncodeBase64(keyData);
    for (size_t i = 0; i < base64.length(); i += 64) {
        ss << base64.substr(i, 64) << "\n";
    }
    
    // Footer
    ss << (isPrivate ? ARMOR_FOOTER_PRIVATE : ARMOR_FOOTER_PUBLIC) << "\n";
    
    return ss.str();
}

bool ParseArmoredFormat(const std::string& armored,
                       std::vector<unsigned char>& keyData,
                       KeyType& keyType,
                       bool& isPrivate,
                       std::vector<unsigned char>& pubKeyData)
{
    std::istringstream ss(armored);
    std::string line;
    
    // Check header
    if (!std::getline(ss, line)) return false;
    
    if (line == ARMOR_HEADER_PRIVATE) {
        isPrivate = true;
    } else if (line == ARMOR_HEADER_PUBLIC) {
        isPrivate = false;
    } else {
        return false;
    }
    
    // Parse metadata
    std::string typeStr;
    std::string checksumStr;
    std::string publicKeyStr;
    bool foundType = false;
    bool foundChecksum = false;
    
    while (std::getline(ss, line) && !line.empty()) {
        if (line.substr(0, 6) == "Type: ") {
            typeStr = line.substr(6);
            foundType = true;
        } else if (line.substr(0, 10) == "Checksum: ") {
            checksumStr = line.substr(10);
            foundChecksum = true;
        } else if (line.substr(0, 11) == "PublicKey: ") {
            publicKeyStr = line.substr(11);
        }
    }
    
    if (!foundType || !foundChecksum) {
        return false;
    }
    
    try {
        keyType = ParseKeyTypeString(typeStr);
    } catch (...) {
        return false;
    }
    
    // Read base64 data
    std::string base64Data;
    while (std::getline(ss, line)) {
        if (line == ARMOR_FOOTER_PRIVATE || line == ARMOR_FOOTER_PUBLIC) {
            break;
        }
        base64Data += line;
    }
    
    // Decode base64
    auto decoded = DecodeBase64(base64Data);
    if (!decoded || decoded->empty()) {
        return false;
    }
    keyData = *decoded;
    
    // Verify checksum
    uint256 hash = Hash(keyData);
    uint32_t expectedChecksum = ReadLE32(hash.begin());
    
    std::vector<unsigned char> checksumBytes = ParseHex(checksumStr);
    if (checksumBytes.size() != 4) {
        return false;
    }
    
    uint32_t actualChecksum = ReadLE32(checksumBytes.data());
    if (expectedChecksum != actualChecksum) {
        return false;
    }
    
    // Decode public key if present
    if (!publicKeyStr.empty()) {
        auto decodedPubKey = DecodeBase64(publicKeyStr);
        if (decodedPubKey) {
            pubKeyData = *decodedPubKey;
        }
    }
    
    return true;
}

std::string GetQuantumKeyFingerprint(const CQuantumPubKey& pubkey)
{
    if (!pubkey.IsValid()) {
        return "";
    }
    
    // Get key ID (hash of public key)
    CKeyID keyId = pubkey.GetID();
    
    // Format as readable fingerprint
    std::stringstream ss;
    ss << GetKeyTypeString(pubkey.GetType()) << ":";
    
    // Convert to hex and add colons every 4 characters
    std::string hex = HexStr(keyId);
    for (size_t i = 0; i < hex.length() && i < 32; i += 4) {
        if (i > 0) ss << ":";
        ss << hex.substr(i, 4);
    }
    
    return ss.str();
}

} // namespace quantum