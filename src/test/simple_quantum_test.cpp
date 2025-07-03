// Simple test to isolate quantum key generation issue

#include <crypto/quantum_key.h>
#include <script/descriptor.h>
#include <script/signingprovider.h>
#include <util/strencodings.h>
#include <wallet/quantum_wallet_setup.h>
#include <wallet/walletdb.h>
#include <wallet/wallet.h>
#include <wallet/scriptpubkeyman.h>
#include <iostream>

int main() {
    std::cout << "Testing quantum key generation...\n";
    
    // Test 1: Basic quantum key generation
    try {
        quantum::CQuantumKey qKey;
        qKey.MakeNewKey(quantum::KeyType::ML_DSA_65);
        if (qKey.IsValid()) {
            std::cout << "✓ ML-DSA key generation successful\n";
        } else {
            std::cout << "✗ ML-DSA key generation failed\n";
            return 1;
        }
        
        auto pubkey = qKey.GetPubKey();
        std::cout << "  Pubkey size: " << pubkey.GetKeyData().size() << " bytes\n";
    } catch (const std::exception& e) {
        std::cout << "✗ Exception during key generation: " << e.what() << "\n";
        return 1;
    }
    
    // Test 2: Descriptor creation
    try {
        quantum::CQuantumKey qKey;
        qKey.MakeNewKey(quantum::KeyType::ML_DSA_65);
        auto pubkey = qKey.GetPubKey();
        
        std::string pubkey_hex = HexStr(pubkey.GetKeyData());
        std::string desc_str = "qpkh(quantum:ml-dsa:" + pubkey_hex + ")";
        
        std::cout << "✓ Descriptor string created (length: " << desc_str.length() << ")\n";
        
        // Parse descriptor
        FlatSigningProvider keys;
        std::string error;
        auto parsed = Parse(desc_str, keys, error, false);
        
        if (!parsed.empty() && parsed[0]) {
            std::cout << "✓ Descriptor parsed successfully\n";
        } else {
            std::cout << "✗ Descriptor parsing failed: " << error << "\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cout << "✗ Exception during descriptor creation: " << e.what() << "\n";
        return 1;
    }
    
    // Test 3: Multiple key generation (stress test)
    try {
        for (int i = 0; i < 5; i++) {
            quantum::CQuantumKey qKey;
            qKey.MakeNewKey(quantum::KeyType::ML_DSA_65);
            if (!qKey.IsValid()) {
                std::cout << "✗ Key generation failed at iteration " << i << "\n";
                return 1;
            }
        }
        std::cout << "✓ Multiple key generation successful (5 keys)\n";
    } catch (const std::exception& e) {
        std::cout << "✗ Exception during multiple key generation: " << e.what() << "\n";
        return 1;
    }
    
    std::cout << "\nAll tests passed!\n";
    return 0;
}