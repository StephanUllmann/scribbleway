#!/usr/bin/env bash

# Exit immediately on unhandled error
set -e

# Resolve HOME directory if missing
if [ -z "$HOME" ]; then
    HOME=$(getent passwd "$USER" | cut -d: -f6)
fi

PREFIX="${PREFIX:-$HOME/.local}"
BUILD_DIR="build"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
RESTART_OVERLAY=true

print_usage() {
    echo "Usage: ./dev-reinstall.sh [options]"
    echo "Options:"
    echo "  --prefix <path>     Set install prefix (default: $HOME/.local)"
    echo "  --system            Install to /usr/local (requires sudo for install)"
    echo "  --release           Build in Release mode instead of Debug"
    echo "  --no-overlay        Do not start scribbleway-overlay after install"
    echo "  --help              Show this help message"
}

while [[ "$#" -gt 0 ]]; do
    case "$1" in
        --prefix)
            PREFIX="$2"
            shift 2
            ;;
        --system)
            PREFIX="/usr/local"
            shift
            ;;
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        --no-overlay)
            RESTART_OVERLAY=false
            shift
            ;;
        --help)
            print_usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            print_usage
            exit 1
            ;;
    esac
done

SUDO=""
if [ ! -w "$PREFIX" ] && [ "$EUID" -ne 0 ]; then
    SUDO="sudo"
fi

echo "=== 1. Terminating existing scribbleway-overlay processes ==="
pkill -f scribbleway-overlay || true
for i in {1..10}; do
    if ! pgrep -f scribbleway-overlay >/dev/null 2>&1; then
        break
    fi
    sleep 0.2
done
if pgrep -f scribbleway-overlay >/dev/null 2>&1; then
    echo "Warning: scribbleway-overlay did not terminate gracefully, sending SIGKILL..."
    pkill -9 -f scribbleway-overlay || true
fi


echo "=== 2. Cleaning old installed artifacts and caches ==="

# Uninstall from build manifest if present
if [ -f "$BUILD_DIR/install_manifest.txt" ]; then
    echo "Removing files listed in $BUILD_DIR/install_manifest.txt..."
    while IFS= read -r file; do
        if [ -f "$file" ] || [ -L "$file" ]; then
            if [ -w "$file" ] || [ -w "$(dirname "$file")" ]; then
                rm -f "$file" || true
            elif command -v sudo >/dev/null 2>&1 && sudo -n true 2>/dev/null; then
                sudo rm -f "$file" 2>/dev/null || true
            fi
        fi
    done < "$BUILD_DIR/install_manifest.txt"
fi

# Explicit purge of standard binary and desktop-file locations, which catches
# installs made at a different prefix than the current build manifest covers.
TARGET_FILES=(
    "$HOME/.local/bin/scribbleway-overlay"
    "/usr/local/bin/scribbleway-overlay"
    "/usr/bin/scribbleway-overlay"
    "$HOME/.local/share/applications/org.kde.scribbleway.desktop"
    "$HOME/.config/autostart/org.kde.scribbleway-autostart.desktop"
    "$HOME/.local/etc/xdg/autostart/org.kde.scribbleway-autostart.desktop"
    "/usr/share/applications/org.kde.scribbleway.desktop"
    "/etc/xdg/autostart/org.kde.scribbleway-autostart.desktop"
)

for file in "${TARGET_FILES[@]}"; do
    if [ -f "$file" ] || [ -L "$file" ]; then
        if [ -w "$file" ] || [ -w "$(dirname "$file")" ]; then
            rm -f "$file" || true
        elif command -v sudo >/dev/null 2>&1 && sudo -n true 2>/dev/null; then
            sudo rm -f "$file" 2>/dev/null || true
        fi
    fi
done


echo "Clearing the QML disk cache..."
rm -rf "$HOME/.cache/qmlcache"

echo "Wiping build directory and leftover packaging artifacts..."
if [ -d "$BUILD_DIR" ]; then
    rm -rf "$BUILD_DIR" 2>/dev/null || sudo rm -rf "$BUILD_DIR" || true
fi
rm -f ./*.deb 2>/dev/null || true

echo "=== 3. Configuring clean build ==="
NUM_CORES=$(nproc 2>/dev/null || echo 2)

cmake -B "$BUILD_DIR" -S . \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

echo "=== 4. Building project ==="
cmake --build "$BUILD_DIR" -j"$NUM_CORES"

echo "=== 5. Installing freshly built artifacts ==="
if [ -n "$SUDO" ]; then
    $SUDO cmake --install "$BUILD_DIR"
else
    cmake --install "$BUILD_DIR"
fi

if [ "$PREFIX" = "$HOME/.local" ]; then
    mkdir -p "$HOME/.local/share/applications"
    mkdir -p "$HOME/.config/autostart"
    if [ -f org.kde.scribbleway.desktop ]; then
        sed "s#Exec=scribbleway-overlay#Exec=\"$HOME/.local/bin/scribbleway-overlay\"#g" org.kde.scribbleway.desktop > "$HOME/.local/share/applications/org.kde.scribbleway.desktop"
        chmod +x "$HOME/.local/share/applications/org.kde.scribbleway.desktop"
    fi
    if [ -f org.kde.scribbleway-autostart.desktop ]; then
        sed "s#Exec=scribbleway-overlay#Exec=\"$HOME/.local/bin/scribbleway-overlay\"#g" org.kde.scribbleway-autostart.desktop > "$HOME/.config/autostart/org.kde.scribbleway-autostart.desktop"
        chmod +x "$HOME/.config/autostart/org.kde.scribbleway-autostart.desktop"
    fi
fi

echo "=== 6. Process lifecycle management ==="
if [ "$RESTART_OVERLAY" = true ]; then
    OVERLAY_BIN=""
    if [ -x "$PREFIX/bin/scribbleway-overlay" ]; then
        OVERLAY_BIN="$PREFIX/bin/scribbleway-overlay"
    elif command -v scribbleway-overlay >/dev/null 2>&1; then
        OVERLAY_BIN=$(command -v scribbleway-overlay)
    fi

    if [ -n "$OVERLAY_BIN" ]; then
        echo "Starting $OVERLAY_BIN..."
        nohup "$OVERLAY_BIN" >/dev/null 2>&1 &
        echo "Scribbleway overlay daemon started (PID $!)."
    else
        echo "Notice: scribbleway-overlay binary not found in PATH or $PREFIX/bin."
    fi
fi

echo "=== Reinstall complete! ==="
