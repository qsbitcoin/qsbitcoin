#!/bin/bash
# QSBitcoin Development Dependencies Installation Script
# Supports Ubuntu/Debian, Fedora/RHEL, macOS, and Arch Linux

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}QSBitcoin Development Environment Setup${NC}"
echo "========================================"
echo ""

# Detect OS
detect_os() {
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
        if [ -f /etc/debian_version ]; then
            echo "debian"
        elif [ -f /etc/fedora-release ]; then
            echo "fedora"
        elif [ -f /etc/redhat-release ]; then
            echo "rhel"
        elif [ -f /etc/arch-release ]; then
            echo "arch"
        else
            echo "unknown-linux"
        fi
    elif [[ "$OSTYPE" == "darwin"* ]]; then
        echo "macos"
    else
        echo "unknown"
    fi
}

OS=$(detect_os)
echo -e "Detected OS: ${YELLOW}$OS${NC}"
echo ""

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Install dependencies based on OS
case $OS in
    debian)
        echo -e "${GREEN}Installing dependencies for Ubuntu/Debian...${NC}"
        sudo apt-get update
        sudo apt-get install -y \
            build-essential \
            cmake \
            pkg-config \
            python3 \
            python3-pip \
            libboost-all-dev \
            libevent-dev \
            libsqlite3-dev \
            libssl-dev \
            ccache \
            git \
            curl \
            wget \
            clang-format \
            clang-tidy \
            valgrind
        
        # Check GCC version
        GCC_VERSION=$(gcc -dumpversion | cut -d. -f1)
        if [ "$GCC_VERSION" -lt 12 ]; then
            echo -e "${YELLOW}Warning: GCC version is less than 12. Installing gcc-12...${NC}"
            sudo apt-get install -y gcc-12 g++-12
            sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 100
            sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 100
        fi
        ;;
        
    fedora|rhel)
        echo -e "${GREEN}Installing dependencies for Fedora/RHEL...${NC}"
        sudo dnf install -y \
            gcc-c++ \
            cmake \
            pkgconfig \
            python3 \
            python3-pip \
            boost-devel \
            libevent-devel \
            sqlite-devel \
            openssl-devel \
            ccache \
            git \
            curl \
            wget \
            clang-tools-extra \
            valgrind
        ;;
        
    arch)
        echo -e "${GREEN}Installing dependencies for Arch Linux...${NC}"
        sudo pacman -Syu --needed \
            base-devel \
            cmake \
            python \
            python-pip \
            boost \
            libevent \
            sqlite \
            openssl \
            ccache \
            git \
            curl \
            wget \
            clang \
            valgrind
        ;;
        
    macos)
        echo -e "${GREEN}Installing dependencies for macOS...${NC}"
        
        # Check if Homebrew is installed
        if ! command_exists brew; then
            echo -e "${YELLOW}Homebrew not found. Installing Homebrew...${NC}"
            /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
        fi
        
        brew update
        brew install \
            cmake \
            pkg-config \
            python3 \
            boost \
            libevent \
            sqlite \
            openssl \
            ccache \
            git \
            wget \
            clang-format \
            valgrind || true  # valgrind might not be available on all macOS versions
        ;;
        
    *)
        echo -e "${RED}Unsupported operating system: $OS${NC}"
        echo "Please install the following dependencies manually:"
        echo "- C++ compiler with C++20 support (GCC 12+ or Clang 14+)"
        echo "- CMake 3.22+"
        echo "- Boost libraries 1.73.0+"
        echo "- SQLite3 3.7.17+"
        echo "- Python 3.10+"
        echo "- libevent 2.1.8+"
        echo "- OpenSSL"
        echo "- ccache (optional but recommended)"
        exit 1
        ;;
esac

echo ""
echo -e "${GREEN}Checking installed versions...${NC}"

# Check CMake version
if command_exists cmake; then
    CMAKE_VERSION=$(cmake --version | head -n1 | cut -d' ' -f3)
    echo -e "CMake version: ${YELLOW}$CMAKE_VERSION${NC}"
else
    echo -e "${RED}CMake not found!${NC}"
fi

# Check compiler version
if command_exists g++; then
    GCC_VERSION=$(g++ --version | head -n1)
    echo -e "G++ version: ${YELLOW}$GCC_VERSION${NC}"
elif command_exists clang++; then
    CLANG_VERSION=$(clang++ --version | head -n1)
    echo -e "Clang++ version: ${YELLOW}$CLANG_VERSION${NC}"
else
    echo -e "${RED}No C++ compiler found!${NC}"
fi

# Check Python version
if command_exists python3; then
    PYTHON_VERSION=$(python3 --version)
    echo -e "Python version: ${YELLOW}$PYTHON_VERSION${NC}"
else
    echo -e "${RED}Python 3 not found!${NC}"
fi

# Check Boost version
if [ -f /usr/include/boost/version.hpp ]; then
    BOOST_VERSION=$(grep "BOOST_LIB_VERSION" /usr/include/boost/version.hpp | cut -d'"' -f2 | tr '_' '.')
    echo -e "Boost version: ${YELLOW}$BOOST_VERSION${NC}"
elif [ -f /usr/local/include/boost/version.hpp ]; then
    BOOST_VERSION=$(grep "BOOST_LIB_VERSION" /usr/local/include/boost/version.hpp | cut -d'"' -f2 | tr '_' '.')
    echo -e "Boost version: ${YELLOW}$BOOST_VERSION${NC}"
fi

# Set up ccache
echo ""
echo -e "${GREEN}Setting up ccache for faster builds...${NC}"
if command_exists ccache; then
    ccache --set-config=max_size=5G
    echo -e "ccache configured with max size: ${YELLOW}5G${NC}"
    
    # Add ccache to PATH if not already there
    if [[ ":$PATH:" != *":/usr/lib/ccache:"* ]]; then
        echo ""
        echo -e "${YELLOW}Add the following to your ~/.bashrc or ~/.zshrc:${NC}"
        echo 'export PATH="/usr/lib/ccache:$PATH"'
    fi
else
    echo -e "${YELLOW}ccache not installed. Consider installing it for faster builds.${NC}"
fi

# Create development directories
echo ""
echo -e "${GREEN}Creating development directories...${NC}"
mkdir -p ~/qsbitcoin-dev/{logs,builds,test-results}

echo ""
echo -e "${GREEN}Development environment setup complete!${NC}"
echo ""
echo "Next steps:"
echo "1. Clone the QSBitcoin repository:"
echo "   git clone git@github.com:qsbitcoin/qsbitcoin.git"
echo "2. Build QSBitcoin:"
echo "   cd qsbitcoin/bitcoin"
echo "   cmake -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTS=ON"
echo "   cmake --build build -j\$(nproc)"
echo "3. Run tests:"
echo "   ctest --test-dir build --output-on-failure"
echo ""
echo -e "${YELLOW}Note: liboqs will be added as a dependency in Phase 1.3${NC}"