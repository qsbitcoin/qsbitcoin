/*
 * test_slhdsa_liboqs.cpp
 *
 * Direct test of liboqs SLH-DSA (SPHINCS+) functions used in bitcoind
 * This tests the actual OQS library functions without Bitcoin wrapper code
 *
 * SPDX-License-Identifier: MIT
 */

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <chrono>
#include <vector>
#include <iomanip>

#include <oqs/oqs.h>

// Test configuration
constexpr size_t MESSAGE_LEN = 32; // Bitcoin uses 32-byte hashes
constexpr size_t NUM_ITERATIONS = 10; // Number of test iterations
constexpr const char* SLH_DSA_ALGORITHM = "SPHINCS+-SHA2-192f-simple";

// Memory cleaner
struct OQSSecureDeleter {
    size_t length;
    explicit OQSSecureDeleter(size_t len) : length(len) {}
    void operator()(uint8_t* ptr) const {
        if (ptr) {
            OQS_MEM_secure_free(ptr, length);
        }
    }
};

struct OQSSigDeleter {
    void operator()(OQS_SIG* sig) {
        if (sig) {
            OQS_SIG_free(sig);
        }
    }
};

// Helper to print hex
void print_hex(const std::string& label, const uint8_t* data, size_t len) {
    std::cout << label << " (" << len << " bytes): ";
    if (len > 32) {
        // Print first 16 and last 16 bytes
        for (size_t i = 0; i < 16; ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
        }
        std::cout << "...";
        for (size_t i = len - 16; i < len; ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
        }
    } else {
        for (size_t i = 0; i < len; ++i) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)data[i];
        }
    }
    std::cout << std::dec << std::endl;
}

// Test basic SLH-DSA operations
OQS_STATUS test_basic_operations() {
    std::cout << "\n=== Test Basic SLH-DSA Operations ===" << std::endl;
    
    // Create SLH-DSA signature object
    std::unique_ptr<OQS_SIG, OQSSigDeleter> sig(OQS_SIG_new(SLH_DSA_ALGORITHM));
    if (!sig) {
        std::cerr << "ERROR: Failed to create SLH-DSA signature object" << std::endl;
        std::cerr << "Algorithm: " << SLH_DSA_ALGORITHM << std::endl;
        return OQS_ERROR;
    }
    
    std::cout << "Algorithm: " << sig->method_name << std::endl;
    std::cout << "Public key size: " << sig->length_public_key << " bytes" << std::endl;
    std::cout << "Secret key size: " << sig->length_secret_key << " bytes" << std::endl;
    std::cout << "Signature size: " << sig->length_signature << " bytes" << std::endl;
    
    // Allocate memory for keys and signature
    std::unique_ptr<uint8_t[]> public_key(new uint8_t[sig->length_public_key]);
    std::unique_ptr<uint8_t[], OQSSecureDeleter> secret_key(
        new uint8_t[sig->length_secret_key], 
        OQSSecureDeleter(sig->length_secret_key)
    );
    std::unique_ptr<uint8_t[]> signature(new uint8_t[sig->length_signature]);
    
    // Generate keypair
    std::cout << "\nGenerating keypair..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    
    OQS_STATUS rc = OQS_SIG_keypair(sig.get(), public_key.get(), secret_key.get());
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    if (rc != OQS_SUCCESS) {
        std::cerr << "ERROR: Keypair generation failed" << std::endl;
        return OQS_ERROR;
    }
    
    std::cout << "Keypair generated in " << duration.count() << " ms" << std::endl;
    print_hex("Public key", public_key.get(), std::min(sig->length_public_key, size_t(64)));
    print_hex("Secret key", secret_key.get(), std::min(sig->length_secret_key, size_t(64)));
    
    // Create a test message (simulating Bitcoin hash)
    uint8_t message[MESSAGE_LEN];
    memset(message, 0xAB, MESSAGE_LEN); // Fill with test pattern
    print_hex("\nMessage", message, MESSAGE_LEN);
    
    // Sign the message
    std::cout << "\nSigning message..." << std::endl;
    size_t signature_len = 0;
    
    start = std::chrono::high_resolution_clock::now();
    
    rc = OQS_SIG_sign(sig.get(), signature.get(), &signature_len, 
                      message, MESSAGE_LEN, secret_key.get());
    
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    if (rc != OQS_SUCCESS) {
        std::cerr << "ERROR: Signing failed" << std::endl;
        return OQS_ERROR;
    }
    
    std::cout << "Message signed in " << duration.count() << " ms" << std::endl;
    std::cout << "Actual signature size: " << signature_len << " bytes" << std::endl;
    print_hex("Signature", signature.get(), std::min(signature_len, size_t(64)));
    
    // Verify the signature
    std::cout << "\nVerifying signature..." << std::endl;
    
    start = std::chrono::high_resolution_clock::now();
    
    rc = OQS_SIG_verify(sig.get(), message, MESSAGE_LEN, 
                        signature.get(), signature_len, public_key.get());
    
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    if (rc != OQS_SUCCESS) {
        std::cerr << "ERROR: Verification failed" << std::endl;
        return OQS_ERROR;
    }
    
    std::cout << "Signature verified in " << duration.count() << " μs" << std::endl;
    
    // Test with modified message (should fail)
    std::cout << "\nTesting with modified message..." << std::endl;
    message[0] ^= 0xFF; // Flip bits in first byte
    
    rc = OQS_SIG_verify(sig.get(), message, MESSAGE_LEN, 
                        signature.get(), signature_len, public_key.get());
    
    if (rc == OQS_SUCCESS) {
        std::cerr << "ERROR: Verification should have failed with modified message" << std::endl;
        return OQS_ERROR;
    }
    
    std::cout << "Verification correctly failed for modified message" << std::endl;
    
    return OQS_SUCCESS;
}

