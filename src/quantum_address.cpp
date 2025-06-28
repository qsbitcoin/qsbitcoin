// Copyright (c) 2025 The QSBitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <quantum_address.h>
#include <crypto/quantum_key.h>
#include <hash.h>
#include <script/script.h>
#include <crypto/sha3.h>

#include <vector>

namespace quantum {

// Quantum signature opcodes are now defined in script/script.h
// OP_CHECKSIG_ML_DSA = OP_NOP4
// OP_CHECKSIG_SLH_DSA = OP_NOP5
// OP_CHECKSIGVERIFY_ML_DSA = OP_NOP6
// OP_CHECKSIGVERIFY_SLH_DSA = OP_NOP7

uint256 QuantumHash256(const std::vector<unsigned char>& data)
{
    // Use SHA3-256 for quantum resistance
    uint256 result;
    SHA3_256 hasher;
    hasher.Write(data);
    hasher.Finalize(result);
    return result;
}

} // namespace quantum