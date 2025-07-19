#!/bin/bash
# Test script for quantum GUI functionality

echo "Bitcoin Qt GUI Quantum Support Test"
echo "===================================="

# Clean start
pkill -f "bitcoin-qt.*regtest" || true
rm -rf ~/.bitcoin/regtest/

# Start bitcoin-qt in regtest mode
echo "Starting bitcoin-qt with regtest..."
./build/bin/bitcoin-qt -regtest -fallbackfee=0.00001 &
BITCOIN_QT_PID=$!

# Wait for GUI to start
sleep 5

echo ""
echo "Bitcoin Qt GUI is now running with quantum support!"
echo ""
echo "To test quantum address generation:"
echo "1. Go to 'Receive' tab"
echo "2. You'll see a new 'Algorithm' dropdown with options:"
echo "   - Standard (ECDSA)"
echo "   - Quantum-Safe (ML-DSA)"
echo "   - Quantum-Safe High Security (SLH-DSA)"
echo "3. Select a quantum algorithm and click 'Create new receiving address'"
echo "4. The address type will automatically switch to Bech32 for quantum addresses"
echo ""
echo "To view quantum signature details in transactions:"
echo "1. Send some coins to a quantum address"
echo "2. In the transaction history, click on a transaction"
echo "3. You'll see 'Signature Algorithm' field showing the quantum algorithm used"
echo ""
echo "Press Ctrl+C to stop the test..."

# Wait for user to test
wait $BITCOIN_QT_PID

echo "Test completed."