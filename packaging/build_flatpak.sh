#!/usr/bin/env bash
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_DIR"

MANIFEST="packaging/flatpak/org.kde.scribbleway.yml"
BUILD_DIR="build-flatpak"
REPO_DIR_FLATPAK="repo-flatpak"
APP_ID="org.kde.scribbleway"
RUNTIME="org.kde.Platform//6.10"
SDK="org.kde.Sdk//6.10"

MODE="install"   # default: build + install --user
# --bundle → also export a single-file .flatpak next to repo root
# --build-only → build without install/export

for arg in "$@"; do
    case "$arg" in
        --bundle)
            MODE="bundle"
            ;;
        --build-only)
            if [ "$MODE" != "bundle" ]; then
                MODE="build-only"
            fi
            ;;
        --help|-h)
            echo "Usage: $0 [options]"
            echo "Options:"
            echo "  --bundle       Build and export to a single-file .flatpak at repo root"
            echo "  --build-only   Build without exporting or installing"
            echo "  --help, -h     Show this help message"
            exit 0
            ;;
        *)
            echo "Unknown option: $arg"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

echo "=== Step 1: Checking Prerequisites ==="
if ! command -v flatpak >/dev/null 2>&1; then
    echo "Error: flatpak is not installed or not in PATH."
    echo "Please install flatpak."
    exit 1
fi

if ! command -v flatpak-builder >/dev/null 2>&1; then
    echo "Error: flatpak-builder is not installed or not in PATH."
    echo "Please install it using your package manager, e.g.:"
    echo "  sudo apt install flatpak-builder"
    exit 1
fi

echo "=== Step 2: Ensuring Flathub remote is configured ==="
flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo

echo "=== Step 3: Installing KDE 6.10 Platform and SDK if missing ==="
flatpak install -y --user flathub "$RUNTIME" "$SDK"

echo "=== Step 4: Building Flatpak package ==="
flatpak-builder --force-clean --ccache "$BUILD_DIR" "$MANIFEST"

if [ "$MODE" = "install" ]; then
    echo "=== Step 5: Installing Flatpak package for current user ==="
    flatpak-builder --user --install --force-clean "$BUILD_DIR" "$MANIFEST"
    echo "Flatpak successfully built and installed."
    echo "To run: flatpak run $APP_ID"
elif [ "$MODE" = "bundle" ]; then
    echo "=== Step 5: Exporting Flatpak to a single-file bundle ==="
    flatpak-builder --repo="$REPO_DIR_FLATPAK" --force-clean "$BUILD_DIR" "$MANIFEST"
    flatpak build-bundle "$REPO_DIR_FLATPAK" "scribbleway.flatpak" "$APP_ID"
    echo "Flatpak bundle successfully created: scribbleway.flatpak"
elif [ "$MODE" = "build-only" ]; then
    echo "=== Step 5: Build-only mode selected. Skipping install and bundle export ==="
    echo "Flatpak successfully built in $BUILD_DIR."
fi
