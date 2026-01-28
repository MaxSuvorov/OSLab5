#!/bin/bash

echo "Building Temperature Monitoring System v2.0..."

echo ""
echo "Step 1: Installing dependencies..."
sudo apt-get update
sudo apt-get install -y g++ cmake build-essential libsqlite3-dev

echo ""
echo "Step 2: Creating build directory..."
mkdir -p build
cd build

echo ""
echo "Step 3: Configuring with CMake..."
cmake ..

echo ""
echo "Step 4: Building..."
make -j$(nproc)

echo ""
echo "Done!"
echo ""
echo "Executables created:"
echo "  - temperature_server (main server)"
echo "  - web_server (web interface)"
echo "  - device_simulator (device simulator)"
echo ""
echo "Usage:"
echo "  1. Run ./temperature_server"
echo "  2. Run ./web_server (optional, for web interface)"
echo "  3. Open browser: http://localhost:8081"
echo ""
