// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <common/signmessage.h>
#include <hash.h>
#include <key.h>
#include <key_io.h>
#include <pubkey.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <crypto/quantum_key.h>
#include <script/quantum_witness.h>
#include <crypto/sha256.h>

#include <cassert>
#include <optional>
#include <string>
#include <variant>
#include <vector>

/**
 * Text used to signify that a signed message follows and to prevent
 * inadvertently signing a transaction.
 */
const std::string MESSAGE_MAGIC = "Bitcoin Signed Message:\n";

MessageVerificationResult MessageVerify(
    const std::string& address,
    const std::string& signature,
    const std::string& message)
{
    CTxDestination destination = DecodeDestination(address);
    if (!IsValidDestination(destination)) {
        return MessageVerificationResult::ERR_INVALID_ADDRESS;
    }

    auto signature_bytes = DecodeBase64(signature);
    if (!signature_bytes) {
        return MessageVerificationResult::ERR_MALFORMED_SIGNATURE;
    }

    // Handle P2PKH addresses (ECDSA)
    if (auto pkhash = std::get_if<PKHash>(&destination)) {
        CPubKey pubkey;
        if (!pubkey.RecoverCompact(MessageHash(message), *signature_bytes)) {
            return MessageVerificationResult::ERR_PUBKEY_NOT_RECOVERED;
        }

        if (!(PKHash(pubkey) == *pkhash)) {
            return MessageVerificationResult::ERR_NOT_SIGNED;
        }

        return MessageVerificationResult::OK;
    }
    
    // Handle P2WSH addresses (potential quantum)
    if (auto witness_script_hash = std::get_if<WitnessV0ScriptHash>(&destination)) {
        // Check if this is a quantum signature with embedded public key
        if (signature_bytes->size() < 7) { // Minimum: version(1) + type(1) + pubkey_size(2) + sig_size(2) + data(1+)
            return MessageVerificationResult::ERR_MALFORMED_SIGNATURE;
        }
        
        size_t pos = 0;
        
        // Check version byte
        if ((*signature_bytes)[pos++] != 0x01) {
            // Not a quantum signature format we recognize
            return MessageVerificationResult::ERR_NOT_SIGNED;
        }
        
        // Get key type
        auto key_type = static_cast<quantum::KeyType>((*signature_bytes)[pos++]);
        if (key_type != quantum::KeyType::ML_DSA_65 && key_type != quantum::KeyType::SLH_DSA_192F) {
            return MessageVerificationResult::ERR_MALFORMED_SIGNATURE;
        }
        
        // Get pubkey size
        if (pos + 2 > signature_bytes->size()) {
            return MessageVerificationResult::ERR_MALFORMED_SIGNATURE;
        }
        size_t pubkey_size = ((*signature_bytes)[pos] << 8) | (*signature_bytes)[pos + 1];
        pos += 2;
        
        // Extract public key
        if (pos + pubkey_size + 2 > signature_bytes->size()) {
            return MessageVerificationResult::ERR_MALFORMED_SIGNATURE;
        }
        std::vector<unsigned char> pubkey_data(signature_bytes->begin() + pos, 
                                                signature_bytes->begin() + pos + pubkey_size);
        pos += pubkey_size;
        
        // Get signature size
        size_t sig_size = ((*signature_bytes)[pos] << 8) | (*signature_bytes)[pos + 1];
        pos += 2;
        
        // Extract signature
        if (pos + sig_size != signature_bytes->size()) {
            return MessageVerificationResult::ERR_MALFORMED_SIGNATURE;
        }
        std::vector<unsigned char> sig_data(signature_bytes->begin() + pos, 
                                             signature_bytes->end());
        
        // Create quantum public key
        quantum::CQuantumPubKey quantum_pubkey(key_type, pubkey_data);
        if (!quantum_pubkey.IsValid()) {
            return MessageVerificationResult::ERR_MALFORMED_SIGNATURE;
        }
        
        // Verify the signature
        uint256 message_hash = MessageHash(message);
        if (!quantum::CQuantumKey::Verify(message_hash, sig_data, quantum_pubkey)) {
            return MessageVerificationResult::ERR_NOT_SIGNED;
        }
        
        // Check if the public key matches the address
        CScript witness_script = quantum::CreateQuantumWitnessScript(quantum_pubkey);
        uint256 hash;
        CSHA256().Write(witness_script.data(), witness_script.size()).Finalize(hash.begin());
        
        if (hash != uint256(*witness_script_hash)) {
            return MessageVerificationResult::ERR_NOT_SIGNED;
        }
        
        return MessageVerificationResult::OK;
    }

    return MessageVerificationResult::ERR_ADDRESS_NO_KEY;
}

bool MessageSign(
    const CKey& privkey,
    const std::string& message,
    std::string& signature)
{
    std::vector<unsigned char> signature_bytes;

    if (!privkey.SignCompact(MessageHash(message), signature_bytes)) {
        return false;
    }

    signature = EncodeBase64(signature_bytes);

    return true;
}

uint256 MessageHash(const std::string& message)
{
    HashWriter hasher{};
    hasher << MESSAGE_MAGIC << message;

    return hasher.GetHash();
}

std::string SigningResultString(const SigningResult res)
{
    switch (res) {
        case SigningResult::OK:
            return "No error";
        case SigningResult::PRIVATE_KEY_NOT_AVAILABLE:
            return "Private key not available";
        case SigningResult::SIGNING_FAILED:
            return "Sign failed";
        // no default case, so the compiler can warn about missing cases
    }
    assert(false);
}