// Test memory allocation and cleanup
OQS_STATUS test_memory_management() {
    std::cout << "\n=== Test Memory Management ===" << std::endl;
    
    // Test multiple allocations and deallocations
    for (int i = 0; i < 5; ++i) {
        std::cout << "Iteration " << (i + 1) << "..." << std::endl;
        
        std::unique_ptr<OQS_SIG, OQSSigDeleter> sig(OQS_SIG_new(SLH_DSA_ALGORITHM));
        if (!sig) {
            std::cerr << "ERROR: Failed to create signature object in iteration " << (i + 1) << std::endl;
            return OQS_ERROR;
        }
        
        // Allocate large buffers
        std::vector<uint8_t> public_key(sig->length_public_key);
        std::vector<uint8_t> secret_key(sig->length_secret_key);
        std::vector<uint8_t> signature(sig->length_signature);
        
        // Generate keypair
        OQS_STATUS rc = OQS_SIG_keypair(sig.get(), public_key.data(), secret_key.data());
        if (rc != OQS_SUCCESS) {
            std::cerr << "ERROR: Keypair generation failed in iteration " << (i + 1) << std::endl;
            return OQS_ERROR;
        }
        
        // Clear sensitive data
        OQS_MEM_cleanse(secret_key.data(), secret_key.size());
    }
    
    std::cout << "Memory management test completed successfully" << std::endl;
    return OQS_SUCCESS;
}

