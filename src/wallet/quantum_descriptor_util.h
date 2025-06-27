// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_QUANTUM_DESCRIPTOR_UTIL_H
#define BITCOIN_WALLET_QUANTUM_DESCRIPTOR_UTIL_H

#include <script/signingprovider.h>
#include <script/script.h>

namespace wallet {

/**
 * Populate a signing provider with quantum keys for a given script
 * This checks if the script uses quantum signatures and adds the necessary
 * quantum keys from the global keystore to the signing provider
 */
void PopulateQuantumSigningProvider(const CScript& script, FlatSigningProvider& provider, bool include_private = false);

} // namespace wallet

#endif // BITCOIN_WALLET_QUANTUM_DESCRIPTOR_UTIL_H