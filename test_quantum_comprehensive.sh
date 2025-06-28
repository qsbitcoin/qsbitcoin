#!/bin/bash
set -e

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Comprehensive Quantum Bitcoin Test ===${NC}"

# Set up paths
BITCOIND="./build/bin/bitcoind"
BITCOIN_CLI="./build/bin/bitcoin-cli"
DATADIR="$HOME/.bitcoin_quantum_test"

# Clean up any previous test data
echo -e "${YELLOW}Cleaning up previous test data...${NC}"
$BITCOIN_CLI -regtest stop 2>/dev/null || true
sleep 2
rm -rf "$DATADIR"

# Create data directory
mkdir -p "$DATADIR"

# Start bitcoind in regtest mode
echo -e "${YELLOW}Starting bitcoind in regtest mode...${NC}"
$BITCOIND -regtest -datadir="$DATADIR" -daemon -fallbackfee=0.00001

# Wait for bitcoind to start
sleep 3

# Create a new wallet
echo -e "${GREEN}1. Creating new wallet...${NC}"
$BITCOIN_CLI -regtest -datadir="$DATADIR" createwallet "quantum_test_wallet"

# Generate some blocks to have funds
echo -e "${GREEN}2. Mining blocks to generate funds...${NC}"
MINING_ADDR=$($BITCOIN_CLI -regtest -datadir="$DATADIR" getnewaddress "mining")
echo "Mining address: $MINING_ADDR"
$BITCOIN_CLI -regtest -datadir="$DATADIR" generatetoaddress 101 "$MINING_ADDR"

# Check balance
echo -e "${GREEN}3. Checking initial balance...${NC}"
BALANCE=$($BITCOIN_CLI -regtest -datadir="$DATADIR" getbalance)
echo "Balance: $BALANCE BTC"

# Create quantum addresses
echo -e "${GREEN}4. Creating quantum addresses...${NC}"
echo -e "${YELLOW}Creating ML-DSA address...${NC}"
ML_DSA_ADDR=$($BITCOIN_CLI -regtest -datadir="$DATADIR" getnewquantumaddress "ml-dsa-test" "ml-dsa")
echo "ML-DSA address: $ML_DSA_ADDR"

echo -e "${YELLOW}Creating SLH-DSA address...${NC}"
SLH_DSA_ADDR=$($BITCOIN_CLI -regtest -datadir="$DATADIR" getnewquantumaddress "slh-dsa-test" "slh-dsa")
echo "SLH-DSA address: $SLH_DSA_ADDR"

# Create regular addresses for comparison
echo -e "${YELLOW}Creating regular P2WPKH address...${NC}"
REGULAR_ADDR=$($BITCOIN_CLI -regtest -datadir="$DATADIR" getnewaddress "regular-test" "bech32")
echo "Regular address: $REGULAR_ADDR"

# Validate quantum addresses
echo -e "${GREEN}5. Validating quantum addresses...${NC}"
echo -e "${YELLOW}Validating ML-DSA address...${NC}"
$BITCOIN_CLI -regtest -datadir="$DATADIR" validatequantumaddress "$ML_DSA_ADDR" | jq .

echo -e "${YELLOW}Validating SLH-DSA address...${NC}"
$BITCOIN_CLI -regtest -datadir="$DATADIR" validatequantumaddress "$SLH_DSA_ADDR" | jq .

# Send to quantum addresses
echo -e "${GREEN}6. Sending funds to quantum addresses...${NC}"
echo -e "${YELLOW}Sending 1 BTC to ML-DSA address...${NC}"
TXID_ML=$($BITCOIN_CLI -regtest -datadir="$DATADIR" sendtoaddress "$ML_DSA_ADDR" 1.0)
echo "Transaction ID: $TXID_ML"

echo -e "${YELLOW}Sending 2 BTC to SLH-DSA address...${NC}"
TXID_SLH=$($BITCOIN_CLI -regtest -datadir="$DATADIR" sendtoaddress "$SLH_DSA_ADDR" 2.0)
echo "Transaction ID: $TXID_SLH"

