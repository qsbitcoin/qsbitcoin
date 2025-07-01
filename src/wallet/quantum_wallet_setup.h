// Copyright (c) 2024-present The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_QUANTUM_WALLET_SETUP_H
#define BITCOIN_WALLET_QUANTUM_WALLET_SETUP_H

#include <script/quantum_signature.h>
#include <crypto/quantum_key.h>
#include <memory>

namespace wallet {

class CWallet;
class WalletBatch;

/**
 * Setup a quantum descriptor for the wallet
 * @param wallet The wallet to add the descriptor to
 * @param batch Database batch for atomic operations
 * @param scheme_id Either SCHEME_ML_DSA_65 or SCHEME_SLH_DSA_192F
 * @param internal Whether this is for change addresses (currently unused for quantum)
 * @param provided_key Optional pre-generated quantum key to use (if null, generates new key)
 * @return true on success
 */
bool SetupQuantumDescriptor(CWallet& wallet, WalletBatch& batch, quantum::SignatureSchemeID scheme_id, bool internal, std::unique_ptr<quantum::CQuantumKey> provided_key = nullptr);

/**
 * Setup all quantum descriptors for a wallet
 * Creates both ML-DSA and SLH-DSA descriptors
 */
void SetupQuantumDescriptors(CWallet& wallet, WalletBatch& batch);

} // namespace wallet

#endif // BITCOIN_WALLET_QUANTUM_WALLET_SETUP_H