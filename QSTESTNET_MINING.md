# QSTestnet Mining Guide

This document explains how to properly mine on the QSBitcoin testnet network using actual Proof-of-Work.

## Overview

QSTestnet uses real Proof-of-Work mining with the following parameters:
- Algorithm: SHA256d (same as Bitcoin)
- Block time: 1 minute (reduced from Bitcoin's 10 minutes)
- Difficulty adjustment: Every 1,440 blocks (~1 day)
- Minimum difficulty blocks allowed: Yes (for testnet)

**Important**: The `generatetoaddress` command used in the bootstrap instructions is for development/regtest only and will NOT work for actual block mining on QSTestnet.

## Mining Methods

### Method 1: Using External Mining Software (Required)

#### Step 1: Set up your node for mining

Configure your node to accept RPC connections:

```bash
cat >> ~/.qsbitcoin/qstestnet/bitcoin.conf << EOF
# Mining RPC settings
rpcuser=qsminer
rpcpassword=strongpassword123
rpcallowip=127.0.0.1
rpcport=28332

# Enable RPC server
server=1
EOF
```

Restart your node:
```bash
./build/bin/bitcoin-cli -conf=~/.qsbitcoin/qstestnet/bitcoin.conf stop
./build/bin/bitcoind -conf=~/.qsbitcoin/qstestnet/bitcoin.conf -datadir=~/.qsbitcoin/qstestnet
```

#### Step 2: Create a mining address

```bash
# Create a wallet if you haven't already
./build/bin/bitcoin-cli -conf=~/.qsbitcoin/qstestnet/bitcoin.conf createwallet "miner"

# Generate a mining address (using standard ECDSA for efficiency)
MINING_ADDR=$(./build/bin/bitcoin-cli -conf=~/.qsbitcoin/qstestnet/bitcoin.conf getnewaddress "mining" "bech32")
echo "Mining address: $MINING_ADDR"
```

#### Step 3: Install mining software

**Option A: BFGMiner (CPU/GPU)**
```bash
# Install dependencies
sudo apt-get install build-essential autoconf automake libtool pkg-config libcurl4-openssl-dev libjansson-dev

# Clone and build BFGMiner
git clone https://github.com/luke-jr/bfgminer.git
cd bfgminer
./autogen.sh
./configure --enable-cpumining
make
```

**Option B: CGMiner (GPU focused)**
```bash
# Install dependencies
sudo apt-get install build-essential autoconf automake libtool pkg-config libcurl4-openssl-dev libssl-dev libjansson-dev

# Clone and build CGMiner
git clone https://github.com/ckolivas/cgminer.git
cd cgminer
./autogen.sh
./configure --enable-cpumining
make
```

**Option C: CPUMiner (CPU only)**
```bash
# Install dependencies
sudo apt-get install build-essential libcurl4-openssl-dev libjansson-dev

# Clone and build cpuminer
git clone https://github.com/pooler/cpuminer.git
cd cpuminer
./autogen.sh
./configure
make
```

#### Step 4: Start mining

**Using BFGMiner:**
```bash
./bfgminer \
  -o http://127.0.0.1:28332 \
  -u qsminer \
  -p strongpassword123 \
  --coinbase-addr=$MINING_ADDR \
  -S cpu:auto
```

**Using CPUMiner:**
```bash
./minerd \
  -o http://127.0.0.1:28332 \
  -u qsminer \
  -p strongpassword123 \
  --coinbase-addr=$MINING_ADDR \
  -t $(nproc)  # Use all CPU threads
```

### Method 2: Using getblocktemplate API (Advanced)

For custom mining implementations, use the `getblocktemplate` RPC:

```bash
# Get block template
TEMPLATE=$(./build/bin/bitcoin-cli -conf=~/.qsbitcoin/qstestnet/bitcoin.conf getblocktemplate '{"rules": ["segwit"]}')

# Extract necessary fields
echo "$TEMPLATE" | jq '{
  version: .version,
  previousblockhash: .previousblockhash,
  target: .target,
  bits: .bits,
  height: .height,
  curtime: .curtime
}'
```

Then implement the mining loop:
1. Construct block header with coinbase transaction
2. Increment nonce and hash with SHA256d
3. Check if hash meets target difficulty
4. Submit with `submitblock` RPC when found

### Method 3: Mining Pool Setup

For collaborative mining on QSTestnet:

1. **Pool Operator**: Set up pool software like CKPool or P2Pool
2. **Miners**: Connect to pool using stratum protocol

Example stratum connection:
```bash
./bfgminer -o stratum+tcp://pool.qstestnet.example:3333 -u YourWorkerName -p x
```

## Monitoring Mining Progress

Check mining status:
```bash
# Get mining info
./build/bin/bitcoin-cli -conf=~/.qsbitcoin/qstestnet/bitcoin.conf getmininginfo

# Monitor block generation
watch -n 10 './build/bin/bitcoin-cli -conf=~/.qsbitcoin/qstestnet/bitcoin.conf getblockcount'

# Check network hash rate
./build/bin/bitcoin-cli -conf=~/.qsbitcoin/qstestnet/bitcoin.conf getnetworkhashps

# View recent blocks
./build/bin/bitcoin-cli -conf=~/.qsbitcoin/qstestnet/bitcoin.conf getblockchaininfo | jq '.blocks, .difficulty'
```

## Initial Network Bootstrap (First Miner Only)

When starting the QSTestnet network for the very first time, the first miner needs to mine the initial blocks:

1. Start your node as the first network node
2. Set up mining as described above
3. Mine at least 101 blocks before coins become spendable (approximately 1.7 hours with 1-minute blocks)
4. Other nodes can then join and sync the blockchain

## Difficulty Adjustment

- QSTestnet allows minimum difficulty blocks after 20 minutes of no blocks
- This prevents the network from stalling if mining power drops
- Normal difficulty adjustment occurs every 1,440 blocks (exactly 1 day with 1-minute blocks)

## Performance Tips

1. **CPU Mining**: Use all available cores with `-t $(nproc)`
2. **Cooling**: Ensure adequate cooling for sustained mining
3. **Power**: Monitor power consumption, especially for 24/7 mining
4. **Optimization**: Compile mining software with `-march=native` for CPU-specific optimizations

## Troubleshooting

### "Method not found" error
- The `generate` and `setgenerate` commands are deprecated
- Use external mining software instead

### No blocks being found
- Check if difficulty is too high: `getmininginfo`
- Verify your mining software is properly connected
- Ensure your node is synced and connected to peers

### Connection refused
- Check RPC settings in bitcoin.conf
- Verify firewall allows connections on RPC port
- Ensure bitcoind is running

## Security Notes

1. Use strong RPC passwords
2. Limit RPC access to localhost only
3. Consider using RPC SSL for remote mining
4. Regularly update mining software

## Energy Efficiency

For testnet mining, consider:
- Mining only when needed for testing
- Using energy-efficient hardware
- Coordinating with other testnet participants
- Implementing mining schedules