// Test performance with multiple operations
OQS_STATUS test_performance() {
    std::cout << "\n=== Test Performance ===" << std::endl;
    
    std::unique_ptr<OQS_SIG, OQSSigDeleter> sig(OQS_SIG_new(SLH_DSA_ALGORITHM));
    if (!sig) {
        std::cerr << "ERROR: Failed to create signature object" << std::endl;
        return OQS_ERROR;
    }
    
    // Allocate buffers
    std::vector<uint8_t> public_key(sig->length_public_key);
    std::vector<uint8_t> secret_key(sig->length_secret_key);
    std::vector<uint8_t> signature(sig->length_signature);
    uint8_t message[MESSAGE_LEN];
    
    // Generate test keypair
    OQS_STATUS rc = OQS_SIG_keypair(sig.get(), public_key.data(), secret_key.data());
    if (rc != OQS_SUCCESS) {
        std::cerr << "ERROR: Keypair generation failed" << std::endl;
        return OQS_ERROR;
    }
    
    // Test signing performance
    std::cout << "\nTesting signing performance (" << NUM_ITERATIONS << " iterations)..." << std::endl;
    
    auto total_start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
        // Generate different message each time
        OQS_randombytes(message, MESSAGE_LEN);
        
        size_t sig_len = 0;
        rc = OQS_SIG_sign(sig.get(), signature.data(), &sig_len, 
                          message, MESSAGE_LEN, secret_key.data());
        
        if (rc != OQS_SUCCESS) {
            std::cerr << "ERROR: Signing failed at iteration " << (i + 1) << std::endl;
            return OQS_ERROR;
        }
    }
    
    auto total_end = std::chrono::high_resolution_clock::now();
    auto total_duration = std::chrono::duration_cast<std::chrono::milliseconds>(total_end - total_start);
    
    std::cout << "Total time: " << total_duration.count() << " ms" << std::endl;
    std::cout << "Average per signature: " << (total_duration.count() / NUM_ITERATIONS) << " ms" << std::endl;
    
    // Test verification performance
    std::cout << "\nTesting verification performance (" << NUM_ITERATIONS << " iterations)..." << std::endl;
    
    // Create a signature to verify
    size_t sig_len = 0;
    OQS_randombytes(message, MESSAGE_LEN);
    rc = OQS_SIG_sign(sig.get(), signature.data(), &sig_len, 
                      message, MESSAGE_LEN, secret_key.data());
    
    if (rc != OQS_SUCCESS) {
        std::cerr << "ERROR: Failed to create test signature" << std::endl;
        return OQS_ERROR;
    }
    
    total_start = std::chrono::high_resolution_clock::now();
    
    for (size_t i = 0; i < NUM_ITERATIONS * 10; ++i) { // More iterations for verification
        rc = OQS_SIG_verify(sig.get(), message, MESSAGE_LEN, 
                            signature.data(), sig_len, public_key.data());
        
        if (rc != OQS_SUCCESS) {
            std::cerr << "ERROR: Verification failed at iteration " << (i + 1) << std::endl;
            return OQS_ERROR;
        }
    }
    
    total_end = std::chrono::high_resolution_clock::now();
    total_duration = std::chrono::duration_cast<std::chrono::microseconds>(total_end - total_start);
    
    std::cout << "Total time: " << total_duration.count() << " μs" << std::endl;
    std::cout << "Average per verification: " << (total_duration.count() / (NUM_ITERATIONS * 10)) << " μs" << std::endl;
    
    // Clean up sensitive data
    OQS_MEM_cleanse(secret_key.data(), secret_key.size());
    
    return OQS_SUCCESS;
}

