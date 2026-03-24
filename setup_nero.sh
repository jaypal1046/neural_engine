#!/bin/bash

# Setup and Launch Nero Brain (Unix/macOS)
# ========================================

echo -e "\n>> Starting Nero Brain Setup..."

# 1. Check for Python
if ! command -v python3 &> /dev/null; then
    echo "Error: Python3 is not installed or not in PATH."
    exit 1
fi

# 2. Check for Node.js
if ! command -v npm &> /dev/null; then
    echo "Error: Node.js/NPM is not installed or not in PATH."
    exit 1
fi

# 3. Build Neural Engine
echo -e "\n>> Building Neural Engine..."
if [ -f "./build_smart_brain.sh" ]; then
    chmod +x ./build_smart_brain.sh
    ./build_smart_brain.sh
else
    echo "Warning: build_smart_brain.sh not found. Skipping."
fi

# 4. Install Dependencies
echo -e "\n>> Installing Frontend Dependencies..."
cd desktop_app && npm install && cd ..

echo -e "\n>> Installing Backend Dependencies..."
python3 -m pip install -r server/requirements.txt

# 5. Launch
echo -e "\n>> Launching Nero Brain..."
# Start server in background
cd server && python3 main.py &
cd ..

# Start UI
cd desktop_app && npm run dev

echo -e "\n>> Setup Complete."
