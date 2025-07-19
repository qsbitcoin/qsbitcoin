# QSTestnet Bootstrap Instructions

This document describes how to bootstrap and run the QSBitcoin testnet network across multiple nodes.

## Network Configuration

The QSTestnet is configured with the following seed nodes:
- Node 1: 192.168.1.102:28333
- Node 2: 192.168.1.103:28333  
- Node 3: 192.168.1.104:28333

## Starting a Testnet Node

### 1. Build QSBitcoin

On each machine, build the software:

```bash
# Clean build
rm -rf build/
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON
ninja -C build -j$(nproc)
```

### 2. Configure Node

Create a data directory and configuration file:

```bash
# Create testnet data directory
mkdir -p ~/.qsbitcoin/qstestnet

# Create configuration file
cat > ~/.qsbitcoin/qstestnet/bitcoin.conf << EOF
# QSTestnet configuration
testnet=1
chain=qstestnet
server=1
listen=1
daemon=1

# RPC settings
rpcuser=qstestnet
rpcpassword=qstestnetpass
rpcallowip=192.168.1.0/24
rpcbind=0.0.0.0

# Network settings
port=28333
maxconnections=100
fallbackfee=0.00001

# Mining settings (for generating blocks)
generatetoaddress=100

# Logging
debug=net
debug=validation
printtoconsole=1

# Explicitly connect to other nodes
addnode=192.168.1.102:28333
addnode=192.168.1.103:28333
addnode=192.168.1.104:28333
EOF
```

### 3. Start the Node

```bash
# Start bitcoind with QSTestnet
./build/bin/bitcoind -conf=~/.qsbitcoin/qstestnet/bitcoin.conf -datadir=~/.qsbitcoin/qstestnet

# Or use the provided startup script
./start_qstestnet.sh
```

### 4. Verify Connection

Check that your node is connected to the network:

```bash
./build/bin/bitcoin-cli -conf=~/.qsbitcoin/qstestnet/bitcoin.conf getpeerinfo
./build/bin/bitcoin-cli -conf=~/.qsbitcoin/qstestnet/bitcoin.conf getnetworkinfo
```

## Initial Network Bootstrap

When starting the network for the first time:

### On Node 1 (192.168.1.102):

1. Start the node first
2. Generate the initial blocks:

```bash
# Create a wallet
./build/bin/bitcoin-cli -conf=~/.qsbitcoin/qstestnet/bitcoin.conf createwallet "miner"

# Generate an address
ADDR=$(./build/bin/bitcoin-cli -conf=~/.qsbitcoin/qstestnet/bitcoin.conf getnewaddress)

# Mine initial blocks
./build/bin/bitcoin-cli -conf=~/.qsbitcoin/qstestnet/bitcoin.conf generatetoaddress 101 $ADDR
```

### On Nodes 2 & 3:

1. Start the nodes
2. They should automatically connect to Node 1 and sync the blockchain

## Testing Quantum Addresses

Once the network is running:

```bash
# Create quantum addresses
MLDSA=$(./build/bin/bitcoin-cli -conf=~/.qsbitcoin/qstestnet/bitcoin.conf getnewaddress "" "bech32" "ml-dsa")
SLHDSA=$(./build/bin/bitcoin-cli -conf=~/.qsbitcoin/qstestnet/bitcoin.conf getnewaddress "" "bech32" "slh-dsa")

echo "ML-DSA address: $MLDSA"
echo "SLH-DSA address: $SLHDSA"

# Send funds to quantum addresses
./build/bin/bitcoin-cli -conf=~/.qsbitcoin/qstestnet/bitcoin.conf sendtoaddress $MLDSA 10.0
./build/bin/bitcoin-cli -conf=~/.qsbitcoin/qstestnet/bitcoin.conf sendtoaddress $SLHDSA 10.0

# Mine to confirm
./build/bin/bitcoin-cli -conf=~/.qsbitcoin/qstestnet/bitcoin.conf generatetoaddress 6 $ADDR
```

## Network Monitoring

Monitor the network status:

```bash
# Check blockchain info
./build/bin/bitcoin-cli -conf=~/.qsbitcoin/qstestnet/bitcoin.conf getblockchaininfo

# Check peer connections
./build/bin/bitcoin-cli -conf=~/.qsbitcoin/qstestnet/bitcoin.conf getpeerinfo | grep -E "addr|subver|bytessent|bytesrecv"

# Check network info
./build/bin/bitcoin-cli -conf=~/.qsbitcoin/qstestnet/bitcoin.conf getnetworkinfo

# Monitor logs
tail -f ~/.qsbitcoin/qstestnet/debug.log
```

## Troubleshooting

### Nodes Not Connecting

1. Check firewall settings - port 28333 must be open
2. Verify IP addresses are correct
3. Check debug.log for connection errors

### Sync Issues

1. Ensure all nodes have the same genesis block
2. Check that quantum signatures are enabled (should be ALWAYS_ACTIVE on QSTestnet)
3. Verify network magic bytes match

### Clean Restart

If you need to restart the network from scratch:

```bash
# Stop the node
./build/bin/bitcoin-cli -conf=~/.qsbitcoin/qstestnet/bitcoin.conf stop

# Remove blockchain data (keeps wallet)
rm -rf ~/.qsbitcoin/qstestnet/blocks
rm -rf ~/.qsbitcoin/qstestnet/chainstate

# Restart
./build/bin/bitcoind -conf=~/.qsbitcoin/qstestnet/bitcoin.conf -datadir=~/.qsbitcoin/qstestnet
```