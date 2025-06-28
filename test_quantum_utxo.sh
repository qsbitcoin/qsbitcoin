#!/bin/bash

# Test quantum UTXO recognition
set -e

echo "=== Testing Quantum UTXO Recognition ==="

# Use built binaries
BITCOIND="./build/bin/bitcoind"
BITCOIN_CLI="./build/bin/bitcoin-cli"

# Stop any running bitcoind
$BITCOIN_CLI -regtest stop 2>/dev/null || true
sleep 2

# Clean up old data
rm -rf ~/.bitcoin/regtest/wallets/quantum_utxo_test

# Start bitcoind (already started)
# $BITCOIND -regtest -daemon -fallbackfee=0.00001 -acceptnonstdtxn=1
sleep 3

# Create a quantum wallet
echo "Creating quantum wallet..."
$BITCOIN_CLI -regtest createwallet "quantum_utxo_test" false false "" false true false true true

# Generate some blocks to a legacy address for mining rewards
echo "Generating initial blocks..."
LEGACY_ADDR=$($BITCOIN_CLI -regtest getnewaddress)
$BITCOIN_CLI -regtest generatetoaddress 101 "$LEGACY_ADDR"

# Get quantum addresses
echo "Getting quantum addresses..."
QUANTUM_ML_DSA=$($BITCOIN_CLI -regtest getnewquantumaddress "ml-dsa-test" "ml-dsa")
QUANTUM_SLH_DSA=$($BITCOIN_CLI -regtest getnewquantumaddress "slh-dsa-test" "slh-dsa")

echo "ML-DSA Address: $QUANTUM_ML_DSA"
echo "SLH-DSA Address: $QUANTUM_SLH_DSA"

# Send coins to quantum addresses
echo "Sending coins to quantum addresses..."
TXID1=$($BITCOIN_CLI -regtest sendtoaddress "$QUANTUM_ML_DSA" 1.0)
TXID2=$($BITCOIN_CLI -regtest sendtoaddress "$QUANTUM_SLH_DSA" 2.0)

echo "TX1 (ML-DSA): $TXID1"
echo "TX2 (SLH-DSA): $TXID2"

# Mine a block to confirm
$BITCOIN_CLI -regtest generatetoaddress 1 "$LEGACY_ADDR"

# Check if wallet recognizes the UTXOs
echo -e "\n=== Checking wallet balance ==="
$BITCOIN_CLI -regtest getbalance

echo -e "\n=== Checking unspent outputs ==="
$BITCOIN_CLI -regtest listunspent | jq '.[] | select(.address == "'$QUANTUM_ML_DSA'" or .address == "'$QUANTUM_SLH_DSA'")'

echo -e "\n=== Checking address info ==="
$BITCOIN_CLI -regtest getaddressinfo "$QUANTUM_ML_DSA" | jq '{address, ismine, solvable, ischange, isscript}'
$BITCOIN_CLI -regtest getaddressinfo "$QUANTUM_SLH_DSA" | jq '{address, ismine, solvable, ischange, isscript}'

echo -e "\n=== Wallet quantum info ==="
$BITCOIN_CLI -regtest getquantuminfo

echo -e "\n=== Testing complete ==="