#!/bin/bash

# Setup script for ZoneMinder E2E Tests
# This script installs dependencies and sets up the test environment

set -e

echo "🔧 Setting up ZoneMinder E2E Tests..."
echo ""

# Check if Node.js is installed
if ! command -v node &> /dev/null; then
    echo "❌ Node.js is not installed!"
    echo "   Please install Node.js 16+ from: https://nodejs.org/"
    exit 1
fi

NODE_VERSION=$(node -v | cut -d'.' -f1 | sed 's/v//')
if [ "$NODE_VERSION" -lt 16 ]; then
    echo "❌ Node.js version is too old ($(node -v))"
    echo "   Please upgrade to Node.js 16+ from: https://nodejs.org/"
    exit 1
fi

echo "✅ Node.js $(node -v) detected"
echo ""

# Install npm dependencies
echo "📦 Installing npm dependencies..."
npm install

echo ""
echo "🌐 Installing Playwright browsers..."
npx playwright install

echo ""
echo "✅ Setup complete!"
echo ""
echo "📚 Quick Start:"
echo ""
echo "  # Run tests in both browsers:"
echo "  npm run test:both"
echo ""
echo "  # Compare results:"
echo "  npm run compare"
echo ""
echo "  # View HTML report:"
echo "  npm run show:report"
echo ""
echo "  # Run with UI (interactive):"
echo "  npm run test:ui"
echo ""
echo "📖 See README.md for full documentation"
echo ""
