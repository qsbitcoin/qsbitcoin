#!/bin/bash

# Simple test for quantum UTXO recognition
set -e

echo "=== Testing Quantum UTXO Recognition ==="

BITCOIN_CLI="./build/bin/bitcoin-cli"

# Create a quantum wallet with the quantum flag
echo "Creating quantum wallet..."
$BITCOIN_CLI -regtest createwallet "quantum_test" false false "" false true false true true

# Check wallet info
echo -e "\n=== Wallet Info ==="
$BITCOIN_CLI -regtest getwalletinfo | jq '{walletname, balance, unconfirmed_balance}'

# Get quantum addresses
echo -e "\n=== Getting quantum addresses ==="
QUANTUM_ML_DSA=$($BITCOIN_CLI -regtest getnewquantumaddress "ml-dsa-test" "ml-dsa")
QUANTUM_SLH_DSA=$($BITCOIN_CLI -regtest getnewquantumaddress "slh-dsa-test" "slh-dsa")

echo "ML-DSA Address: $QUANTUM_ML_DSA"
echo "SLH-DSA Address: $QUANTUM_SLH_DSA"

# Check quantum info
echo -e "\n=== Quantum wallet info ==="
$BITCOIN_CLI -regtest getquantuminfo

# Get address info
echo -e "\n=== Address info for ML-DSA ==="
$BITCOIN_CLI -regtest getaddressinfo "$QUANTUM_ML_DSA" | jq '{address, ismine, solvable, desc}'

echo -e "\n=== Address info for SLH-DSA ==="
$BITCOIN_CLI -regtest getaddressinfo "$QUANTUM_SLH_DSA" | jq '{address, ismine, solvable, desc}'

echo -e "\n=== Testing complete ==="