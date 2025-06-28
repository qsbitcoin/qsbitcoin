/*
 * test_slhdsa_crash.cpp
 * 
 * Minimal test to reproduce the SLH-DSA crash in bitcoind
 * Tests large signature allocation and memory handling
 */

#include <iostream>
#include <vector>
#include <cstring>
#include <oqs/oqs.h>

#define SLH_DSA_SIG_SIZE 49856  // Expected signature size for SLH-DSA-192f

void test_memory_allocation() {
    std::cout << "Test 1: Large memory allocation patterns" << std::endl;
    
    // Simulate what happens in Bitcoin when creating SLH-DSA signatures
    for (int i = 0; i < 10; i++) {
        std::cout << "Iteration " << i << ": ";
        
        // Allocate large buffer like Bitcoin does
        std::vector<unsigned char> sig_buffer;
        sig_buffer.reserve(SLH_DSA_SIG_SIZE);
        sig_buffer.resize(SLH_DSA_SIG_SIZE);
        
        // Fill with pattern
        memset(sig_buffer.data(), 0xAA + i, sig_buffer.size());
        
        // Simulate reallocation (like when Bitcoin adjusts signature size)
        sig_buffer.resize(SLH_DSA_SIG_SIZE - 100);
        sig_buffer.resize(SLH_DSA_SIG_SIZE);
        
        std::cout << "OK (" << sig_buffer.size() << " bytes)" << std::endl;
    }
}

void test_oqs_signature_creation() {
    std::cout << "\nTest 2: OQS SLH-DSA signature creation" << std::endl;
    
    OQS_SIG *sig = OQS_SIG_new("SPHINCS+-SHA2-192f-simple");
    if (!sig) {
        std::cerr << "ERROR: Failed to create SLH-DSA object" << std::endl;
        return;
    }
    
    std::cout << "SLH-DSA object created:" << std::endl;
    std::cout << "  Public key size: " << sig->length_public_key << std::endl;
    std::cout << "  Secret key size: " << sig->length_secret_key << std::endl;
    std::cout << "  Signature size: " << sig->length_signature << std::endl;
    
    // Allocate keys
    std::vector<uint8_t> public_key(sig->length_public_key);
    std::vector<uint8_t> secret_key(sig->length_secret_key);
    
    // Generate keypair
    std::cout << "Generating keypair..." << std::endl;
    OQS_STATUS rc = OQS_SIG_keypair(sig, public_key.data(), secret_key.data());
    if (rc != OQS_SUCCESS) {
        std::cerr << "ERROR: Keypair generation failed" << std::endl;
        OQS_SIG_free(sig);
        return;
    }
    std::cout << "Keypair generated successfully" << std::endl;
    
    // Test multiple signature creations
    for (int i = 0; i < 5; i++) {
        std::cout << "Creating signature " << i << "..." << std::endl;
        
        // Message (32 bytes like Bitcoin hash)
        uint8_t message[32];
        memset(message, 0xBB + i, 32);
        
        // Allocate signature buffer
        std::vector<uint8_t> signature(sig->length_signature);
        size_t sig_len = 0;
        
        // Sign
        rc = OQS_SIG_sign(sig, signature.data(), &sig_len, 
                          message, 32, secret_key.data());
        
        if (rc != OQS_SUCCESS) {
            std::cerr << "ERROR: Signing failed at iteration " << i << std::endl;
            break;
        }
        
        std::cout << "  Signature created: " << sig_len << " bytes" << std::endl;
        
        // Verify
        rc = OQS_SIG_verify(sig, message, 32, 
                            signature.data(), sig_len, public_key.data());
        
        if (rc != OQS_SUCCESS) {
            std::cerr << "ERROR: Verification failed at iteration " << i << std::endl;
            break;
        }
        
        std::cout << "  Signature verified OK" << std::endl;
    }
    
    OQS_SIG_free(sig);
}

