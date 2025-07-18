#!/bin/bash

# QSTestnet Mining Monitor
# This script monitors the mining progress on QSTestnet

set -e

# Configuration
QSBITCOIN_DIR="/home/user/work/bitcoin"
CONFIG_FILE="$HOME/.qsbitcoin/qstestnet/bitcoin.conf"

# Colors - using printf format
GREEN=$'\033[0;32m'
YELLOW=$'\033[1;33m'
BLUE=$'\033[0;34m'
NC=$'\033[0m'

clear

echo "${GREEN}QSTestnet Mining Monitor${NC}"
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
        echo "${YELLOW}Network:${NC} $CHAIN"
        echo "${YELLOW}Blocks:${NC} $(format_number $BLOCKS)"
        echo "${YELLOW}Difficulty:${NC} $DIFFICULTY"
        echo "${YELLOW}Network Hashrate:${NC} $(format_number ${HASHRATE%.*}) H/s"
        echo "${YELLOW}Mempool:${NC} $POOLED_TX transactions"
        echo "${YELLOW}Peers:${NC} $PEERS"
        
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
                
                echo ""
                echo "${BLUE}Last Block:${NC}"
                echo "  Time: ${MINUTES}m ${SECONDS}s ago"
                echo "  Size: $(format_number $BLOCK_SIZE) bytes"
                echo "  Transactions: $BLOCK_TX"
            fi
        fi
        
        # Check if mining (look for cpuminer process)
        if pgrep -f "minerd.*qstestnet" > /dev/null; then
            echo ""
            echo "${GREEN}✓ Mining is active${NC}"
            
            # Try to get miner's balance
            BALANCE=$("$QSBITCOIN_DIR/build/bin/bitcoin-cli" -conf="$CONFIG_FILE" -rpcwallet=miner getbalance 2>/dev/null || echo "0")
            UNCONFIRMED=$("$QSBITCOIN_DIR/build/bin/bitcoin-cli" -conf="$CONFIG_FILE" -rpcwallet=miner getunconfirmedbalance 2>/dev/null || echo "0")
            BALANCES=$("$QSBITCOIN_DIR/build/bin/bitcoin-cli" -conf="$CONFIG_FILE" -rpcwallet=miner getbalances 2>/dev/null || echo "{}")
            
            if [ "$BALANCES" != "{}" ]; then
                SPENDABLE=$(echo "$BALANCES" | jq -r '.mine.trusted // 0')
                IMMATURE=$(echo "$BALANCES" | jq -r '.mine.immature // 0')
                PENDING=$(echo "$BALANCES" | jq -r '.mine.untrusted_pending // 0')
                
                echo "${YELLOW}Miner Balance:${NC}"
                echo "  Spendable: $SPENDABLE BTC"
                echo "  Immature: $IMMATURE BTC (needs 100 confirmations)"
                if [ "$PENDING" != "0" ]; then
                    echo "  Pending: $PENDING BTC"
                fi
            else
                echo "${YELLOW}Miner Balance:${NC} $BALANCE BTC"
            fi
        else
            echo ""
            echo "${YELLOW}⚠ Mining is not active${NC}"
            echo "Run ./start_qstestnet_mining.sh to start mining"
        fi
    else
        echo "${YELLOW}Waiting for bitcoind connection...${NC}"
    fi
    
    sleep 5
done