#!/bin/bash

# QSTestnet Mining Script
# This script sets up and starts CPU mining for QSTestnet

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Configuration
QSBITCOIN_DIR="/home/user/work/bitcoin"
CPUMINER_PATH="/home/user/mining/cpuminer/minerd"
CONFIG_FILE="$HOME/.qsbitcoin/qstestnet/bitcoin.conf"
DATA_DIR="$HOME/.qsbitcoin/qstestnet"

# Default values
RPC_PORT=28332
RPC_USER="qstestnet"
RPC_PASS="qstestnetpass"
THREADS=$(nproc)

echo -e "${GREEN}QSTestnet Mining Setup${NC}"
echo "========================"

# Check if cpuminer exists
if [ ! -f "$CPUMINER_PATH" ]; then
    echo -e "${RED}Error: cpuminer not found at $CPUMINER_PATH${NC}"
    echo "Please build cpuminer first:"
    echo "  cd ~/mining/cpuminer && ./autogen.sh && ./configure && make"
    exit 1
fi

# Check if bitcoind is running
if ! pgrep -f "bitcoind.*qstestnet" > /dev/null; then
    echo -e "${YELLOW}QSTestnet bitcoind is not running. Starting it...${NC}"
    
    # Create config directory if it doesn't exist
    mkdir -p "$DATA_DIR"
    
    # Start bitcoind
    "$QSBITCOIN_DIR/build/bin/bitcoind" -conf="$CONFIG_FILE" -datadir="$DATA_DIR" -daemon
    
    echo "Waiting for bitcoind to start..."
    sleep 5
    
    # Check if started successfully
    if ! "$QSBITCOIN_DIR/build/bin/bitcoin-cli" -conf="$CONFIG_FILE" getblockchaininfo >/dev/null 2>&1; then
        echo -e "${RED}Failed to start bitcoind. Check the logs at $DATA_DIR/debug.log${NC}"
        exit 1
    fi
fi

# Check if wallet exists, create if not
if ! "$QSBITCOIN_DIR/build/bin/bitcoin-cli" -conf="$CONFIG_FILE" listwallets | grep -q "miner"; then
    echo "Creating miner wallet..."
    "$QSBITCOIN_DIR/build/bin/bitcoin-cli" -conf="$CONFIG_FILE" createwallet "miner" >/dev/null 2>&1 || {
        echo -e "${YELLOW}Wallet may already exist, trying to load it...${NC}"
        "$QSBITCOIN_DIR/build/bin/bitcoin-cli" -conf="$CONFIG_FILE" loadwallet "miner" >/dev/null 2>&1 || true
    }
fi

# Get or create mining address
echo "Getting mining address..."
MINING_ADDR=$("$QSBITCOIN_DIR/build/bin/bitcoin-cli" -conf="$CONFIG_FILE" -rpcwallet=miner getnewaddress "mining" "bech32")
echo -e "${GREEN}Mining address: $MINING_ADDR${NC}"

# Display current network info
printf "\n${YELLOW}Network Status:${NC}\n"
BLOCK_COUNT=$("$QSBITCOIN_DIR/build/bin/bitcoin-cli" -conf="$CONFIG_FILE" getblockcount)
DIFFICULTY=$("$QSBITCOIN_DIR/build/bin/bitcoin-cli" -conf="$CONFIG_FILE" getdifficulty)
CONNECTIONS=$("$QSBITCOIN_DIR/build/bin/bitcoin-cli" -conf="$CONFIG_FILE" getconnectioncount)
echo "  Blocks: $BLOCK_COUNT"
echo "  Difficulty: $DIFFICULTY"
echo "  Connections: $CONNECTIONS"

# Mining options
printf "\n${YELLOW}Mining Configuration:${NC}\n"
echo "  Algorithm: SHA256d"
echo "  Threads: $THREADS"
echo "  RPC URL: http://127.0.0.1:$RPC_PORT"

# Ask user for thread count
printf "Number of CPU threads to use for mining [default: %s]: " "$THREADS"
read USER_THREADS
if [ -n "$USER_THREADS" ]; then
    THREADS=$USER_THREADS
fi

# Start mining
printf "\n${GREEN}Starting CPU mining with $THREADS threads...${NC}\n"
echo "Press Ctrl+C to stop mining"
echo ""

# Run cpuminer
"$CPUMINER_PATH" \
    -a sha256d \
    -o "http://127.0.0.1:$RPC_PORT" \
    -u "$RPC_USER" \
    -p "$RPC_PASS" \
    --coinbase-addr="$MINING_ADDR" \
    -t "$THREADS" \
    --no-longpoll \
    -P

# Note: -P shows hashrate periodically
# --no-longpoll is used because QSTestnet might not support longpoll