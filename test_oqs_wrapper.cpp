/*
 * test_oqs_wrapper.cpp
 * 
 * Test the OQSContext wrapper class used in Bitcoin for SLH-DSA signatures
 * This tests the wrapper layer between Bitcoin and liboqs
 */

#include <iostream>
#include <vector>
#include <cstring>
#include <chrono>

// Include the OQS wrapper implementation
#include "src/crypto/oqs_wrapper.cpp"
#include "src/uint256.h"
#include "src/random.h"

using namespace quantum;

void print_result(const std::string& test, bool success) {
    std::cout << test << ": " << (success ? "PASS ✓" : "FAIL ✗") << std::endl;
}

void print_hex(const std::string& label, const uint8_t* data, size_t len) {
    std::cout << label << " (" << len << " bytes): ";
    if (len > 32) {
        for (size_t i = 0; i < 16; ++i) {
            printf("%02x", data[i]);
        }
        std::cout << "...";
        for (size_t i = len - 16; i < len; ++i) {
            printf("%02x", data[i]);
        }
    } else {
        for (size_t i = 0; i < len; ++i) {
            printf("%02x", data[i]);
        }
    }
    std::cout << std::endl;
}

bool test_oqs_context_creation() {
    std::cout << "\n=== Test OQSContext Creation ===" << std::endl;
    
    try {
        // Test SLH-DSA context creation
        OQSContext ctx("SPHINCS+-SHA2-192f-simple");
        
        std::cout << "Algorithm: " << ctx.GetAlgorithmName() << std::endl;
        std::cout << "Public key size: " << ctx.GetPublicKeySize() << " bytes" << std::endl;
        std::cout << "Secret key size: " << ctx.GetSecretKeySize() << " bytes" << std::endl;
        std::cout << "Max signature size: " << ctx.GetMaxSignatureSize() << " bytes" << std::endl;
        
        // Check expected sizes for SLH-DSA-192f
        bool size_check = (ctx.GetPublicKeySize() == 48) &&
                         (ctx.GetSecretKeySize() == 96) &&
                         (ctx.GetMaxSignatureSize() == 49856);
        
        print_result("Size verification", size_check);
        
        // Test ML-DSA context creation
        OQSContext ctx2("ML-DSA-65");
        std::cout << "\nML-DSA-65 signature size: " << ctx2.GetMaxSignatureSize() << " bytes" << std::endl;
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return false;
    }
}

bool test_keypair_generation() {
    std::cout << "\n=== Test Keypair Generation ===" << std::endl;
    
    try {
        OQSContext ctx("SPHINCS+-SHA2-192f-simple");
        
        std::vector<unsigned char> public_key;
        std::vector<unsigned char> secret_key;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        bool result = ctx.GenerateKeypair(public_key, secret_key);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        print_result("Keypair generation", result);
        
        if (result) {
            std::cout << "Generation time: " << duration.count() << " ms" << std::endl;
            print_hex("Public key", public_key.data(), 48);
            print_hex("Secret key", secret_key.data(), 48);
            
            // Verify sizes
            bool size_ok = (public_key.size() == ctx.GetPublicKeySize()) &&
                          (secret_key.size() == ctx.GetSecretKeySize());
            print_result("Key size verification", size_ok);
        }
        
        return result;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return false;
    }
}

bool test_sign_and_verify() {
    std::cout << "\n=== Test Sign and Verify ===" << std::endl;
    
    try {
        OQSContext ctx("SPHINCS+-SHA2-192f-simple");
        
        // Generate keypair
        std::vector<unsigned char> public_key;
        std::vector<unsigned char> secret_key;
        
        if (!ctx.GenerateKeypair(public_key, secret_key)) {
            std::cerr << "Failed to generate keypair" << std::endl;
            return false;
        }
        
        // Create a test message (32 bytes like Bitcoin hash)
        unsigned char message[32];
        memset(message, 0xAA, 32);
        print_hex("Message", message, 32);
        
        // Sign the message
        std::vector<unsigned char> signature;
        size_t signature_len = 0;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        bool sign_result = ctx.Sign(signature, signature_len, message, 32, secret_key);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto sign_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        print_result("Signing", sign_result);
        
        if (sign_result) {
            std::cout << "Signing time: " << sign_duration.count() << " ms" << std::endl;
            std::cout << "Signature size: " << signature_len << " bytes" << std::endl;
            print_hex("Signature", signature.data(), std::min(signature_len, size_t(64)));
            
            // Verify the signature
            start = std::chrono::high_resolution_clock::now();
            
            bool verify_result = ctx.Verify(message, 32, signature.data(), signature_len, public_key);
            
            end = std::chrono::high_resolution_clock::now();
            auto verify_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            
            print_result("Verification", verify_result);
            std::cout << "Verification time: " << verify_duration.count() << " μs" << std::endl;
            
            // Test with wrong message
            message[0] ^= 0xFF;
            bool wrong_msg_result = ctx.Verify(message, 32, signature.data(), signature_len, public_key);
            print_result("Reject wrong message", !wrong_msg_result);
            
            // Test with corrupted signature
            message[0] ^= 0xFF; // Restore original
            signature[0] ^= 0xFF;
            bool wrong_sig_result = ctx.Verify(message, 32, signature.data(), signature_len, public_key);
            print_result("Reject corrupted signature", !wrong_sig_result);
            
            return verify_result;
        }
        
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return false;
    }
}

