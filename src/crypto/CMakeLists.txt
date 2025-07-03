# Copyright (c) 2023-present The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.

#=============================
# liboqs configuration
#=============================
message("")
message("Configuring liboqs...")
set(OQS_BUILD_ONLY_LIB ON CACHE BOOL "Build only liboqs library" FORCE)
set(OQS_USE_OPENSSL OFF CACHE BOOL "Build without OpenSSL" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build liboqs as static library" FORCE)

# First disable all KEMs
set(OQS_ENABLE_KEM_BIKE OFF CACHE BOOL "" FORCE)
set(OQS_ENABLE_KEM_CLASSIC_MCELIECE OFF CACHE BOOL "" FORCE)
set(OQS_ENABLE_KEM_HQC OFF CACHE BOOL "" FORCE)
set(OQS_ENABLE_KEM_KYBER OFF CACHE BOOL "" FORCE)
set(OQS_ENABLE_KEM_ML_KEM OFF CACHE BOOL "" FORCE)
set(OQS_ENABLE_KEM_FRODOKEM OFF CACHE BOOL "" FORCE)
set(OQS_ENABLE_KEM_NTRUPRIME OFF CACHE BOOL "" FORCE)

# Disable signature algorithms we don't need
set(OQS_ENABLE_SIG_CROSS OFF CACHE BOOL "" FORCE)
set(OQS_ENABLE_SIG_DILITHIUM OFF CACHE BOOL "" FORCE)
set(OQS_ENABLE_SIG_FALCON OFF CACHE BOOL "" FORCE)
set(OQS_ENABLE_SIG_MAYO OFF CACHE BOOL "" FORCE)

# Enable only the algorithms we need
# ML-DSA is enabled by default, just enable specific variant
set(OQS_ENABLE_SIG_ml_dsa_65 ON CACHE BOOL "Enable ML-DSA-65" FORCE)
# Disable other ML-DSA variants
set(OQS_ENABLE_SIG_ml_dsa_44 OFF CACHE BOOL "" FORCE)
set(OQS_ENABLE_SIG_ml_dsa_87 OFF CACHE BOOL "" FORCE)

# Enable SPHINCS+ and specific variant
set(OQS_ENABLE_SIG_SPHINCS ON CACHE BOOL "Enable SPHINCS+ parent category" FORCE)
set(OQS_ENABLE_SIG_sphincs_sha2_192f_simple ON CACHE BOOL "Enable SPHINCS+ SHA2-192f-simple" FORCE)

# Use minimal build to only build what we need
set(OQS_MINIMAL_BUILD "SIG_ml_dsa_65;SIG_sphincs_sha2_192f_simple" CACHE STRING "" FORCE)

add_subdirectory(${CMAKE_SOURCE_DIR}/liboqs ${CMAKE_BINARY_DIR}/liboqs EXCLUDE_FROM_ALL)

add_library(bitcoin_crypto STATIC EXCLUDE_FROM_ALL
  aes.cpp
  chacha20.cpp
  chacha20poly1305.cpp
  hex_base.cpp
  hkdf_sha256_32.cpp
  hmac_sha256.cpp
  hmac_sha512.cpp
  muhash.cpp
  poly1305.cpp
  ripemd160.cpp
  sha1.cpp
  sha256.cpp
  sha256_sse4.cpp
  sha3.cpp
  sha512.cpp
  siphash.cpp
  ../support/cleanse.cpp
  # Quantum-safe signature schemes
  signature_scheme.cpp
  ecdsa_scheme.cpp
  oqs_wrapper.cpp
  mldsa_scheme.cpp
  slhdsa_scheme.cpp
  quantum_key.cpp
  quantum_key_io.cpp
)

target_link_libraries(bitcoin_crypto
  PRIVATE
    core_interface
    oqs  # Link liboqs for quantum-safe signatures
)

# Include liboqs headers
target_include_directories(bitcoin_crypto
  PRIVATE
    ${CMAKE_SOURCE_DIR}/liboqs/include
    ${CMAKE_BINARY_DIR}/liboqs/include
)

if(HAVE_SSE41)
  target_compile_definitions(bitcoin_crypto PRIVATE ENABLE_SSE41)
  target_sources(bitcoin_crypto PRIVATE sha256_sse41.cpp)
  set_property(SOURCE sha256_sse41.cpp PROPERTY
    COMPILE_OPTIONS ${SSE41_CXXFLAGS}
  )
endif()

if(HAVE_AVX2)
  target_compile_definitions(bitcoin_crypto PRIVATE ENABLE_AVX2)
  target_sources(bitcoin_crypto PRIVATE sha256_avx2.cpp)
  set_property(SOURCE sha256_avx2.cpp PROPERTY
    COMPILE_OPTIONS ${AVX2_CXXFLAGS}
  )
endif()

if(HAVE_SSE41 AND HAVE_X86_SHANI)
  target_compile_definitions(bitcoin_crypto PRIVATE ENABLE_SSE41 ENABLE_X86_SHANI)
  target_sources(bitcoin_crypto PRIVATE sha256_x86_shani.cpp)
  set_property(SOURCE sha256_x86_shani.cpp PROPERTY
    COMPILE_OPTIONS ${SSE41_CXXFLAGS} ${X86_SHANI_CXXFLAGS}
  )
endif()

if(HAVE_ARM_SHANI)
  target_compile_definitions(bitcoin_crypto PRIVATE ENABLE_ARM_SHANI)
  target_sources(bitcoin_crypto PRIVATE sha256_arm_shani.cpp)
  set_property(SOURCE sha256_arm_shani.cpp PROPERTY
    COMPILE_OPTIONS ${ARM_SHANI_CXXFLAGS}
  )
endif()