// Test edge cases
OQS_STATUS test_edge_cases() {
    std::cout << "\n=== Test Edge Cases ===" << std::endl;
    
    std::unique_ptr<OQS_SIG, OQSSigDeleter> sig(OQS_SIG_new(SLH_DSA_ALGORITHM));
    if (!sig) {
        std::cerr << "ERROR: Failed to create signature object" << std::endl;
        return OQS_ERROR;
    }
    
    std::vector<uint8_t> public_key(sig->length_public_key);
    std::vector<uint8_t> secret_key(sig->length_secret_key);
    std::vector<uint8_t> signature(sig->length_signature);
    
    // Generate keypair
    OQS_STATUS rc = OQS_SIG_keypair(sig.get(), public_key.data(), secret_key.data());
    if (rc != OQS_SUCCESS) {
        std::cerr << "ERROR: Keypair generation failed" << std::endl;
        return OQS_ERROR;
    }
    
    // Test 1: Empty message
    std::cout << "\nTest 1: Empty message" << std::endl;
    size_t sig_len = 0;
    rc = OQS_SIG_sign(sig.get(), signature.data(), &sig_len, 
                      nullptr, 0, secret_key.data());
    
    if (rc == OQS_SUCCESS) {
        rc = OQS_SIG_verify(sig.get(), nullptr, 0, 
                            signature.data(), sig_len, public_key.data());
        std::cout << "Empty message: " << (rc == OQS_SUCCESS ? "PASS" : "FAIL") << std::endl;
    } else {
        std::cout << "Empty message signing not supported" << std::endl;
    }
    
    // Test 2: Large message
    std::cout << "\nTest 2: Large message (1MB)" << std::endl;
    std::vector<uint8_t> large_message(1024 * 1024);
    OQS_randombytes(large_message.data(), large_message.size());
    
    auto start = std::chrono::high_resolution_clock::now();
    
    rc = OQS_SIG_sign(sig.get(), signature.data(), &sig_len, 
                      large_message.data(), large_message.size(), secret_key.data());
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    if (rc == OQS_SUCCESS) {
        std::cout << "Large message signed in " << duration.count() << " ms" << std::endl;
        
        start = std::chrono::high_resolution_clock::now();
        
        rc = OQS_SIG_verify(sig.get(), large_message.data(), large_message.size(), 
                            signature.data(), sig_len, public_key.data());
        
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "Large message verified in " << duration.count() << " ms" << std::endl;
        std::cout << "Large message: " << (rc == OQS_SUCCESS ? "PASS" : "FAIL") << std::endl;
    } else {
        std::cout << "Large message signing failed" << std::endl;
    }
    
    // Test 3: Wrong signature size
    std::cout << "\nTest 3: Wrong signature size" << std::endl;
    uint8_t message[MESSAGE_LEN];
    OQS_randombytes(message, MESSAGE_LEN);
    
    rc = OQS_SIG_sign(sig.get(), signature.data(), &sig_len, 
                      message, MESSAGE_LEN, secret_key.data());
    
    if (rc == OQS_SUCCESS) {
        // Try to verify with wrong signature length
        rc = OQS_SIG_verify(sig.get(), message, MESSAGE_LEN, 
                            signature.data(), sig_len - 1, public_key.data());
        std::cout << "Wrong signature size correctly rejected: " << (rc != OQS_SUCCESS ? "PASS" : "FAIL") << std::endl;
    }
    
    // Clean up
    OQS_MEM_cleanse(secret_key.data(), secret_key.size());
    
    return OQS_SUCCESS;
}

int main() {
    std::cout << "=== SLH-DSA (SPHINCS+) liboqs Test Suite ===" << std::endl;
    std::cout << "Testing algorithm: " << SLH_DSA_ALGORITHM << std::endl;
    
    // Initialize liboqs
    OQS_init();
    
    // Check if algorithm is available
    if (!OQS_SIG_alg_is_enabled(SLH_DSA_ALGORITHM)) {
        std::cerr << "ERROR: " << SLH_DSA_ALGORITHM << " is not enabled in this build of liboqs" << std::endl;
        OQS_destroy();
        return EXIT_FAILURE;
    }
    
    try {
        OQS_STATUS status;
        
        // Run tests
        status = test_basic_operations();
        if (status != OQS_SUCCESS) {
            throw std::runtime_error("Basic operations test failed");
        }
        
        status = test_memory_management();
        if (status != OQS_SUCCESS) {
            throw std::runtime_error("Memory management test failed");
        }
        
        status = test_performance();
        if (status != OQS_SUCCESS) {
            throw std::runtime_error("Performance test failed");
        }
        
        status = test_edge_cases();
        if (status != OQS_SUCCESS) {
            throw std::runtime_error("Edge cases test failed");
        }
        
        std::cout << "\n=== All tests completed successfully! ===" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "\nERROR: " << e.what() << std::endl;
        OQS_destroy();
        return EXIT_FAILURE;
    }
    
    // Cleanup
    OQS_destroy();
    return EXIT_SUCCESS;
}