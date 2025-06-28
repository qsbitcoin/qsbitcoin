/*
 * test_liboqs_slhdsa_only.cpp
 *
 * Minimal test of only the liboqs SLH-DSA functions used in bitcoind
 * This isolates the quantum crypto library from Bitcoin code
 */

#include <iostream>
#include <vector>
#include <cstring>
#include <chrono>
#include <oqs/oqs.h>

#define TEST_ITERATIONS 5
#define MESSAGE_SIZE 32  // Bitcoin uses 32-byte hashes

void print_result(const std::string& test, bool success) {
    std::cout << test << ": " << (success ? "PASS ✓" : "FAIL ✗") << std::endl;
}

bool test_slhdsa_basic() {
    std::cout << "\n=== Test 1: Basic SLH-DSA Operations ===" << std::endl;
    
    // Initialize
    OQS_STATUS status = OQS_SUCCESS;
    OQS_SIG *sig = OQS_SIG_new("SPHINCS+-SHA2-192f-simple");
    if (!sig) {
        std::cerr << "Failed to create SLH-DSA object" << std::endl;
        return false;
    }
    
    // Print sizes
    std::cout << "Algorithm: " << sig->method_name << std::endl;
    std::cout << "Public key size: " << sig->length_public_key << " bytes" << std::endl;
    std::cout << "Secret key size: " << sig->length_secret_key << " bytes" << std::endl;
    std::cout << "Signature size: " << sig->length_signature << " bytes" << std::endl;
    
    // Allocate keys
    std::vector<uint8_t> public_key(sig->length_public_key);
    std::vector<uint8_t> secret_key(sig->length_secret_key);
    
    // Generate keypair
    auto start = std::chrono::high_resolution_clock::now();
    status = OQS_SIG_keypair(sig, public_key.data(), secret_key.data());
    auto end = std::chrono::high_resolution_clock::now();
    
    if (status != OQS_SUCCESS) {
        OQS_SIG_free(sig);
        return false;
    }
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Keypair generation: " << duration.count() << " ms" << std::endl;
    
    // Sign a message
    uint8_t message[MESSAGE_SIZE];
    memset(message, 0xAA, MESSAGE_SIZE);
    
    std::vector<uint8_t> signature(sig->length_signature);
    size_t sig_len = 0;
    
    start = std::chrono::high_resolution_clock::now();
    status = OQS_SIG_sign(sig, signature.data(), &sig_len, 
                          message, MESSAGE_SIZE, secret_key.data());
    end = std::chrono::high_resolution_clock::now();
    
    if (status != OQS_SUCCESS) {
        OQS_SIG_free(sig);
        return false;
    }
    
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    std::cout << "Signing: " << duration.count() << " ms" << std::endl;
    std::cout << "Actual signature size: " << sig_len << " bytes" << std::endl;
    
    // Verify
    start = std::chrono::high_resolution_clock::now();
    status = OQS_SIG_verify(sig, message, MESSAGE_SIZE, 
                            signature.data(), sig_len, public_key.data());
    end = std::chrono::high_resolution_clock::now();
    
    auto verify_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    std::cout << "Verification: " << verify_duration.count() << " μs" << std::endl;
    
    OQS_SIG_free(sig);
    return status == OQS_SUCCESS;
}

