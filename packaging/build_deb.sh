#!/usr/bin/env bash
set -e

# Path to the build directory (passed as argument or default to 'build')
BUILD_DIR="${1:-build}"
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Ensure we are in the repo directory
cd "$REPO_DIR"

# Skip rebuild if inside a Debian packaging environment to avoid recursion
if [ -n "$DEB_BUILD_ARCH" ] || [ -n "$DPKG_RUNNING_VERSION" ]; then
    echo "Detected Debian packaging environment. Skipping automatic .deb rebuild."
    exit 0
fi

# Parse CMAKE_INSTALL_PREFIX from CMakeCache.txt
PREFIX=""
if [ -f "${BUILD_DIR}/CMakeCache.txt" ]; then
    PREFIX=$(grep -E "^CMAKE_INSTALL_PREFIX:PATH=" "${BUILD_DIR}/CMakeCache.txt" | cut -d= -f2)
fi

DEB_BUILD_DIR="$BUILD_DIR"
if [ "$PREFIX" != "/usr" ]; then
    echo "Current build directory prefix is '$PREFIX', but Debian package requires '/usr'."
    echo "Configuring and building in a separate 'build-deb' directory..."
    
    # Configure build-deb if not already configured
    if [ ! -d "build-deb" ] || [ ! -f "build-deb/CMakeCache.txt" ]; then
        cmake -B build-deb -S . -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_BUILD_TYPE=Release -DBUILD_DEB_PACKAGE=OFF
    fi
    
    # Build build-deb
    NUM_CORES=$(nproc 2>/dev/null || echo 2)
    cmake --build build-deb -j"$NUM_CORES"
    
    DEB_BUILD_DIR="build-deb"
fi

echo "=== Packaging .deb file ==="

# Staging directory
STAGING_DIR="${DEB_BUILD_DIR}/debian_package"
rm -rf "$STAGING_DIR"
mkdir -p "$STAGING_DIR/DEBIAN"

# Install CMake build outputs to staging directory overriding the prefix to /usr
DESTDIR="$STAGING_DIR" cmake --install "$DEB_BUILD_DIR" --prefix /usr

# Determine architecture
ARCH=$(dpkg --print-architecture 2>/dev/null || echo "amd64")

# Resolve dependencies and generate control file
echo "misc:Depends=" > "${DEB_BUILD_DIR}/substvars"
# Find all executables and shared libraries in the staging directory to resolve dependencies
BINARIES=$(find "$STAGING_DIR" -type f -executable -o -name "*.so")
if [ -n "$BINARIES" ]; then
    dpkg-shlibdeps -O $BINARIES >> "${DEB_BUILD_DIR}/substvars"
fi

# Use dpkg-gencontrol to write a clean binary package control file with correct architecture, size, and resolved dependencies
dpkg-gencontrol -pscribbleway -P"$STAGING_DIR" -T"${DEB_BUILD_DIR}/substvars" -O > "$STAGING_DIR/DEBIAN/control"
chmod 755 "$STAGING_DIR/DEBIAN"
chmod 644 "$STAGING_DIR/DEBIAN/control"

# Extract version from the generated control file
VERSION=$(grep -E "^Version:" "$STAGING_DIR/DEBIAN/control" | awk '{print $2}')
if [ -z "$VERSION" ]; then
    VERSION="0.1-1"
fi

DEB_FILE="scribbleway_${VERSION}_${ARCH}.deb"

echo "Building package using dpkg-deb..."
dpkg-deb --build --root-owner-group "$STAGING_DIR" "$DEB_FILE"

echo "Debian package successfully built: $DEB_FILE"
