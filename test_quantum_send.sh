#!/bin/bash
# Test sending to quantum addresses

echo "Starting bitcoind..."
./build/bin/bitcoind -regtest -daemon -fallbackfee=0.00001
sleep 3

# Clean up and create wallet
./build/bin/bitcoin-cli -regtest unloadwallet "test_wallet" 2>/dev/null
rm -rf ~/.bitcoin/regtest/wallets/test_wallet
./build/bin/bitcoin-cli -regtest createwallet "test_wallet"

# Generate blocks
echo "Generating blocks..."
ADDR=$(./build/bin/bitcoin-cli -regtest getnewaddress)
./build/bin/bitcoin-cli -regtest generatetoaddress 101 "$ADDR" > /dev/null

# Get quantum addresses
ML_ADDR=$(./build/bin/bitcoin-cli -regtest getnewquantumaddress "" "ml-dsa")
SLH_ADDR=$(./build/bin/bitcoin-cli -regtest getnewquantumaddress "" "slh-dsa")

echo "ML-DSA address: $ML_ADDR"
echo "SLH-DSA address: $SLH_ADDR"

# Test sending to ML-DSA
echo -e "\nSending 1 BTC to ML-DSA address..."
ML_TXID=$(./build/bin/bitcoin-cli -regtest sendtoaddress "$ML_ADDR" 1.0 2>&1)
if [[ $ML_TXID == *"error"* ]]; then
    echo "✗ Failed to send to ML-DSA:"
    echo "$ML_TXID"
    
    # Try with the non-Q version
    ML_ADDR_NO_Q=$(echo "$ML_ADDR" | sed 's/^Q1/m/')
    echo "Trying with non-Q version: $ML_ADDR_NO_Q"
    ML_TXID=$(./build/bin/bitcoin-cli -regtest sendtoaddress "$ML_ADDR_NO_Q" 1.0 2>&1)
    if [[ $ML_TXID == *"error"* ]]; then
        echo "✗ Also failed with non-Q version"
    else
        echo "✓ Success with non-Q version: $ML_TXID"
    fi
else
    echo "✓ Success: $ML_TXID"
fi

# Check if bitcoind is still running
if ./build/bin/bitcoin-cli -regtest getblockcount >/dev/null 2>&1; then
    echo "bitcoind is still running"
    
    # Test sending to SLH-DSA
    echo -e "\nSending 2 BTC to SLH-DSA address..."
    SLH_TXID=$(./build/bin/bitcoin-cli -regtest sendtoaddress "$SLH_ADDR" 2.0 2>&1)
    if [[ $SLH_TXID == *"error"* ]]; then
        echo "✗ Failed to send to SLH-DSA:"
        echo "$SLH_TXID"
        
        # Check if bitcoind crashed
        if ! ./build/bin/bitcoin-cli -regtest getblockcount >/dev/null 2>&1; then
            echo "✗ bitcoind crashed!"
        fi
    else
        echo "✓ Success: $SLH_TXID"
        
        # Mine a block
        echo "Mining block..."
        ./build/bin/bitcoin-cli -regtest generatetoaddress 1 "$ADDR" > /dev/null
        
        # Check transaction
        echo "Transaction details:"
        ./build/bin/bitcoin-cli -regtest gettransaction "$SLH_TXID" | jq '.confirmations'
    fi
else
    echo "✗ bitcoind has crashed!"
fi

# Check the last few debug log entries
echo -e "\nLast quantum-related log entries:"
grep -i "quantum\|commitment\|slh-dsa\|ml-dsa" ~/.bitcoin/regtest/debug.log | tail -10

# Stop bitcoind if running
./build/bin/bitcoin-cli -regtest stop 2>/dev/null