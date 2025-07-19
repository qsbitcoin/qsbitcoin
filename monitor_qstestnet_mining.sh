#!/bin/bash

# QSTestnet Mining Monitor
# This script monitors the mining progress on QSTestnet

set -e

# Configuration
QSBITCOIN_DIR="/home/user/work/bitcoin"
CONFIG_FILE="$HOME/.qsbitcoin/qstestnet/bitcoin.conf"

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

clear

echo -e "${GREEN}QSTestnet Mining Monitor${NC}"
echo "========================"
echo "Press Ctrl+C to exit"
echo ""

# Function to format numbers with commas
format_number() {
    printf "%'d" $1
}

# Monitor loop
while true; do
    # Get blockchain info
    INFO=$("$QSBITCOIN_DIR/build/bin/bitcoin-cli" -conf="$CONFIG_FILE" getblockchaininfo 2>/dev/null || echo "{}")
    MINING_INFO=$("$QSBITCOIN_DIR/build/bin/bitcoin-cli" -conf="$CONFIG_FILE" getmininginfo 2>/dev/null || echo "{}")
    
    if [ "$INFO" != "{}" ]; then
        # Extract data
        BLOCKS=$(echo "$INFO" | jq -r '.blocks // 0')
        DIFFICULTY=$(echo "$INFO" | jq -r '.difficulty // 0')
        CHAIN=$(echo "$INFO" | jq -r '.chain // "unknown"')
        
        HASHRATE=$(echo "$MINING_INFO" | jq -r '.networkhashps // 0')
        POOLED_TX=$(echo "$MINING_INFO" | jq -r '.pooledtx // 0')
        
        # Get peer info
        PEERS=$("$QSBITCOIN_DIR/build/bin/bitcoin-cli" -conf="$CONFIG_FILE" getconnectioncount 2>/dev/null || echo "0")
        
        # Clear previous output (keep header)
        tput cup 4 0
        tput ed
        
        # Display info
        echo -e "${YELLOW}Network:${NC} $CHAIN"
        echo -e "${YELLOW}Blocks:${NC} $(format_number $BLOCKS)"
        echo -e "${YELLOW}Difficulty:${NC} $DIFFICULTY"
        echo -e "${YELLOW}Network Hashrate:${NC} $(format_number ${HASHRATE%.*}) H/s"
        echo -e "${YELLOW}Mempool:${NC} $POOLED_TX transactions"
        echo -e "${YELLOW}Peers:${NC} $PEERS"
        
        # Get last block info if available
        if [ "$BLOCKS" -gt "0" ]; then
            LAST_BLOCK=$("$QSBITCOIN_DIR/build/bin/bitcoin-cli" -conf="$CONFIG_FILE" getblock $("$QSBITCOIN_DIR/build/bin/bitcoin-cli" -conf="$CONFIG_FILE" getbestblockhash) 2>/dev/null || echo "{}")
            if [ "$LAST_BLOCK" != "{}" ]; then
                BLOCK_TIME=$(echo "$LAST_BLOCK" | jq -r '.time // 0')
                BLOCK_SIZE=$(echo "$LAST_BLOCK" | jq -r '.size // 0')
                BLOCK_TX=$(echo "$LAST_BLOCK" | jq -r '.nTx // 0')
                
                # Calculate time since last block
                CURRENT_TIME=$(date +%s)
                TIME_DIFF=$((CURRENT_TIME - BLOCK_TIME))
                MINUTES=$((TIME_DIFF / 60))
                SECONDS=$((TIME_DIFF % 60))
                
                echo -e "\n${BLUE}Last Block:${NC}"
                echo -e "  Time: ${MINUTES}m ${SECONDS}s ago"
                echo -e "  Size: $(format_number $BLOCK_SIZE) bytes"
                echo -e "  Transactions: $BLOCK_TX"
            fi
        fi
        
        # Check if mining (look for cpuminer process)
        if pgrep -f "minerd.*qstestnet" > /dev/null; then
            echo -e "\n${GREEN}✓ Mining is active${NC}"
            
            # Try to get miner's balance
            BALANCE=$("$QSBITCOIN_DIR/build/bin/bitcoin-cli" -conf="$CONFIG_FILE" -rpcwallet=miner getbalance 2>/dev/null || echo "0")
            if [ "$BALANCE" != "0" ]; then
                echo -e "${YELLOW}Miner Balance:${NC} $BALANCE BTC"
            fi
        else
            echo -e "\n${YELLOW}⚠ Mining is not active${NC}"
            echo "Run ./start_qstestnet_mining.sh to start mining"
        fi
    else
        echo -e "${YELLOW}Waiting for bitcoind connection...${NC}"
    fi
    
    sleep 5
done