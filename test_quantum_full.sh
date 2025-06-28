#!/bin/bash
# Full test script for quantum signature implementation using bitcoin-cli

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

echo "=== Full Quantum Signature Implementation Test ==="
echo "Testing both ML-DSA and SLH-DSA transactions"
echo

# Clean up any previous test data
echo "Cleaning up previous test data..."
./build/bin/bitcoin-cli -regtest stop 2>/dev/null
sleep 2
rm -rf ~/.bitcoin/regtest/wallets/*

# Start bitcoind in regtest mode with debug logging
echo "Starting bitcoind..."
./build/bin/bitcoind -regtest -daemon -fallbackfee=0.00001 -debug=net -debug=mempool -debug=validation
sleep 3

# Verify bitcoind is running
if ! ./build/bin/bitcoin-cli -regtest getblockcount >/dev/null 2>&1; then
    echo -e "${RED}Failed to start bitcoind${NC}"
    exit 1
fi

# Create wallet
echo "Creating test wallet..."
./build/bin/bitcoin-cli -regtest createwallet "test_wallet" || {
    echo -e "${RED}Failed to create wallet${NC}"
    ./build/bin/bitcoin-cli -regtest stop
    exit 1
}

# Generate initial blocks
echo "Generating initial blocks..."
INITIAL_ADDR=$(./build/bin/bitcoin-cli -regtest getnewaddress)
./build/bin/bitcoin-cli -regtest generatetoaddress 101 "$INITIAL_ADDR" > /dev/null

# Get wallet balance
BALANCE=$(./build/bin/bitcoin-cli -regtest getbalance)
echo "Initial wallet balance: $BALANCE BTC"

# Test 1: ML-DSA Address Generation and Transaction
echo -e "\n${YELLOW}=== Test 1: ML-DSA (Dilithium) ====${NC}"
echo "Generating ML-DSA address..."
ML_DSA_ADDR=$(./build/bin/bitcoin-cli -regtest getnewquantumaddress "" "ml-dsa")
echo "ML-DSA address: $ML_DSA_ADDR"

# Validate the ML-DSA address
echo "Validating ML-DSA address..."
./build/bin/bitcoin-cli -regtest validatequantumaddress "$ML_DSA_ADDR" | jq '.'

# Send to ML-DSA address
echo -e "\nSending 1 BTC to ML-DSA address..."
ML_TXID=$(./build/bin/bitcoin-cli -regtest sendtoaddress "$ML_DSA_ADDR" 1.0 2>&1)
if [[ $ML_TXID == *"error"* ]]; then
    echo -e "${RED}✗ Failed to send to ML-DSA address:${NC}"
    echo "$ML_TXID"
else
    echo -e "${GREEN}✓ Successfully sent to ML-DSA address${NC}"
    echo "Transaction ID: $ML_TXID"
    
    # Mine a block to confirm
    echo "Mining block to confirm transaction..."
    ./build/bin/bitcoin-cli -regtest generatetoaddress 1 "$INITIAL_ADDR" > /dev/null
    
    # Check transaction details
    echo "Transaction details:"
    ./build/bin/bitcoin-cli -regtest gettransaction "$ML_TXID" | jq '.confirmations, .details[0]'
fi

# Test 2: SLH-DSA Address Generation and Transaction
echo -e "\n${YELLOW}=== Test 2: SLH-DSA (SPHINCS+) ====${NC}"
echo "Generating SLH-DSA address..."
SLH_DSA_ADDR=$(./build/bin/bitcoin-cli -regtest getnewquantumaddress "" "slh-dsa")
echo "SLH-DSA address: $SLH_DSA_ADDR"

# Validate the SLH-DSA address
echo "Validating SLH-DSA address..."
./build/bin/bitcoin-cli -regtest validatequantumaddress "$SLH_DSA_ADDR" | jq '.'

# Send to SLH-DSA address
echo -e "\nSending 2 BTC to SLH-DSA address..."
SLH_TXID=$(./build/bin/bitcoin-cli -regtest sendtoaddress "$SLH_DSA_ADDR" 2.0 2>&1)
if [[ $SLH_TXID == *"error"* ]]; then
    echo -e "${RED}✗ Failed to send to SLH-DSA address:${NC}"
    echo "$SLH_TXID"
    
    # Check if bitcoind is still running
    if ! ./build/bin/bitcoin-cli -regtest getblockcount >/dev/null 2>&1; then
        echo -e "${RED}bitcoind has crashed!${NC}"
        echo "Checking debug log for errors..."
        grep -i "error\|assertion\|fault\|quantum\|commitment" ~/.bitcoin/regtest/debug.log | tail -20
    fi
else
    echo -e "${GREEN}✓ Successfully sent to SLH-DSA address${NC}"
    echo "Transaction ID: $SLH_TXID"
    
    # Mine a block to confirm
    echo "Mining block to confirm transaction..."
    ./build/bin/bitcoin-cli -regtest generatetoaddress 1 "$INITIAL_ADDR" > /dev/null
    
    # Check transaction details
    echo "Transaction details:"
    ./build/bin/bitcoin-cli -regtest gettransaction "$SLH_TXID" | jq '.confirmations, .details[0]'
fi

# Test 3: Spending from Quantum Addresses
echo -e "\n${YELLOW}=== Test 3: Spending from Quantum Addresses ====${NC}"

# Check if we have quantum UTXOs
echo "Listing unspent quantum outputs..."
QUANTUM_UTXOS=$(./build/bin/bitcoin-cli -regtest listunspent | jq '[.[] | select(.address | startswith("Q"))]')
echo "$QUANTUM_UTXOS" | jq -r '.[] | "Address: \(.address) Amount: \(.amount) BTC"'

# Try to spend from ML-DSA address if we have balance
ML_BALANCE=$(echo "$QUANTUM_UTXOS" | jq '[.[] | select(.address == "'"$ML_DSA_ADDR"'") | .amount] | add // 0')
if (( $(echo "$ML_BALANCE > 0" | bc -l) )); then
    echo -e "\nSpending from ML-DSA address (balance: $ML_BALANCE BTC)..."
    SPEND_ADDR=$(./build/bin/bitcoin-cli -regtest getnewaddress)
    SPEND_AMOUNT=$(echo "$ML_BALANCE - 0.0001" | bc)
    
    # Create raw transaction spending from ML-DSA
    ML_UTXO=$(echo "$QUANTUM_UTXOS" | jq -r '[.[] | select(.address == "'"$ML_DSA_ADDR"'")] | .[0]')
    ML_TXID_IN=$(echo "$ML_UTXO" | jq -r '.txid')
    ML_VOUT=$(echo "$ML_UTXO" | jq -r '.vout')
    
    RAW_TX=$(./build/bin/bitcoin-cli -regtest createrawtransaction "[{\"txid\":\"$ML_TXID_IN\",\"vout\":$ML_VOUT}]" "{\"$SPEND_ADDR\":$SPEND_AMOUNT}")
    SIGNED_TX=$(./build/bin/bitcoin-cli -regtest signrawtransactionwithwallet "$RAW_TX" | jq -r '.hex')
    
    if [ "$SIGNED_TX" != "null" ]; then
        SPEND_TXID=$(./build/bin/bitcoin-cli -regtest sendrawtransaction "$SIGNED_TX" 2>&1)
        if [[ $SPEND_TXID == *"error"* ]]; then
            echo -e "${RED}✗ Failed to spend from ML-DSA address:${NC}"
            echo "$SPEND_TXID"
        else
            echo -e "${GREEN}✓ Successfully spent from ML-DSA address${NC}"
            echo "Spend transaction ID: $SPEND_TXID"
        fi
    else
        echo -e "${RED}✗ Failed to sign transaction from ML-DSA address${NC}"
    fi
fi

# Try to spend from SLH-DSA address if we have balance
SLH_BALANCE=$(echo "$QUANTUM_UTXOS" | jq '[.[] | select(.address == "'"$SLH_DSA_ADDR"'") | .amount] | add // 0')
if (( $(echo "$SLH_BALANCE > 0" | bc -l) )); then
    echo -e "\nSpending from SLH-DSA address (balance: $SLH_BALANCE BTC)..."
    SPEND_ADDR2=$(./build/bin/bitcoin-cli -regtest getnewaddress)
    SPEND_AMOUNT2=$(echo "$SLH_BALANCE - 0.0001" | bc)
    
    # Create raw transaction spending from SLH-DSA
    SLH_UTXO=$(echo "$QUANTUM_UTXOS" | jq -r '[.[] | select(.address == "'"$SLH_DSA_ADDR"'")] | .[0]')
    SLH_TXID_IN=$(echo "$SLH_UTXO" | jq -r '.txid')
    SLH_VOUT=$(echo "$SLH_UTXO" | jq -r '.vout')
    
    RAW_TX2=$(./build/bin/bitcoin-cli -regtest createrawtransaction "[{\"txid\":\"$SLH_TXID_IN\",\"vout\":$SLH_VOUT}]" "{\"$SPEND_ADDR2\":$SPEND_AMOUNT2}")
    SIGNED_TX2=$(./build/bin/bitcoin-cli -regtest signrawtransactionwithwallet "$RAW_TX2" | jq -r '.hex')
    
    if [ "$SIGNED_TX2" != "null" ]; then
        SPEND_TXID2=$(./build/bin/bitcoin-cli -regtest sendrawtransaction "$SIGNED_TX2" 2>&1)
        if [[ $SPEND_TXID2 == *"error"* ]]; then
            echo -e "${RED}✗ Failed to spend from SLH-DSA address:${NC}"
            echo "$SPEND_TXID2"
        else
            echo -e "${GREEN}✓ Successfully spent from SLH-DSA address${NC}"
            echo "Spend transaction ID: $SPEND_TXID2"
            echo -e "${GREEN}Note: SLH-DSA transaction used commitment scheme for large signature${NC}"
        fi
    else
        echo -e "${RED}✗ Failed to sign transaction from SLH-DSA address${NC}"
    fi
fi

# Test 4: Get quantum wallet information
echo -e "\n${YELLOW}=== Test 4: Quantum Wallet Information ====${NC}"
echo "Getting quantum wallet info..."
./build/bin/bitcoin-cli -regtest getquantuminfo | jq '.'

# Test 5: Message signing with quantum algorithms
echo -e "\n${YELLOW}=== Test 5: Message Signing ====${NC}"
TEST_MESSAGE="Hello Quantum Bitcoin!"

if [ -n "$ML_DSA_ADDR" ]; then
    echo "Signing message with ML-DSA..."
    ML_SIG=$(./build/bin/bitcoin-cli -regtest signmessagewithscheme "$ML_DSA_ADDR" "$TEST_MESSAGE" "ml-dsa" 2>&1)
    if [[ $ML_SIG == *"error"* ]]; then
        echo -e "${RED}✗ Failed to sign with ML-DSA:${NC}"
        echo "$ML_SIG"
    else
        echo -e "${GREEN}✓ Successfully signed with ML-DSA${NC}"
        echo "$ML_SIG" | jq '.'
    fi
fi

if [ -n "$SLH_DSA_ADDR" ]; then
    echo -e "\nSigning message with SLH-DSA..."
    SLH_SIG=$(./build/bin/bitcoin-cli -regtest signmessagewithscheme "$SLH_DSA_ADDR" "$TEST_MESSAGE" "slh-dsa" 2>&1)
    if [[ $SLH_SIG == *"error"* ]]; then
        echo -e "${RED}✗ Failed to sign with SLH-DSA:${NC}"
        echo "$SLH_SIG"
    else
        echo -e "${GREEN}✓ Successfully signed with SLH-DSA${NC}"
        echo "$SLH_SIG" | jq '.'
    fi
fi

# Final summary
echo -e "\n${YELLOW}=== Test Summary ====${NC}"
echo "Final wallet balance: $(./build/bin/bitcoin-cli -regtest getbalance) BTC"
echo "Total blocks: $(./build/bin/bitcoin-cli -regtest getblockcount)"

# Check for any quantum-related log entries
echo -e "\n${YELLOW}=== Quantum-related log entries ====${NC}"
grep -i "quantum\|commitment\|ml-dsa\|slh-dsa" ~/.bitcoin/regtest/debug.log | tail -10

# Clean up
echo -e "\nStopping bitcoind..."
./build/bin/bitcoin-cli -regtest stop

echo -e "\n${GREEN}Test completed!${NC}"