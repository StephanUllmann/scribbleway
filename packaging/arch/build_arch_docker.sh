#!/usr/bin/env bash
set -euo pipefail

# Find repository root
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_DIR"

echo "=== Step 1: Checking Prerequisites ==="
if ! command -v docker >/dev/null 2>&1; then
    echo "Error: docker is not installed or not in PATH."
    exit 1
fi

IMAGE_NAME="scribbleway-arch-builder"
CONTAINER_NAME="scribbleway-arch-builder-container"

echo "=== Step 2: Building Docker Image ==="
docker build -t "$IMAGE_NAME" -f packaging/arch/Dockerfile.arch .

echo "=== Step 3: Creating Temporary Container ==="
# Remove any existing container with the same name
docker rm -f "$CONTAINER_NAME" >/dev/null 2>&1 || true
docker create --name "$CONTAINER_NAME" "$IMAGE_NAME"

echo "=== Step 4: Extracting Built Packages ==="
TEMP_OUT=$(mktemp -d)
docker cp "$CONTAINER_NAME:/home/builduser/scribbleway/packaging/arch/." "$TEMP_OUT"
mv "$TEMP_OUT"/*.pkg.tar.zst packaging/arch/ 2>/dev/null || true
rm -rf "$TEMP_OUT"

# Find if any pkg.tar.zst files were extracted
PKG_FILES=(packaging/arch/*.pkg.tar.zst)
if [ -e "${PKG_FILES[0]}" ]; then
    echo "Successfully extracted packages:"
    for pkg in "${PKG_FILES[@]}"; do
        echo "  - $(basename "$pkg")"
    done
else
    echo "Error: No .pkg.tar.zst packages found in build output."
    docker rm -f "$CONTAINER_NAME" >/dev/null 2>&1 || true
    exit 1
fi

echo "=== Step 5: Cleaning Up ==="
docker rm -f "$CONTAINER_NAME" >/dev/null 2>&1 || true

echo "Build complete! Packages are located in packaging/arch/"
