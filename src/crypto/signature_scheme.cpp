// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/signature_scheme.h>
#include <crypto/ecdsa_scheme.h>
#include <crypto/mldsa_scheme.h>
#include <crypto/slhdsa_scheme.h>
#include <crypto/oqs_wrapper.h>
#include <sync.h>

namespace quantum {

// Static instance pointer
SignatureSchemeRegistry* SignatureSchemeRegistry::s_instance = nullptr;

// Mutex for thread-safe singleton access
static Mutex g_registry_mutex;

SignatureSchemeRegistry::SignatureSchemeRegistry()
{
    // Register default schemes
    RegisterScheme(std::make_unique<ECDSAScheme>());
    
    // Register quantum-safe schemes if available
    if (OQSContext::IsAlgorithmAvailable("ML-DSA-65")) {
        RegisterScheme(std::make_unique<MLDSAScheme>());
    }
    
    if (OQSContext::IsAlgorithmAvailable("SPHINCS+-SHA2-192f-simple")) {
        RegisterScheme(std::make_unique<SLHDSAScheme>());
    }
}

SignatureSchemeRegistry& SignatureSchemeRegistry::GetInstance()
{
    LOCK(g_registry_mutex);
    if (!s_instance) {
        s_instance = new SignatureSchemeRegistry();
    }
    return *s_instance;
}

void SignatureSchemeRegistry::RegisterScheme(std::unique_ptr<ISignatureScheme> scheme)
{
    if (!scheme) {
        return;
    }
    
    SignatureSchemeId id = scheme->GetSchemeId();
    m_schemes[id] = std::move(scheme);
}

const ISignatureScheme* SignatureSchemeRegistry::GetScheme(SignatureSchemeId id) const
{
    auto it = m_schemes.find(id);
    if (it != m_schemes.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<SignatureSchemeId> SignatureSchemeRegistry::GetRegisteredSchemes() const
{
    std::vector<SignatureSchemeId> result;
    for (const auto& [id, scheme] : m_schemes) {
        result.push_back(id);
    }
    return result;
}

bool SignatureSchemeRegistry::IsSchemeRegistered(SignatureSchemeId id) const
{
    return m_schemes.find(id) != m_schemes.end();
}

} // namespace quantum