bool test_slhdsa_stress() {
    std::cout << "\n=== Test 2: SLH-DSA Stress Test ===" << std::endl;
    
    OQS_SIG *sig = OQS_SIG_new("SPHINCS+-SHA2-192f-simple");
    if (!sig) return false;
    
    // Generate one keypair
    std::vector<uint8_t> public_key(sig->length_public_key);
    std::vector<uint8_t> secret_key(sig->length_secret_key);
    
    if (OQS_SIG_keypair(sig, public_key.data(), secret_key.data()) != OQS_SUCCESS) {
        OQS_SIG_free(sig);
        return false;
    }
    
    // Sign multiple messages
    std::cout << "Signing " << TEST_ITERATIONS << " messages..." << std::endl;
    
    for (int i = 0; i < TEST_ITERATIONS; i++) {
        uint8_t message[MESSAGE_SIZE];
        memset(message, i, MESSAGE_SIZE);
        
        std::vector<uint8_t> signature(sig->length_signature);
        size_t sig_len = 0;
        
        if (OQS_SIG_sign(sig, signature.data(), &sig_len, 
                         message, MESSAGE_SIZE, secret_key.data()) != OQS_SUCCESS) {
            std::cerr << "Sign failed at iteration " << i << std::endl;
            OQS_SIG_free(sig);
            return false;
        }
        
        if (OQS_SIG_verify(sig, message, MESSAGE_SIZE, 
                           signature.data(), sig_len, public_key.data()) != OQS_SUCCESS) {
            std::cerr << "Verify failed at iteration " << i << std::endl;
            OQS_SIG_free(sig);
            return false;
        }
        
        std::cout << "." << std::flush;
    }
    std::cout << " Done!" << std::endl;
    
    OQS_SIG_free(sig);
    return true;
}

bool test_slhdsa_memory() {
    std::cout << "\n=== Test 3: SLH-DSA Memory Handling ===" << std::endl;
    
    // Test multiple allocations
    std::vector<OQS_SIG*> sigs;
    
    std::cout << "Creating multiple SLH-DSA contexts..." << std::endl;
    for (int i = 0; i < 3; i++) {
        OQS_SIG *sig = OQS_SIG_new("SPHINCS+-SHA2-192f-simple");
        if (!sig) {
            std::cerr << "Failed to create context " << i << std::endl;
            break;
        }
        sigs.push_back(sig);
    }
    
    std::cout << "Created " << sigs.size() << " contexts" << std::endl;
    
    // Use each context
    for (size_t i = 0; i < sigs.size(); i++) {
        std::vector<uint8_t> pub(sigs[i]->length_public_key);
        std::vector<uint8_t> sec(sigs[i]->length_secret_key);
        
        if (OQS_SIG_keypair(sigs[i], pub.data(), sec.data()) != OQS_SUCCESS) {
            std::cerr << "Keypair generation failed for context " << i << std::endl;
        }
    }
    
    // Clean up
    for (auto sig : sigs) {
        OQS_SIG_free(sig);
    }
    
    return true;
}

bool test_slhdsa_large_buffers() {
    std::cout << "\n=== Test 4: Large Buffer Handling ===" << std::endl;
    
    OQS_SIG *sig = OQS_SIG_new("SPHINCS+-SHA2-192f-simple");
    if (!sig) return false;
    
    // Test with pre-allocated large buffers
    std::cout << "Testing with oversized buffers..." << std::endl;
    
    // Allocate extra space
    std::vector<uint8_t> public_key(sig->length_public_key * 2);
    std::vector<uint8_t> secret_key(sig->length_secret_key * 2);
    std::vector<uint8_t> signature(sig->length_signature * 2);
    
    // Generate keypair
    if (OQS_SIG_keypair(sig, public_key.data(), secret_key.data()) != OQS_SUCCESS) {
        OQS_SIG_free(sig);
        return false;
    }
    
    // Sign with oversized buffer
    uint8_t message[MESSAGE_SIZE] = {0};
    size_t sig_len = 0;
    
    if (OQS_SIG_sign(sig, signature.data(), &sig_len, 
                     message, MESSAGE_SIZE, secret_key.data()) != OQS_SUCCESS) {
        OQS_SIG_free(sig);
        return false;
    }
    
    std::cout << "Signature created in oversized buffer: " << sig_len << " bytes" << std::endl;
    
    // Verify
    bool verify_ok = (OQS_SIG_verify(sig, message, MESSAGE_SIZE, 
                                     signature.data(), sig_len, public_key.data()) == OQS_SUCCESS);
    
    OQS_SIG_free(sig);
    return verify_ok;
}

