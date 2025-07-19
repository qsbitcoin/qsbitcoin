#!/bin/bash

# QSTestnet startup script
# This script sets up and starts a QSBitcoin testnet node

set -e

# Get the machine's IP address
CURRENT_IP=$(hostname -I | awk '{print $1}')
echo "Starting QSTestnet node on IP: $CURRENT_IP"

# Paths
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
DATA_DIR="$HOME/.qsbitcoin/qstestnet"
CONF_FILE="$DATA_DIR/bitcoin.conf"

# Check if build exists
if [ ! -f "$BUILD_DIR/bin/bitcoind" ]; then
    echo "Error: bitcoind not found. Please build first with:"
    echo "  cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release"
    echo "  ninja -C build -j$(nproc)"
    exit 1
fi

# Create data directory
mkdir -p "$DATA_DIR"

# Generate configuration
echo "Generating configuration..."
cat > "$CONF_FILE" << EOF
# QSTestnet configuration
testnet=1
chain=qstestnet
server=1
listen=1
daemon=0  # Run in foreground for this script

# RPC settings
rpcuser=qstestnet
rpcpassword=qstestnetpass
rpcallowip=192.168.1.0/24
rpcbind=0.0.0.0

# Network settings
port=28333
maxconnections=100
fallbackfee=0.00001

# Logging
debug=net
debug=validation
debuglogfile=debug.log

# Connect to other testnet nodes
EOF

# Add peer connections (excluding self)
for ip in 192.168.1.102 192.168.1.103 192.168.1.104; do
    if [ "$ip" != "$CURRENT_IP" ]; then
        echo "addnode=$ip:28333" >> "$CONF_FILE"
    fi
done

echo "Configuration written to: $CONF_FILE"

# Check if this is the first node (192.168.1.102) and if blockchain exists
if [ "$CURRENT_IP" == "192.168.1.102" ] && [ ! -d "$DATA_DIR/blocks" ]; then
    echo ""
    echo "This appears to be the first node. After startup, you may want to:"
    echo "1. Create a wallet: ./build/bin/bitcoin-cli -conf=$CONF_FILE createwallet \"miner\""
    echo "2. Generate initial blocks: ./build/bin/bitcoin-cli -conf=$CONF_FILE generatetoaddress 101 \$(./build/bin/bitcoin-cli -conf=$CONF_FILE getnewaddress)"
fi

# Start the node
echo ""
echo "Starting QSTestnet node..."
echo "RPC interface will be available at: http://$CURRENT_IP:18332"
echo "P2P port: $CURRENT_IP:28333"
echo ""
echo "Useful commands:"
echo "  Check peers: ./build/bin/bitcoin-cli -conf=$CONF_FILE getpeerinfo"
echo "  Check info:  ./build/bin/bitcoin-cli -conf=$CONF_FILE getblockchaininfo"
echo "  Stop node:   ./build/bin/bitcoin-cli -conf=$CONF_FILE stop"
echo ""
echo "Press Ctrl+C to stop the node"
echo ""

# Start bitcoind
exec "$BUILD_DIR/bin/bitcoind" -conf="$CONF_FILE" -datadir="$DATA_DIR"