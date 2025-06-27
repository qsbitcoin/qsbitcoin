// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <crypto/ecdsa_scheme.h>
#include <key.h>
#include <pubkey.h>

namespace quantum {

bool ECDSAScheme::Sign(const uint256& hash, const CKey& key, 
                       std::vector<unsigned char>& sig) const 
{
    if (!key.IsValid()) {
        return false;
    }
    
    // Use Bitcoin Core's existing ECDSA signing
    return key.Sign(hash, sig);
}

bool ECDSAScheme::Verify(const uint256& hash, const CPubKey& pubkey,
                         const std::vector<unsigned char>& sig) const 
{
    if (!pubkey.IsValid()) {
        return false;
    }
    
    // Use Bitcoin Core's existing ECDSA verification
    return pubkey.Verify(hash, sig);
}

} // namespace quantum