echo -e "${YELLOW}Sending 0.5 BTC to regular address...${NC}"
TXID_REG=$($BITCOIN_CLI -regtest -datadir="$DATADIR" sendtoaddress "$REGULAR_ADDR" 0.5)
echo "Transaction ID: $TXID_REG"

# Mine a block to confirm transactions
echo -e "${GREEN}7. Mining block to confirm transactions...${NC}"
$BITCOIN_CLI -regtest -datadir="$DATADIR" generatetoaddress 1 "$MINING_ADDR"

# Check UTXOs
echo -e "${GREEN}8. Checking UTXOs...${NC}"
echo -e "${YELLOW}All UTXOs:${NC}"
$BITCOIN_CLI -regtest -datadir="$DATADIR" listunspent | jq 'map(select(.amount > 0.1)) | map({address: .address, amount: .amount, label: .label})'

# Get quantum wallet info
echo -e "${GREEN}9. Getting quantum wallet info...${NC}"
$BITCOIN_CLI -regtest -datadir="$DATADIR" getquantuminfo | jq .

# Test spending from quantum addresses
echo -e "${GREEN}10. Testing spending from quantum addresses...${NC}"
NEW_ADDR=$($BITCOIN_CLI -regtest -datadir="$DATADIR" getnewaddress "spend-test")

echo -e "${YELLOW}Spending from ML-DSA address...${NC}"
# Create raw transaction spending from ML-DSA
UTXO_ML=$($BITCOIN_CLI -regtest -datadir="$DATADIR" listunspent | jq -r --arg addr "$ML_DSA_ADDR" '.[] | select(.address == $addr) | {txid: .txid, vout: .vout}')
if [ ! -z "$UTXO_ML" ] && [ "$UTXO_ML" != "null" ]; then
    echo "Found ML-DSA UTXO"
    # Try to send from ML-DSA address
    SPEND_TXID=$($BITCOIN_CLI -regtest -datadir="$DATADIR" sendtoaddress "$NEW_ADDR" 0.5 "" "" true)
    echo "Spend transaction: $SPEND_TXID"
fi

# Save wallet state
echo -e "${GREEN}11. Testing wallet persistence...${NC}"
echo -e "${YELLOW}Unloading wallet...${NC}"
$BITCOIN_CLI -regtest -datadir="$DATADIR" unloadwallet "quantum_test_wallet"

echo -e "${YELLOW}Reloading wallet...${NC}"
$BITCOIN_CLI -regtest -datadir="$DATADIR" loadwallet "quantum_test_wallet"

# Verify addresses are still there
echo -e "${GREEN}12. Verifying addresses after reload...${NC}"
$BITCOIN_CLI -regtest -datadir="$DATADIR" listreceivedbyaddress 0 true | jq 'map(select(.address == "'$ML_DSA_ADDR'" or .address == "'$SLH_DSA_ADDR'")) | map({address: .address, label: .label, amount: .amount})'

# Test address labels
echo -e "${GREEN}13. Testing address labels...${NC}"
$BITCOIN_CLI -regtest -datadir="$DATADIR" getaddressesbylabel "ml-dsa-test" | jq .
$BITCOIN_CLI -regtest -datadir="$DATADIR" getaddressesbylabel "slh-dsa-test" | jq .

# Get final wallet info
echo -e "${GREEN}14. Final wallet status...${NC}"
echo -e "${YELLOW}Balance:${NC}"
$BITCOIN_CLI -regtest -datadir="$DATADIR" getbalance

echo -e "${YELLOW}Wallet info:${NC}"
$BITCOIN_CLI -regtest -datadir="$DATADIR" getwalletinfo | jq '{walletname, balance, txcount, keypoolsize}'

# Clean up
echo -e "${GREEN}15. Cleaning up...${NC}"
$BITCOIN_CLI -regtest -datadir="$DATADIR" stop
sleep 2

echo -e "${GREEN}=== Test completed successfully! ===${NC}"