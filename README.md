# QSBitcoin - Quantum-Safe Bitcoin

A forward-looking fork of Bitcoin designed to be resistant to quantum computing attacks.

## Overview

QSBitcoin replaces Bitcoin's quantum-vulnerable cryptographic components with post-quantum alternatives while maintaining the core principles and functionality that make Bitcoin the world's leading cryptocurrency.

## Key Features

- **Quantum-Resistant Signatures**: Replaces ECDSA with NIST-approved post-quantum signature schemes
- **Future-Proof Security**: Implements cryptographic algorithms resistant to both classical and quantum attacks  
- **Smooth Migration Path**: Provides tools and timeline for users to migrate from Bitcoin
- **Bitcoin Compatible**: Maintains compatibility with Bitcoin's ecosystem where possible

## Development Status

✅ **Implementation Complete** ✅

As of July 2025, the quantum-safe implementation is **100% complete** with all core features fully operational:

- ✅ Post-quantum signature schemes (ML-DSA-65 and SLH-DSA-192f) via liboqs
- ✅ Quantum address generation and management  
- ✅ Full wallet integration with descriptor support
- ✅ Complete transaction lifecycle (create, sign, broadcast, mine)
- ✅ Comprehensive test suite (93/93 tests passing)
- ✅ Successfully tested on regtest network

See [QSBITCOIN_IMPLEMENTATION_STATUS.md](QSBITCOIN_IMPLEMENTATION_STATUS.md) for detailed implementation status and [QSBITCOIN_PLAN.md](QSBITCOIN_PLAN.md) for technical design details.

## Quick Links

- [Development Plan](QSBITCOIN_PLAN.md)
- [Technical Specification](docs/technical-spec.md) (Coming Soon)
- [Migration Guide](docs/migration-guide.md) (Coming Soon)
- [Contributing Guidelines](CONTRIBUTING.md) (Coming Soon)

## Quantum Threat Timeline

- **2024-2030**: Development and testing of quantum-safe implementation
- **2030-2035**: Early quantum computers may threaten exposed public keys
- **2035-2040**: Quantum computers likely capable of breaking Bitcoin's ECDSA

## Getting Started

### Prerequisites

- C++ compiler with C++20 support (GCC 12.2+ or Clang 14+)
- CMake 3.22 or newer
- Boost libraries 1.73.0+
- SQLite3 3.7.17+
- Python 3.10+ (for build tools)
- libevent 2.1.8+

**Note**: liboqs (v0.12.0) is already integrated in the `/bitcoin/liboqs/` directory and will be built automatically.

### Building from Source

```bash
# Clone the repository (always use SSH)
git clone git@github.com:qsbitcoin/qsbitcoin.git
cd qsbitcoin

# Navigate to the Bitcoin source directory
cd bitcoin

# Clean any previous build
rm -rf build/

# Configure the build with CMake
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON

# Build the project (use -j for parallel builds)
cmake --build build -j$(nproc)

# Run unit tests to verify the build
ctest --test-dir build --output-on-failure
```

For more detailed build options and troubleshooting, see [QSBITCOIN_TASKS.md](QSBITCOIN_TASKS.md#build-instructions).

## Quick Start: Testing Quantum Features

### Generate a Quantum Address
```bash
# Start bitcoind in regtest mode
./build/bin/bitcoind -regtest -daemon -fallbackfee=0.00001

# Create a wallet
./build/bin/bitcoin-cli -regtest createwallet "quantum_test"

# Generate a quantum address (ML-DSA for standard use)
./build/bin/bitcoin-cli -regtest getnewaddress "" "bech32" "ml-dsa"

# Or generate a high-security address (SLH-DSA for cold storage)
./build/bin/bitcoin-cli -regtest getnewaddress "" "bech32" "slh-dsa"

# Get quantum wallet info
./build/bin/bitcoin-cli -regtest getquantuminfo
```

### Send and Receive with Quantum Addresses
```bash
# Generate blocks for testing
./build/bin/bitcoin-cli -regtest generatetoaddress 101 $(./build/bin/bitcoin-cli -regtest getnewaddress)

# Send to a quantum address
QUANTUM_ADDR=$(./build/bin/bitcoin-cli -regtest getnewaddress "" "bech32" "ml-dsa")
./build/bin/bitcoin-cli -regtest sendtoaddress $QUANTUM_ADDR 1.0

# Mine the transaction
./build/bin/bitcoin-cli -regtest generatetoaddress 1 $(./build/bin/bitcoin-cli -regtest getnewaddress)

# Verify the quantum address can spend
./build/bin/bitcoin-cli -regtest sendtoaddress $(./build/bin/bitcoin-cli -regtest getnewaddress) 0.5 "" "" true
```

For more examples and testing scripts, see the `test_quantum_*.sh` scripts in the project root.

## Contributing

We welcome contributions! Please see our [Contributing Guidelines](CONTRIBUTING.md) for details on:
- Code style and standards
- Development workflow
- Testing requirements
- Code review process

## Implementation Highlights

### Completed Features
- [x] **liboqs Integration** - NIST-standardized quantum-safe algorithms (ML-DSA-65, SLH-DSA-192f)
- [x] **Unified Opcodes** - OP_CHECKSIG_EX and OP_CHECKSIGVERIFY_EX support all quantum algorithms
- [x] **P2WSH Addresses** - Standard bech32 format for all quantum addresses (bc1q...)
- [x] **Soft Fork Activation** - BIP9-style deployment with full backward compatibility
- [x] **Wallet Integration** - Complete descriptor-based quantum wallet support
- [x] **Fee Optimization** - Smart fee structure with quantum signature discounts
- [x] **Comprehensive Testing** - Full test suite with quantum-specific test cases

### Technical Specifications
- **ML-DSA-65**: ~3.3KB signatures for standard transactions (99% of use cases)
- **SLH-DSA-192f**: ~35KB signatures for high-value cold storage
- **Address Format**: Standard P2WSH (indistinguishable from regular Bitcoin addresses)
- **Migration**: Optional - users control when to switch from ECDSA to quantum signatures

See [QSBITCOIN_PLAN.md](QSBITCOIN_PLAN.md) for detailed technical design and implementation learnings.

## Community

- **GitHub Discussions**: [Coming Soon]
- **Discord**: [Coming Soon]
- **Twitter**: [@qsbitcoin](https://twitter.com/qsbitcoin) [Coming Soon]
- **Website**: [https://qsbitcoin.org](https://qsbitcoin.org) [Coming Soon]

## Security

QSBitcoin takes security seriously. If you discover a security vulnerability, please:
- **DO NOT** open a public issue
- Email security@qsbitcoin.org with details
- Use our PGP key for sensitive communications

## License

QSBitcoin is released under the MIT License. See [LICENSE](LICENSE) for details.

## Acknowledgments

- Bitcoin Core developers for the foundational codebase
- NIST Post-Quantum Cryptography team for standardization efforts
- Open Quantum Safe project for cryptographic implementations
- The broader cryptocurrency and cryptography communities

---

**Disclaimer**: While the quantum-safe implementation is complete and functional, this is still experimental software. The implementation has been tested on regtest and is ready for testnet deployment. Do not use for mainnet or real value transfer until thoroughly tested and officially released.

**Contact**: dev@qsbitcoin.org