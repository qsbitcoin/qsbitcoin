/*
 * test_slhdsa_direct.cpp
 *
 * Direct test of SLH-DSA signing in Bitcoin context
 * Simulates what happens when creating a transaction to an SLH-DSA address
 */

#include <iostream>
#include <vector>
#include <cstring>
#include <memory>

// Simulate Bitcoin's transaction structures
struct CTxOut {
    int64_t nValue;
    std::vector<unsigned char> scriptPubKey;
};

struct CTxIn {
    std::vector<unsigned char> scriptSig;
};

// Constants from Bitcoin
constexpr size_t MAX_SCRIPT_ELEMENT_SIZE = 520;
constexpr size_t MAX_STANDARD_TX_WEIGHT = 400000;
constexpr size_t MAX_STANDARD_TX_WEIGHT_QUANTUM = 1000000;

// SLH-DSA sizes
constexpr size_t SLH_DSA_PUBKEY_SIZE = 48;
constexpr size_t SLH_DSA_SIGNATURE_SIZE = 35664;  // Actual size from test
constexpr size_t SLH_DSA_EXPECTED_SIZE = 35664;   // SLH-DSA-192f signature size per NIST standard

// Quantum scheme IDs
constexpr uint8_t SCHEME_ID_ECDSA = 0x01;
constexpr uint8_t SCHEME_ID_ML_DSA = 0x02;
constexpr uint8_t SCHEME_ID_SLH_DSA = 0x03;

void print_hex(const std::string& label, const uint8_t* data, size_t len) {
    std::cout << label << " (" << len << " bytes): ";
    if (len > 32) {
        for (size_t i = 0; i < 16; ++i) {
            printf("%02x", data[i]);
        }
        std::cout << "...";
    } else {
        for (size_t i = 0; i < len; ++i) {
            printf("%02x", data[i]);
        }
    }
    std::cout << std::endl;
}

// Simulate creating a quantum signature script
std::vector<unsigned char> CreateQuantumScriptSig(uint8_t scheme_id, size_t sig_size, size_t pubkey_size) {
    std::vector<unsigned char> scriptSig;
    
    // Reserve space to avoid reallocations
    size_t estimated_size = 1 + 5 + sig_size + 1 + pubkey_size; // scheme_id + varint + sig + varint + pubkey
    scriptSig.reserve(estimated_size);
    
    std::cout << "Creating scriptSig for scheme " << (int)scheme_id << std::endl;
    std::cout << "Estimated script size: " << estimated_size << " bytes" << std::endl;
    
    // Add scheme ID
    scriptSig.push_back(scheme_id);
    
    // Add signature length (as varint)
    if (sig_size < 253) {
        scriptSig.push_back(sig_size);
    } else if (sig_size <= 0xFFFF) {
        scriptSig.push_back(253);
        scriptSig.push_back(sig_size & 0xFF);
        scriptSig.push_back((sig_size >> 8) & 0xFF);
    } else {
        scriptSig.push_back(254);
        scriptSig.push_back(sig_size & 0xFF);
        scriptSig.push_back((sig_size >> 8) & 0xFF);
        scriptSig.push_back((sig_size >> 16) & 0xFF);
        scriptSig.push_back((sig_size >> 24) & 0xFF);
    }
    
    // Add dummy signature
    std::vector<unsigned char> signature(sig_size, 0xAA);
    scriptSig.insert(scriptSig.end(), signature.begin(), signature.end());
    
    // Add public key length
    scriptSig.push_back(pubkey_size);
    
    // Add dummy public key
    std::vector<unsigned char> pubkey(pubkey_size, 0xBB);
    scriptSig.insert(scriptSig.end(), pubkey.begin(), pubkey.end());
    
    std::cout << "Final scriptSig size: " << scriptSig.size() << " bytes" << std::endl;
    
    return scriptSig;
}

void test_slhdsa_script_creation() {
    std::cout << "\n=== Test SLH-DSA Script Creation ===" << std::endl;
    
    // Test with actual SLH-DSA sizes
    std::cout << "\nTest 1: Actual SLH-DSA sizes" << std::endl;
    auto script1 = CreateQuantumScriptSig(SCHEME_ID_SLH_DSA, SLH_DSA_SIGNATURE_SIZE, SLH_DSA_PUBKEY_SIZE);
    
    // Check if it exceeds limits
    if (script1.size() > MAX_SCRIPT_ELEMENT_SIZE) {
        std::cout << "WARNING: Script exceeds MAX_SCRIPT_ELEMENT_SIZE (" 
                  << script1.size() << " > " << MAX_SCRIPT_ELEMENT_SIZE << ")" << std::endl;
    }
    
    // Test with expected sizes (what Bitcoin might allocate)
    std::cout << "\nTest 2: Expected SLH-DSA sizes" << std::endl;
    auto script2 = CreateQuantumScriptSig(SCHEME_ID_SLH_DSA, SLH_DSA_EXPECTED_SIZE, SLH_DSA_PUBKEY_SIZE);
    
    // Test ML-DSA for comparison
    std::cout << "\nTest 3: ML-DSA sizes" << std::endl;
    auto script3 = CreateQuantumScriptSig(SCHEME_ID_ML_DSA, 3309, 2592); // ML-DSA-65 sizes
}

