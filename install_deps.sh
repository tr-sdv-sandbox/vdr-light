#!/bin/bash
#
# install_deps.sh - Install dependencies for VDR ecosystem PoC
#
# Tested on: Ubuntu 24.04
#

set -e

echo "=== VDR Ecosystem Dependencies Installer ==="
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1"
    exit 1
}

# Check if running on Ubuntu
if ! grep -q "Ubuntu" /etc/os-release 2>/dev/null; then
    warn "This script is designed for Ubuntu. Other distros may require modifications."
fi

# Check Ubuntu version
UBUNTU_VERSION=$(grep VERSION_ID /etc/os-release | cut -d'"' -f2)
info "Detected Ubuntu version: $UBUNTU_VERSION"

if [[ "$UBUNTU_VERSION" != "24.04" ]]; then
    warn "Script tested on Ubuntu 24.04. Your version ($UBUNTU_VERSION) may have different package versions."
fi

# Update package list
info "Updating package list..."
sudo apt-get update

# Core build tools
info "Installing core build tools..."
sudo apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    git

# Cyclone DDS
info "Installing Cyclone DDS..."
sudo apt-get install -y \
    cyclonedds-dev \
    cyclonedds-tools \
    libcycloneddsidl0t64 \
    libddsc0t64

# Google logging
info "Installing Google logging (glog)..."
sudo apt-get install -y \
    libgoogle-glog-dev

# YAML parsing
info "Installing yaml-cpp..."
sudo apt-get install -y \
    libyaml-cpp-dev

# Google Test for unit testing
info "Installing GoogleTest..."
sudo apt-get install -y \
    libgtest-dev

# Optional: nlohmann-json for JSON encoding (simulated MQTT payload)
info "Installing nlohmann-json..."
sudo apt-get install -y \
    nlohmann-json3-dev

# gRPC and Protobuf for OTLP receiver
info "Installing gRPC and Protobuf..."
sudo apt-get install -y \
    libgrpc++-dev \
    libprotobuf-dev \
    protobuf-compiler \
    protobuf-compiler-grpc

# Lua for libvssdag scripting
info "Installing Lua..."
sudo apt-get install -y \
    liblua5.4-dev

# libvssdag and dependencies (CAN-to-VSS transformation)
info "Installing libvssdag dependencies..."
sudo apt-get install -y \
    can-utils \
    linux-modules-extra-$(uname -r) || true  # For vcan module

# libvssdag - build from source if not installed
info "Checking for libvssdag..."
if ! pkg-config --exists vssdag 2>/dev/null; then
    info "Installing libvssdag from source..."
    TEMP_DIR=$(mktemp -d)
    pushd "$TEMP_DIR" > /dev/null
    git clone --depth 1 https://github.com/tr-sdv-sandbox/libvssdag.git
    cd libvssdag
    cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j$(nproc)
    sudo cmake --install build
    popd > /dev/null
    rm -rf "$TEMP_DIR"
    info "libvssdag installed successfully"
else
    info "libvssdag already installed"
fi

# Open1722 - IEEE 1722 AVTP library for CAN-over-Ethernet
info "Checking for Open1722..."
if ! pkg-config --exists open1722 2>/dev/null; then
    info "Installing Open1722 from source..."
    TEMP_DIR=$(mktemp -d)
    pushd "$TEMP_DIR" > /dev/null
    git clone --depth 1 https://github.com/COVESA/Open1722.git
    cd Open1722
    cmake -B build -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_BUILD_TYPE=Release
    cmake --build build -j$(nproc)
    sudo cmake --install build
    popd > /dev/null
    rm -rf "$TEMP_DIR"
    info "Open1722 installed successfully"
else
    info "Open1722 already installed"
fi

# Verify installations
echo ""
info "Verifying installations..."

verify_package() {
    if dpkg -l | grep -q "^ii  $1 "; then
        echo -e "  ${GREEN}✓${NC} $1"
        return 0
    else
        echo -e "  ${RED}✗${NC} $1"
        return 1
    fi
}

echo ""
echo "Installed packages:"
verify_package "cyclonedds-dev"
verify_package "cyclonedds-tools"
verify_package "libgoogle-glog-dev"
verify_package "libyaml-cpp-dev"
verify_package "libgtest-dev"
verify_package "nlohmann-json3-dev"
verify_package "libgrpc++-dev"
verify_package "libprotobuf-dev"
verify_package "protobuf-compiler"
verify_package "cmake"
verify_package "build-essential"

# Check Cyclone DDS version
echo ""
info "Cyclone DDS version:"
apt-cache show cyclonedds-dev | grep "^Version:" | head -1

# Check idlc (IDL compiler) is available
echo ""
info "Checking IDL compiler (idlc)..."
if command -v idlc &> /dev/null; then
    echo -e "  ${GREEN}✓${NC} idlc found at: $(which idlc)"
    idlc --version 2>/dev/null || true
else
    error "idlc not found. Cyclone DDS tools may not be installed correctly."
fi

# Check ddsperf (performance tool) is available
info "Checking DDS performance tool..."
if command -v ddsperf &> /dev/null; then
    echo -e "  ${GREEN}✓${NC} ddsperf found at: $(which ddsperf)"
else
    warn "ddsperf not found. Performance testing tool not available."
fi

# Print pkg-config info for Cyclone DDS
echo ""
info "Cyclone DDS pkg-config info:"
if pkg-config --exists cyclonedds; then
    echo "  Include path: $(pkg-config --cflags cyclonedds)"
    echo "  Library path: $(pkg-config --libs cyclonedds)"
else
    warn "pkg-config cannot find cyclonedds. CMake may need manual configuration."
fi

# Check libvssdag
echo ""
info "libvssdag pkg-config info:"
if pkg-config --exists vssdag; then
    echo -e "  ${GREEN}✓${NC} libvssdag installed"
    echo "  Version: $(pkg-config --modversion vssdag 2>/dev/null || echo 'unknown')"
else
    warn "libvssdag not installed - vssdag probe will not be built"
fi

# Check Open1722
echo ""
info "Open1722 pkg-config info:"
if pkg-config --exists open1722; then
    echo -e "  ${GREEN}✓${NC} Open1722 installed"
    echo "  Version: $(pkg-config --modversion open1722 2>/dev/null || echo 'unknown')"
else
    warn "Open1722 not installed - AVTP probe will not be built"
fi

echo ""
echo "=== Installation Complete ==="
echo ""
echo "Next steps:"
echo "  1. Review SPECIFICATION.md for architecture details"
echo "  2. Run 'cmake -B build && cmake --build build' to build"
echo "  3. Test DDS with: ddsperf pub  (terminal 1)"
echo "                    ddsperf sub  (terminal 2)"
echo ""
