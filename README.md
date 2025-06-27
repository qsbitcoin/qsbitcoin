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

🚧 **Currently in Planning Phase** 🚧

See [QSBITCOIN_PLAN.md](QSBITCOIN_PLAN.md) for the comprehensive development roadmap.

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
- Post-quantum cryptography libraries (liboqs - will be integrated in Phase 1.3)

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

## Contributing

We welcome contributions! Please see our [Contributing Guidelines](CONTRIBUTING.md) for details on:
- Code style and standards
- Development workflow
- Testing requirements
- Code review process

## Roadmap

### Phase 1: Foundation (Current)
- [x] Fork Bitcoin Core and initialize repository
- [x] Initial build system verification
- [ ] Complete repository setup and infrastructure
- [ ] Integrate liboqs cryptographic library  
- [ ] Create signature abstraction layer

### Phase 2: Core Implementation
- [ ] Quantum-safe signature implementation
- [ ] Transaction structure updates
- [ ] New address format

### Phase 3: Network & Consensus
- [ ] Consensus rule updates
- [ ] Network protocol modifications
- [ ] Migration mechanisms

See [QSBITCOIN_PLAN.md](QSBITCOIN_PLAN.md) for detailed timeline and milestones.

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

**Disclaimer**: This project is in early development. Do not use for production or real value transfer until officially released.

**Contact**: dev@qsbitcoin.org