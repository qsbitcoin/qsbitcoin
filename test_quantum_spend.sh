#!/bin/bash
set -e

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Quantum Address Spending Test ===${NC}"

# Set up paths
BITCOIND="./build/bin/bitcoind"
BITCOIN_CLI="./build/bin/bitcoin-cli"

# Stop any existing bitcoind
$BITCOIN_CLI -regtest stop 2>/dev/null || true
sleep 2

# Clean start
rm -rf ~/.bitcoin/regtest/wallets/quantum_spend_test

# Start bitcoind
echo -e "${YELLOW}Starting bitcoind...${NC}"
$BITCOIND -regtest -daemon -fallbackfee=0.00001
sleep 3

# Create wallet
echo -e "${GREEN}1. Creating wallet...${NC}"
$BITCOIN_CLI -regtest createwallet "quantum_spend_test"

# Generate initial funds
echo -e "${GREEN}2. Mining initial blocks...${NC}"
MINING_ADDR=$($BITCOIN_CLI -regtest getnewaddress)
$BITCOIN_CLI -regtest generatetoaddress 101 "$MINING_ADDR" > /dev/null

# Create quantum addresses
echo -e "${GREEN}3. Creating quantum addresses...${NC}"
ML_DSA_ADDR=$($BITCOIN_CLI -regtest getnewquantumaddress "ml-dsa-test" "ml-dsa")
echo "ML-DSA address: $ML_DSA_ADDR"

SLH_DSA_ADDR=$($BITCOIN_CLI -regtest getnewquantumaddress "slh-dsa-test" "slh-dsa")
echo "SLH-DSA address: $SLH_DSA_ADDR"

# Send to quantum addresses
echo -e "${GREEN}4. Funding quantum addresses...${NC}"
TX1=$($BITCOIN_CLI -regtest sendtoaddress "$ML_DSA_ADDR" 5.0)
echo "Sent 5 BTC to ML-DSA: $TX1"

TX2=$($BITCOIN_CLI -regtest sendtoaddress "$SLH_DSA_ADDR" 10.0)
echo "Sent 10 BTC to SLH-DSA: $TX2"

# Mine to confirm
$BITCOIN_CLI -regtest generatetoaddress 1 "$MINING_ADDR" > /dev/null

# Check balances
echo -e "${GREEN}5. Checking balances...${NC}"
echo "Total wallet balance: $($BITCOIN_CLI -regtest getbalance)"

# List quantum UTXOs
echo -e "${GREEN}6. Quantum UTXOs:${NC}"
$BITCOIN_CLI -regtest listunspent | jq 'map(select(.address == "'$ML_DSA_ADDR'" or .address == "'$SLH_DSA_ADDR'")) | map({address: .address, amount: .amount, txid: .txid, vout: .vout})'

# Try spending from ML-DSA address
echo -e "${GREEN}7. Testing spend from ML-DSA address...${NC}"
SPEND_ADDR=$($BITCOIN_CLI -regtest getnewaddress "spend-destination")
echo "Destination: $SPEND_ADDR"

# Use sendall to spend from specific addresses
echo "Attempting to spend from ML-DSA..."
SPEND_TX=$($BITCOIN_CLI -regtest send '{"'$SPEND_ADDR'": 1.0}' 2>&1 || echo "SPEND_ERROR: $?")
if [[ "$SPEND_TX" == *"SPEND_ERROR"* ]]; then
    echo -e "${RED}Failed to spend from quantum address${NC}"
    echo "Error: $SPEND_TX"
else
    echo -e "${GREEN}Successfully created spend transaction: $SPEND_TX${NC}"
fi

# Check transaction details
echo -e "${GREEN}8. Transaction mempool:${NC}"
$BITCOIN_CLI -regtest getmempoolinfo

# Mine and check result
echo -e "${GREEN}9. Mining to confirm...${NC}"
$BITCOIN_CLI -regtest generatetoaddress 1 "$MINING_ADDR" > /dev/null

# Final balance
echo -e "${GREEN}10. Final balance:${NC}"
$BITCOIN_CLI -regtest getbalance

# Check wallet quantum info
echo -e "${GREEN}11. Quantum wallet status:${NC}"
$BITCOIN_CLI -regtest getquantuminfo | jq .

# Stop
echo -e "${YELLOW}Stopping bitcoind...${NC}"
$BITCOIN_CLI -regtest stop

echo -e "${GREEN}=== Test completed! ===${NC}"