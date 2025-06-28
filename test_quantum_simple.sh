#!/bin/bash
# Simple test for quantum addresses

echo "Starting bitcoind..."
./build/bin/bitcoind -regtest -daemon -fallbackfee=0.00001
sleep 3

# Clean up any existing wallet
./build/bin/bitcoin-cli -regtest unloadwallet "test_wallet" 2>/dev/null
rm -rf ~/.bitcoin/regtest/wallets/test_wallet

echo "Creating wallet..."
./build/bin/bitcoin-cli -regtest createwallet "test_wallet"

echo "Generating blocks..."
ADDR=$(./build/bin/bitcoin-cli -regtest getnewaddress)
./build/bin/bitcoin-cli -regtest generatetoaddress 101 "$ADDR" > /dev/null

echo -e "\nTesting ML-DSA address..."
ML_ADDR=$(./build/bin/bitcoin-cli -regtest getnewquantumaddress "" "ml-dsa")
echo "ML-DSA address: $ML_ADDR"

# Check the prefix
if [[ $ML_ADDR == Q1* ]]; then
    echo "✓ ML-DSA address has correct Q1 prefix"
else
    echo "✗ ML-DSA address has wrong prefix"
fi

# Validate the address
echo "Validating ML-DSA address..."
./build/bin/bitcoin-cli -regtest validatequantumaddress "$ML_ADDR"

# Try to decode it as a regular address (should fail)
echo -e "\nTrying to validate as regular address (should fail)..."
./build/bin/bitcoin-cli -regtest validateaddress "$ML_ADDR"

# Now test SLH-DSA
echo -e "\nTesting SLH-DSA address..."
SLH_ADDR=$(./build/bin/bitcoin-cli -regtest getnewquantumaddress "" "slh-dsa")
echo "SLH-DSA address: $SLH_ADDR"

# Check the prefix
if [[ $SLH_ADDR == Q2* ]]; then
    echo "✓ SLH-DSA address has correct Q2 prefix"
else
    echo "✗ SLH-DSA address has wrong prefix"
fi

# Validate the address
echo "Validating SLH-DSA address..."
./build/bin/bitcoin-cli -regtest validatequantumaddress "$SLH_ADDR"

# Test creating a raw transaction with quantum addresses
echo -e "\nTesting raw transaction creation with quantum addresses..."
UTXO=$(./build/bin/bitcoin-cli -regtest listunspent 1 | jq -r '.[0]')
TXID=$(echo $UTXO | jq -r '.txid')
VOUT=$(echo $UTXO | jq -r '.vout')
AMOUNT=$(echo $UTXO | jq -r '.amount')

# Create outputs with both regular and quantum addresses
OUTPUTS="{\"$ADDR\":1.0,\"$ML_ADDR\":1.0,\"$SLH_ADDR\":1.0}"
echo "Creating raw transaction with outputs: $OUTPUTS"

RAW_TX=$(./build/bin/bitcoin-cli -regtest createrawtransaction "[{\"txid\":\"$TXID\",\"vout\":$VOUT}]" "$OUTPUTS" 2>&1)
if [[ $RAW_TX == *"error"* ]]; then
    echo "✗ Failed to create raw transaction:"
    echo "$RAW_TX"
else
    echo "✓ Successfully created raw transaction"
    
    # Decode to verify
    echo "Decoding transaction..."
    ./build/bin/bitcoin-cli -regtest decoderawtransaction "$RAW_TX" | jq '.vout[] | {value, scriptPubKey}'
fi

# Check quantum info
echo -e "\nQuantum wallet info:"
./build/bin/bitcoin-cli -regtest getquantuminfo

# Stop bitcoind
./build/bin/bitcoin-cli -regtest stop
echo "Test completed"