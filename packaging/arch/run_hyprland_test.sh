#!/usr/bin/env bash
# Run the built Arch package under a real Hyprland session, nested inside the
# host's Wayland compositor. Requires that build_arch_docker.sh has produced a
# package already.
#
#   ./packaging/arch/run_hyprland_test.sh          # run checks, dump results, exit
#   ./packaging/arch/run_hyprland_test.sh --keep   # leave the session open to click around
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_DIR/packaging/arch"

KEEP=""
[ "${1:-}" = "--keep" ] && KEEP="1"

if [ -z "${WAYLAND_DISPLAY:-}" ]; then
    echo "Error: not in a Wayland session. Hyprland can only nest inside one." >&2
    exit 1
fi
HOST_SOCKET="${XDG_RUNTIME_DIR}/${WAYLAND_DISPLAY}"
[ -S "$HOST_SOCKET" ] || { echo "Error: no Wayland socket at $HOST_SOCKET" >&2; exit 1; }

# Newest non-debug package. build_arch_docker.sh leaves older versions behind.
PKG="$(ls -t scribbleway-git-[0-9]*.pkg.tar.zst 2>/dev/null | head -1 || true)"
[ -n "$PKG" ] || { echo "Error: no package found. Run build_arch_docker.sh first." >&2; exit 1; }
echo "=== Using $PKG ==="

# PKGBUILD sources from git, so the package is built from HEAD, not the working
# tree. Uncommitted changes are silently not under test — this bites hard.
if ! git -C "$REPO_DIR" diff --quiet -- src/ CMakeLists.txt; then
    echo "WARNING: uncommitted changes under src/ — testing HEAD, not your working tree." >&2
fi

OUT="$REPO_DIR/packaging/arch/hyprland-test-out"
rm -rf "$OUT"; mkdir -p "$OUT"

echo "=== Building test image ==="
docker build -q -t scribbleway-hyprland-test -f Dockerfile.hyprland \
    --build-arg "UID=$(id -u)" --build-arg "GID=$(id -g)" --build-arg "PKG=$PKG" .

echo "=== Running nested Hyprland ==="
# --group-add for /dev/dri: Hyprland's wayland backend still needs a render node
# for EGL even though the host owns the actual output.
docker run --rm \
    --device /dev/dri \
    $(for g in video render; do echo --group-add "$(getent group "$g" | cut -d: -f3)"; done) \
    -v "$HOST_SOCKET:/tmp/host-wayland.sock" \
    -v "$OUT:/out" \
    -e "WAYLAND_DISPLAY=/tmp/host-wayland.sock" \
    -e "KEEP_OPEN=$KEEP" \
    --name scribbleway-hyprland-test \
    scribbleway-hyprland-test || true

echo
echo "=== Results in $OUT ==="
cat "$OUT/report.txt" 2>/dev/null || echo "No report produced — check $OUT/*.log"