bool test_slhdsa_bitcoin_scenario() {
    std::cout << "\n=== Test 5: Bitcoin-like Usage Pattern ===" << std::endl;
    
    OQS_SIG *sig = OQS_SIG_new("SPHINCS+-SHA2-192f-simple");
    if (!sig) return false;
    
    // Generate keypair
    std::vector<uint8_t> public_key(sig->length_public_key);
    std::vector<uint8_t> secret_key(sig->length_secret_key);
    
    if (OQS_SIG_keypair(sig, public_key.data(), secret_key.data()) != OQS_SUCCESS) {
        OQS_SIG_free(sig);
        return false;
    }
    
    // Simulate Bitcoin script creation
    std::cout << "Creating Bitcoin-like script with SLH-DSA signature..." << std::endl;
    
    // Bitcoin script format: [scheme_id][sig_len][signature][pubkey_len][pubkey]
    std::vector<uint8_t> script;
    script.reserve(1 + 3 + sig->length_signature + 1 + sig->length_public_key);
    
    // Add scheme ID (2 for SLH-DSA)
    script.push_back(0x02);
    
    // Sign a transaction hash
    uint8_t tx_hash[32];
    memset(tx_hash, 0xDE, 32);
    
    std::vector<uint8_t> signature(sig->length_signature);
    size_t sig_len = 0;
    
    if (OQS_SIG_sign(sig, signature.data(), &sig_len, 
                     tx_hash, 32, secret_key.data()) != OQS_SUCCESS) {
        OQS_SIG_free(sig);
        return false;
    }
    
    // Add signature length (as varint)
    if (sig_len < 253) {
        script.push_back(sig_len);
    } else {
        script.push_back(253);
        script.push_back(sig_len & 0xFF);
        script.push_back((sig_len >> 8) & 0xFF);
    }
    
    // Add signature
    script.insert(script.end(), signature.begin(), signature.begin() + sig_len);
    
    // Add pubkey length
    script.push_back(public_key.size());
    
    // Add pubkey
    script.insert(script.end(), public_key.begin(), public_key.end());
    
    std::cout << "Script size: " << script.size() << " bytes" << std::endl;
    
    // Check Bitcoin limits
    const size_t MAX_SCRIPT_ELEMENT_SIZE = 520;
    if (script.size() > MAX_SCRIPT_ELEMENT_SIZE) {
        std::cout << "WARNING: Script exceeds MAX_SCRIPT_ELEMENT_SIZE!" << std::endl;
        std::cout << "  Script size: " << script.size() << " bytes" << std::endl;
        std::cout << "  Limit: " << MAX_SCRIPT_ELEMENT_SIZE << " bytes" << std::endl;
        std::cout << "  Excess: " << (script.size() - MAX_SCRIPT_ELEMENT_SIZE) << " bytes" << std::endl;
    }
    
    OQS_SIG_free(sig);
    return true;
}

int main() {
    std::cout << "=== liboqs SLH-DSA Test Suite ===" << std::endl;
    std::cout << "Testing only the OQS functions used by bitcoind" << std::endl;
    
    // Initialize liboqs
    OQS_init();
    
    // Check if SLH-DSA is available
    if (!OQS_SIG_alg_is_enabled("SPHINCS+-SHA2-192f-simple")) {
        std::cerr << "ERROR: SLH-DSA is not enabled in liboqs" << std::endl;
        OQS_destroy();
        return 1;
    }
    
    bool all_passed = true;
    
    // Run tests
    all_passed &= test_slhdsa_basic();
    all_passed &= test_slhdsa_stress();
    all_passed &= test_slhdsa_memory();
    all_passed &= test_slhdsa_large_buffers();
    all_passed &= test_slhdsa_bitcoin_scenario();
    
    std::cout << "\n=== Summary ===" << std::endl;
    if (all_passed) {
        std::cout << "All liboqs tests PASSED ✓" << std::endl;
        std::cout << "\nKey finding: SLH-DSA signatures (" << OQS_SIG_new("SPHINCS+-SHA2-192f-simple")->length_signature 
                  << " bytes) exceed Bitcoin's MAX_SCRIPT_ELEMENT_SIZE (520 bytes)" << std::endl;
    } else {
        std::cout << "Some tests FAILED ✗" << std::endl;
    }
    
    // Cleanup
    OQS_destroy();
    
    return all_passed ? 0 : 1;
}