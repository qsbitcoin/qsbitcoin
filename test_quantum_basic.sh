#!/bin/bash
set -e

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Basic Quantum Bitcoin Test ===${NC}"

# Set up paths
BITCOIND="./build/bin/bitcoind"
BITCOIN_CLI="./build/bin/bitcoin-cli"

# Clean up any previous test
echo -e "${YELLOW}Stopping any existing bitcoind...${NC}"
$BITCOIN_CLI -regtest stop 2>/dev/null || true
sleep 2

# Start bitcoind in regtest mode
echo -e "${YELLOW}Starting bitcoind in regtest mode...${NC}"
$BITCOIND -regtest -daemon -fallbackfee=0.00001
sleep 3

# Create a new wallet (remove old one first)
echo -e "${GREEN}1. Creating new wallet...${NC}"
rm -rf ~/.bitcoin/regtest/wallets/test_wallet
$BITCOIN_CLI -regtest createwallet "test_wallet"

# Generate some blocks to have funds
echo -e "${GREEN}2. Mining initial blocks...${NC}"
MINING_ADDR=$($BITCOIN_CLI -regtest getnewaddress)
$BITCOIN_CLI -regtest generatetoaddress 101 "$MINING_ADDR" > /dev/null

# Check balance
echo -e "${GREEN}3. Initial balance:${NC}"
$BITCOIN_CLI -regtest getbalance

# Create quantum addresses
echo -e "${GREEN}4. Creating quantum addresses...${NC}"
ML_DSA_ADDR=$($BITCOIN_CLI -regtest getnewquantumaddress "" "ml-dsa")
echo "ML-DSA address: $ML_DSA_ADDR"

SLH_DSA_ADDR=$($BITCOIN_CLI -regtest getnewquantumaddress "" "slh-dsa")
echo "SLH-DSA address: $SLH_DSA_ADDR"

# Validate addresses
echo -e "${GREEN}5. Validating addresses...${NC}"
echo "ML-DSA validation:"
$BITCOIN_CLI -regtest validatequantumaddress "$ML_DSA_ADDR" | jq '{isvalid, iswitness, witness_version}'

echo "SLH-DSA validation:"
$BITCOIN_CLI -regtest validatequantumaddress "$SLH_DSA_ADDR" | jq '{isvalid, iswitness, witness_version}'

# Send to quantum addresses
echo -e "${GREEN}6. Sending to quantum addresses...${NC}"
TXID1=$($BITCOIN_CLI -regtest sendtoaddress "$ML_DSA_ADDR" 1.0)
echo "Sent to ML-DSA: $TXID1"

TXID2=$($BITCOIN_CLI -regtest sendtoaddress "$SLH_DSA_ADDR" 2.0)
echo "Sent to SLH-DSA: $TXID2"

# Mine a block
echo -e "${GREEN}7. Mining block to confirm...${NC}"
$BITCOIN_CLI -regtest generatetoaddress 1 "$MINING_ADDR" > /dev/null

# Check UTXOs
echo -e "${GREEN}8. Checking UTXOs...${NC}"
$BITCOIN_CLI -regtest listunspent | jq 'map(select(.amount >= 1.0)) | map({address, amount, confirmations})'

# Get quantum info
echo -e "${GREEN}9. Quantum wallet info:${NC}"
$BITCOIN_CLI -regtest getquantuminfo | jq .

# Test wallet persistence
echo -e "${GREEN}10. Testing wallet persistence...${NC}"
$BITCOIN_CLI -regtest unloadwallet "test_wallet"
$BITCOIN_CLI -regtest loadwallet "test_wallet"

# Verify addresses still work
echo -e "${GREEN}11. Verifying after reload...${NC}"
$BITCOIN_CLI -regtest listreceivedbyaddress 0 true | jq 'map(select(.address == "'$ML_DSA_ADDR'" or .address == "'$SLH_DSA_ADDR'")) | map({address, amount})'

# Final balance
echo -e "${GREEN}12. Final balance:${NC}"
$BITCOIN_CLI -regtest getbalance

# Stop bitcoind
echo -e "${YELLOW}Stopping bitcoind...${NC}"
$BITCOIN_CLI -regtest stop

echo -e "${GREEN}=== Test completed! ===${NC}"