void test_concurrent_operations() {
    std::cout << "\nTest 3: Concurrent SLH-DSA operations" << std::endl;
    
    // Create multiple SLH-DSA contexts
    std::vector<OQS_SIG*> sigs;
    
    for (int i = 0; i < 3; i++) {
        OQS_SIG *sig = OQS_SIG_new("SPHINCS+-SHA2-192f-simple");
        if (!sig) {
            std::cerr << "ERROR: Failed to create SLH-DSA object " << i << std::endl;
            break;
        }
        sigs.push_back(sig);
        std::cout << "Created context " << i << std::endl;
    }
    
    // Use all contexts
    for (size_t i = 0; i < sigs.size(); i++) {
        std::vector<uint8_t> pub(sigs[i]->length_public_key);
        std::vector<uint8_t> sec(sigs[i]->length_secret_key);
        
        OQS_STATUS rc = OQS_SIG_keypair(sigs[i], pub.data(), sec.data());
        std::cout << "Context " << i << " keypair: " 
                  << (rc == OQS_SUCCESS ? "OK" : "FAILED") << std::endl;
    }
    
    // Clean up
    for (auto sig : sigs) {
        OQS_SIG_free(sig);
    }
}

void test_edge_cases() {
    std::cout << "\nTest 4: Edge cases that might cause crashes" << std::endl;
    
    OQS_SIG *sig = OQS_SIG_new("SPHINCS+-SHA2-192f-simple");
    if (!sig) {
        std::cerr << "ERROR: Failed to create SLH-DSA object" << std::endl;
        return;
    }
    
    // Generate valid keypair
    std::vector<uint8_t> public_key(sig->length_public_key);
    std::vector<uint8_t> secret_key(sig->length_secret_key);
    OQS_SIG_keypair(sig, public_key.data(), secret_key.data());
    
    // Test 1: Wrong buffer sizes
    std::cout << "Testing wrong buffer sizes..." << std::endl;
    {
        std::vector<uint8_t> small_sig(100); // Too small
        size_t sig_len = 0;
        uint8_t message[32] = {0};
        
        // This should handle gracefully
        OQS_STATUS rc = OQS_SIG_sign(sig, small_sig.data(), &sig_len,
                                      message, 32, secret_key.data());
        std::cout << "  Small buffer: " << (rc != OQS_SUCCESS ? "Correctly failed" : "Unexpectedly succeeded") << std::endl;
    }
    
    // Test 2: NULL pointers
    std::cout << "Testing NULL pointers..." << std::endl;
    {
        size_t sig_len = 0;
        uint8_t message[32] = {0};
        
        // These should not crash
        OQS_STATUS rc = OQS_SIG_sign(sig, nullptr, &sig_len,
                                      message, 32, secret_key.data());
        std::cout << "  NULL signature buffer: handled" << std::endl;
    }
    
    // Test 3: Very large allocations
    std::cout << "Testing very large allocations..." << std::endl;
    {
        try {
            // Try to allocate multiple large buffers
            std::vector<std::vector<uint8_t>> buffers;
            for (int i = 0; i < 100; i++) {
                buffers.emplace_back(SLH_DSA_SIG_SIZE);
            }
            std::cout << "  Allocated " << buffers.size() << " x " << SLH_DSA_SIG_SIZE << " bytes" << std::endl;
        } catch (const std::bad_alloc& e) {
            std::cout << "  Allocation failed (expected): " << e.what() << std::endl;
        }
    }
    
    OQS_SIG_free(sig);
}

int main() {
    std::cout << "=== SLH-DSA Crash Reproduction Test ===" << std::endl;
    std::cout << "Testing scenarios that might cause bitcoind to crash\n" << std::endl;
    
    // Initialize liboqs
    OQS_init();
    
    try {
        test_memory_allocation();
        test_oqs_signature_creation();
        test_concurrent_operations();
        test_edge_cases();
        
        std::cout << "\n=== All tests completed without crashes ===" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\nEXCEPTION: " << e.what() << std::endl;
        OQS_destroy();
        return 1;
    }
    
    // Cleanup
    OQS_destroy();
    return 0;
}