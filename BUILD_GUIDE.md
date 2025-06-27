# Bitcoin Core Build Guide for Debian 12

This comprehensive guide walks you through building Bitcoin Core from source on a Debian 12 system.

## Table of Contents
1. [Prerequisites](#prerequisites)
2. [System Requirements](#system-requirements)
3. [Installing Dependencies](#installing-dependencies)
4. [Cloning Bitcoin Core](#cloning-bitcoin-core)
5. [Building Bitcoin Core](#building-bitcoin-core)
6. [Testing the Build](#testing-the-build)
7. [Running Bitcoin Core](#running-bitcoin-core)
8. [Troubleshooting](#troubleshooting)
9. [Additional Resources](#additional-resources)

## Prerequisites

- Debian 12 (Bookworm) installed and updated
- Sudo privileges for installing packages
- At least 2GB of free RAM (4GB recommended)
- At least 10GB of free disk space
- Internet connection for downloading dependencies and source code

## System Requirements

### Minimum Requirements:
- CPU: 1 GHz processor
- RAM: 1.5 GB (for compilation)
- Disk: 10 GB free space

### Recommended Requirements:
- CPU: Multi-core processor
- RAM: 4 GB or more
- Disk: 20 GB free space (for blockchain data if running full node)

## Installing Dependencies

### 1. Update System
```bash
sudo apt update
sudo apt upgrade -y
```

### 2. Core Build Tools
Install essential build tools and CMake (Bitcoin Core now uses CMake):
```bash
sudo apt install -y build-essential cmake pkg-config python3 git ccache
```

### 3. Required Libraries
Install core libraries needed for Bitcoin Core:
```bash
sudo apt install -y libevent-dev libboost-dev libsqlite3-dev libssl-dev
```

### 4. Optional but Recommended Dependencies
These enhance functionality:
```bash
sudo apt install -y libzmq3-dev libminiupnpc-dev systemtap-sdt-dev
```

- `libzmq3-dev`: ZeroMQ for notification interface
- `libminiupnpc-dev`: UPnP for automatic port forwarding
- `systemtap-sdt-dev`: User-space tracing

### 5. GUI Dependencies (Optional)
If you want to build the graphical interface:
```bash
sudo apt install -y qt6-base-dev qt6-tools-dev qt6-l10n-tools \
                    qt6-tools-dev-tools libgl-dev qt6-wayland libqrencode-dev
```

### 6. IPC Dependencies (Optional)
For inter-process communication features:
```bash
sudo apt install -y libcapnp-dev capnproto
```

## Cloning Bitcoin Core

1. Navigate to project directory:
```bash
cd qsbitcoin
```

2. Clone the repository:
```bash
git clone https://github.com/bitcoin/bitcoin.git
cd bitcoin
```

3. (Optional) Checkout a specific version:
```bash
# List available tags
git tag

# Checkout a specific version (e.g., v25.0)
git checkout v25.0
```

## Building Bitcoin Core

### 1. Configure Build
Basic configuration:
```bash
cmake -B build
```

Full-featured build with GUI:
```bash
cmake -B build \
  -DBUILD_GUI=ON \
  -DWITH_ZMQ=ON \
  -DENABLE_WALLET=ON \
  -DWITH_MINIUPNPC=ON
```

### 2. Compile
Build using all available CPU cores:
```bash
cmake --build build -j$(nproc)
```

Note: The `-j$(nproc)` flag uses all CPU cores. Reduce if you encounter memory issues:
```bash
cmake --build build -j2  # Use only 2 cores
```

### 3. Install (Optional)
Install system-wide:
```bash
sudo cmake --install build
```

Or run directly from build directory without installing.

## Testing the Build

### 1. Run Unit Tests
```bash
ctest --test-dir build
```

### 2. Verify Binaries
Check bitcoind version:
```bash
./build/src/bitcoind --version
```

Check GUI (if built):
```bash
./build/src/qt/bitcoin-qt --version
```

## Running Bitcoin Core

### Command Line (bitcoind)
Start the daemon:
```bash
./build/src/bitcoind -daemon
```

Check status:
```bash
./build/src/bitcoin-cli getblockchaininfo
```

Stop the daemon:
```bash
./build/src/bitcoin-cli stop
```

### GUI (bitcoin-qt)
Launch the graphical interface:
```bash
./build/src/qt/bitcoin-qt
```

### Configuration
Create config file:
```bash
mkdir -p ~/.bitcoin
nano ~/.bitcoin/bitcoin.conf
```

Example configuration:
```
# Network
testnet=0  # Use mainnet (set to 1 for testnet)

# Performance
dbcache=2048  # Database cache size in MB

# Connection
maxconnections=20

# RPC (if needed)
server=1
rpcuser=yourusername
rpcpassword=yourpassword
```

## Troubleshooting

### Common Issues

1. **Build fails with memory errors**
   - Solution: Reduce parallel jobs: `cmake --build build -j1`

2. **Missing dependencies**
   - Run: `sudo apt install -f` to fix broken packages
   - Check specific error messages for missing libraries

3. **CMake configuration fails**
   - Ensure all dependencies are installed
   - Check CMake version: `cmake --version` (need 3.13+)

4. **Permission denied errors**
   - Don't use sudo for building, only for installing
   - Check file ownership in build directory

5. **Qt/GUI issues**
   - Verify Qt6 installation: `qmake6 --version`
   - Try building without GUI: `-DBUILD_GUI=OFF`

### Debug Build
For debugging issues:
```bash
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
```

### Clean Build
If you need to start over:
```bash
rm -rf build
cmake -B build
cmake --build build -j$(nproc)
```

## Additional Resources

- [Official Bitcoin Core Documentation](https://github.com/bitcoin/bitcoin/tree/master/doc)
- [Bitcoin Core Build Guide for Unix](https://github.com/bitcoin/bitcoin/blob/master/doc/build-unix.md)
- [Bitcoin Core Releases](https://bitcoincore.org/en/releases/)
- [Bitcoin Developer Documentation](https://developer.bitcoin.org/)

## Security Considerations

1. Always verify GPG signatures when downloading releases
2. Keep your Bitcoin Core updated
3. Use strong RPC passwords if enabling RPC
4. Consider running Bitcoin Core on a dedicated machine
5. Regular backups of wallet.dat if using wallet functionality

## Next Steps

After successful build:
1. Configure your node (bitcoin.conf)
2. Consider setting up as a service for automatic startup
3. Monitor initial blockchain sync (can take several hours/days)
4. Explore bitcoin-cli commands
5. Join Bitcoin development community if interested in contributing

---

Last updated: June 2025
Bitcoin Core uses CMake build system as of v28.0+