void test_memory_patterns() {
    std::cout << "\n=== Test Memory Allocation Patterns ===" << std::endl;
    
    // Simulate what might happen in Bitcoin's signing code
    for (int i = 0; i < 5; i++) {
        std::cout << "\nIteration " << i << std::endl;
        
        // Create a transaction input
        CTxIn input;
        
        // Reserve space for worst case
        std::cout << "Reserving " << SLH_DSA_EXPECTED_SIZE << " bytes..." << std::endl;
        input.scriptSig.reserve(SLH_DSA_EXPECTED_SIZE + 100);
        
        // But only use actual size
        std::cout << "Using " << SLH_DSA_SIGNATURE_SIZE << " bytes..." << std::endl;
        auto script = CreateQuantumScriptSig(SCHEME_ID_SLH_DSA, SLH_DSA_SIGNATURE_SIZE, SLH_DSA_PUBKEY_SIZE);
        input.scriptSig = std::move(script);
        
        // Check capacity vs size
        std::cout << "scriptSig size: " << input.scriptSig.size() 
                  << ", capacity: " << input.scriptSig.capacity() << std::endl;
    }
}

void test_buffer_overflow_scenarios() {
    std::cout << "\n=== Test Buffer Overflow Scenarios ===" << std::endl;
    
    // Test 1: Writing to fixed-size buffer
    std::cout << "\nTest 1: Fixed buffer with SLH-DSA signature" << std::endl;
    {
        // Allocate buffer based on expected size
        std::vector<uint8_t> buffer(SLH_DSA_EXPECTED_SIZE);
        
        // Try to write actual signature
        std::vector<uint8_t> signature(SLH_DSA_SIGNATURE_SIZE, 0xCC);
        
        if (signature.size() <= buffer.size()) {
            std::copy(signature.begin(), signature.end(), buffer.begin());
            std::cout << "Copy succeeded" << std::endl;
        } else {
            std::cout << "ERROR: Signature too large for buffer!" << std::endl;
            std::cout << "Buffer: " << buffer.size() << ", Signature: " << signature.size() << std::endl;
        }
    }
    
    // Test 2: Stack allocation
    std::cout << "\nTest 2: Stack allocation patterns" << std::endl;
    {
        // Don't actually allocate 35KB on stack, just simulate
        size_t stack_usage = SLH_DSA_SIGNATURE_SIZE + SLH_DSA_PUBKEY_SIZE + 1024; // Plus overhead
        std::cout << "Would use " << stack_usage << " bytes of stack" << std::endl;
        
        if (stack_usage > 8192) { // Typical stack frame limit
            std::cout << "WARNING: Exceeds typical stack frame size!" << std::endl;
        }
    }
}

void test_serialization() {
    std::cout << "\n=== Test Serialization Patterns ===" << std::endl;
    
    // Simulate Bitcoin's serialization
    std::vector<uint8_t> stream;
    stream.reserve(100000); // Reserve plenty
    
    // Add multiple SLH-DSA scripts
    for (int i = 0; i < 3; i++) {
        std::cout << "Adding SLH-DSA script " << i << std::endl;
        
        auto script = CreateQuantumScriptSig(SCHEME_ID_SLH_DSA, SLH_DSA_SIGNATURE_SIZE, SLH_DSA_PUBKEY_SIZE);
        
        // Add script size (varint)
        if (script.size() < 253) {
            stream.push_back(script.size());
        } else {
            stream.push_back(253);
            stream.push_back(script.size() & 0xFF);
            stream.push_back((script.size() >> 8) & 0xFF);
        }
        
        // Add script
        stream.insert(stream.end(), script.begin(), script.end());
        
        std::cout << "Stream size: " << stream.size() << " bytes" << std::endl;
    }
    
    std::cout << "Total serialized size: " << stream.size() << " bytes" << std::endl;
    
    // Check weight
    size_t weight = stream.size() * 4; // Non-witness data counts 4x
    std::cout << "Transaction weight: " << weight << std::endl;
    
    if (weight > MAX_STANDARD_TX_WEIGHT_QUANTUM) {
        std::cout << "ERROR: Exceeds MAX_STANDARD_TX_WEIGHT_QUANTUM!" << std::endl;
    } else if (weight > MAX_STANDARD_TX_WEIGHT) {
        std::cout << "Exceeds standard weight but OK for quantum" << std::endl;
    }
}

int main() {
    std::cout << "=== SLH-DSA Bitcoin Integration Test ===" << std::endl;
    std::cout << "Testing potential crash scenarios\n" << std::endl;
    
    std::cout << "SLH-DSA Constants:" << std::endl;
    std::cout << "  Actual signature size: " << SLH_DSA_SIGNATURE_SIZE << " bytes" << std::endl;
    std::cout << "  Expected signature size: " << SLH_DSA_EXPECTED_SIZE << " bytes" << std::endl;
    std::cout << "  Public key size: " << SLH_DSA_PUBKEY_SIZE << " bytes" << std::endl;
    std::cout << "  Size mismatch: " << (SLH_DSA_EXPECTED_SIZE - SLH_DSA_SIGNATURE_SIZE) << " bytes" << std::endl;
    
    try {
        test_slhdsa_script_creation();
        test_memory_patterns();
        test_buffer_overflow_scenarios();
        test_serialization();
        
        std::cout << "\n=== All tests completed successfully ===" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "\nEXCEPTION: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}