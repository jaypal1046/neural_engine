# Setup and Launch Nero Brain (Windows)
# ===================================
# This script automates the build and execution of Nero Brain.

$ErrorActionPreference = "Stop"
$PWD = Get-Location

Write-Host "`n>> Starting Nero Brain Setup..." -ForegroundColor Cyan

# 1. Check for Python
if (!(Get-Command python -ErrorAction SilentlyContinue)) {
    Write-Host "Error: Python is not installed or not in PATH." -ForegroundColor Red
    exit 1
}

# 2. Check for Node.js
if (!(Get-Command npm -ErrorAction SilentlyContinue)) {
    Write-Host "Error: Node.js/NPM is not installed or not in PATH." -ForegroundColor Red
    exit 1
}

# 3. Build Neural Engine
Write-Host "`n>> Building Neural Engine..." -ForegroundColor Yellow
cd "$PWD"
if (Test-Path "build_neural_engine.bat") {
    .\build_neural_engine.bat
} else {
    Write-Host "Warning: build_neural_engine.bat not found. Skipping." -ForegroundColor Gray
}

# 4. Install Dependencies
Write-Host "`n>> Installing Frontend Dependencies..." -ForegroundColor Yellow
cd "$PWD\desktop_app"
npm install

Write-Host "`n>> Installing Backend Dependencies..." -ForegroundColor Yellow
cd "$PWD\server"
python -m pip install -r requirements.txt (Get-Content requirements.txt -ErrorAction SilentlyContinue)

# 5. Launch
Write-Host "`n>> Launching Nero Brain..." -ForegroundColor Green
Start-Process powershell -ArgumentList "-NoExit", "-Command", "cd server; python main.py"
cd "$PWD\desktop_app"
npm run dev

Write-Host "`n>> Setup Complete. The application is now running." -ForegroundColor Green
