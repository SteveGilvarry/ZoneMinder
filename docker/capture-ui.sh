#!/bin/bash
# Helper script to capture ZoneMinder UI screenshots for testing
# Usage: ./docker/capture-ui.sh [url]

set -e

ZM_URL="${1:-http://zoneminder}"

echo "ZoneMinder UI Screenshot Capture Tool"
echo "======================================"
echo ""

# Check if docker-compose is running
if ! docker-compose -f docker-compose.dev.yml ps | grep -q "zm-app"; then
    echo "❌ ZoneMinder containers are not running!"
    echo "Start them with: docker-compose -f docker-compose.dev.yml up -d"
    exit 1
fi

echo "📸 Capturing screenshot from ${ZM_URL}..."
echo ""

# Run Puppeteer in a container to capture screenshot
docker run --rm \
    --network zoneminder_zm-network \
    -v "$(pwd):/workspace" \
    -e ZM_URL="${ZM_URL}" \
    browserless/chrome:latest \
    node /workspace/docker/test-screenshot.js

echo ""
echo "✅ Screenshot captured successfully!"
echo ""
echo "Screenshots are saved in ./screenshots/"
ls -lh ./screenshots/ 2>/dev/null | tail -5

exit 0