bool test_secure_memory() {
    std::cout << "\n=== Test Secure Memory ===" << std::endl;
    
    try {
        // Test SecureQuantumKey
        SecureQuantumKey secure_key(96); // SLH-DSA secret key size
        
        // Fill with test data
        memset(secure_key.data(), 0xBB, secure_key.size());
        
        bool size_ok = (secure_key.size() == 96);
        print_result("SecureQuantumKey size", size_ok);
        
        // Test ToVector
        std::vector<unsigned char> vec = secure_key.ToVector();
        bool copy_ok = (vec.size() == 96) && (vec[0] == 0xBB);
        print_result("SecureQuantumKey copy", copy_ok);
        
        return size_ok && copy_ok;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return false;
    }
}

bool test_multiple_contexts() {
    std::cout << "\n=== Test Multiple Contexts ===" << std::endl;
    
    try {
        // Create multiple contexts
        OQSContext ctx1("SPHINCS+-SHA2-192f-simple");
        OQSContext ctx2("ML-DSA-65");
        
        // Generate keypairs
        std::vector<unsigned char> pub1, sec1, pub2, sec2;
        
        bool gen1 = ctx1.GenerateKeypair(pub1, sec1);
        bool gen2 = ctx2.GenerateKeypair(pub2, sec2);
        
        print_result("SLH-DSA keypair", gen1);
        print_result("ML-DSA keypair", gen2);
        
        // Sign with both
        unsigned char message[32];
        memset(message, 0xCC, 32);
        
        std::vector<unsigned char> sig1, sig2;
        size_t sig1_len = 0, sig2_len = 0;
        
        bool sign1 = ctx1.Sign(sig1, sig1_len, message, 32, sec1);
        bool sign2 = ctx2.Sign(sig2, sig2_len, message, 32, sec2);
        
        print_result("SLH-DSA sign", sign1);
        print_result("ML-DSA sign", sign2);
        
        if (sign1 && sign2) {
            std::cout << "SLH-DSA signature: " << sig1_len << " bytes" << std::endl;
            std::cout << "ML-DSA signature: " << sig2_len << " bytes" << std::endl;
            
            // Verify signatures don't interfere
            bool verify1 = ctx1.Verify(message, 32, sig1.data(), sig1_len, pub1);
            bool verify2 = ctx2.Verify(message, 32, sig2.data(), sig2_len, pub2);
            
            print_result("SLH-DSA verify", verify1);
            print_result("ML-DSA verify", verify2);
            
            return verify1 && verify2;
        }
        
        return false;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return false;
    }
}

bool test_stress() {
    std::cout << "\n=== Stress Test ===" << std::endl;
    
    try {
        OQSContext ctx("SPHINCS+-SHA2-192f-simple");
        
        // Generate keypair once
        std::vector<unsigned char> public_key, secret_key;
        if (!ctx.GenerateKeypair(public_key, secret_key)) {
            std::cerr << "Failed to generate keypair" << std::endl;
            return false;
        }
        
        const int NUM_ITERATIONS = 5;
        int successes = 0;
        
        std::cout << "Running " << NUM_ITERATIONS << " sign/verify cycles..." << std::endl;
        
        auto total_start = std::chrono::high_resolution_clock::now();
        
        for (int i = 0; i < NUM_ITERATIONS; ++i) {
            // Generate random message
            unsigned char message[32];
            GetRandBytes(message, 32);
            
            // Sign
            std::vector<unsigned char> signature;
            size_t sig_len = 0;
            
            if (!ctx.Sign(signature, sig_len, message, 32, secret_key)) {
                std::cerr << "Sign failed at iteration " << i << std::endl;
                continue;
            }
            
            // Verify
            if (!ctx.Verify(message, 32, signature.data(), sig_len, public_key)) {
                std::cerr << "Verify failed at iteration " << i << std::endl;
                continue;
            }
            
            successes++;
            if ((i + 1) % 1 == 0) {
                std::cout << "." << std::flush;
            }
        }
        
        auto total_end = std::chrono::high_resolution_clock::now();
        auto total_duration = std::chrono::duration_cast<std::chrono::seconds>(total_end - total_start);
        
        std::cout << std::endl;
        std::cout << "Completed: " << successes << "/" << NUM_ITERATIONS << std::endl;
        std::cout << "Total time: " << total_duration.count() << " seconds" << std::endl;
        std::cout << "Average: " << (total_duration.count() * 1000 / NUM_ITERATIONS) << " ms per cycle" << std::endl;
        
        return successes == NUM_ITERATIONS;
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return false;
    }
}

int main() {
    std::cout << "=== OQS Wrapper Test Suite ===" << std::endl;
    std::cout << "Testing quantum signature wrapper for Bitcoin" << std::endl;
    
    // Initialize liboqs
    OQS_init();
    
    // Initialize Bitcoin's RNG
    ECC_Start();
    
    bool all_passed = true;
    
    // Run tests
    all_passed &= test_oqs_context_creation();
    all_passed &= test_keypair_generation();
    all_passed &= test_sign_and_verify();
    all_passed &= test_secure_memory();
    all_passed &= test_multiple_contexts();
    all_passed &= test_stress();
    
    std::cout << "\n=== Summary ===" << std::endl;
    if (all_passed) {
        std::cout << "All tests PASSED ✓" << std::endl;
    } else {
        std::cout << "Some tests FAILED ✗" << std::endl;
    }
    
    // Cleanup
    ECC_Stop();
    OQS_destroy();
    
    return all_passed ? 0 : 1;
}