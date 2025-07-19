# QSBitcoin Testnet Quantum-Safe Addresses

## Node Information
- **Node IP Address**: 192.168.1.103
- **QSTestnet RPC Port**: 28332
- **QSTestnet P2P Port**: 28333
- **Network**: QSTestnet (Quantum-Safe Bitcoin Test Network)
- **Current Block Height**: 355
- **Wallet Name**: quantum_wallet

## Generated Quantum-Safe Addresses

### ML-DSA-65 Address
- **Algorithm**: ML-DSA-65 (Module Lattice Digital Signature Algorithm)
- **Address**: `qt1qkdp6fmapxcq43xvdezngrgy8gv067nfs5w2qzct6dzddg5v9qunqppfc38`
- **Signature Size**: ~3.3 KB
- **Use Case**: Standard transactions (recommended for 99% of cases)
- **Security Level**: NIST Level 3

### SLH-DSA-192f Address
- **Algorithm**: SLH-DSA-192f (Stateless Hash-Based Digital Signature Algorithm)
- **Address**: `qt1qlj6z4favjp9krkzqgsyx6qw92jzaxuzm3t2466azcel499ykwkhq2clwqx`
- **Signature Size**: ~35 KB
- **Use Case**: High-value cold storage
- **Security Level**: NIST Level 3

## Address Properties
- Both addresses use standard bech32 P2WSH encoding
- Prefix: `qt1` (QSTestnet bech32 prefix)
- These addresses are quantum-safe and resistant to attacks from quantum computers
- Can receive and spend funds on the QSTestnet network

## Usage Examples

### Send funds to ML-DSA address:
```bash
./build/bin/bitcoin-cli -conf="$HOME/.qsbitcoin/qstestnet/bitcoin.conf" sendtoaddress qt1qkdp6fmapxcq43xvdezngrgy8gv067nfs5w2qzct6dzddg5v9qunqppfc38 1.0
```

### Send funds to SLH-DSA address:
```bash
./build/bin/bitcoin-cli -conf="$HOME/.qsbitcoin/qstestnet/bitcoin.conf" sendtoaddress qt1qlj6z4favjp9krkzqgsyx6qw92jzaxuzm3t2466azcel499ykwkhq2clwqx 1.0
```

### Check quantum wallet info:
```bash
./build/bin/bitcoin-cli -conf="$HOME/.qsbitcoin/qstestnet/bitcoin.conf" getquantuminfo
```

Generated on: $(date)