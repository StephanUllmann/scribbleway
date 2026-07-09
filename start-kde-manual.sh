#!/bin/bash
# Enable job control for running processes in the background
set -m

# Force Qt applications to use xcb (X11) platform so they connect to VNC server
export QT_QPA_PLATFORM=xcb

echo "=== Starting KDE Plasma 6 Components Manually (Headless Container Mode) ==="

# 1. Start the KDE global shortcut daemon
echo "Launching kglobalacceld..."
/usr/lib/kglobalacceld &

# 2. Start KWin X11 Window Manager (using software rendering LLVMpipe if no GPU)
echo "Launching kwin_x11..."
kwin_x11 --replace &

# 3. Start KDE Plasma Desktop Shell (wallpaper, panel, widgets)
echo "Launching plasmashell..."
plasmashell &

# 4. Start Scribbleway overlay daemon
echo "Launching scribbleway-overlay..."
/home/builduser/.local/bin/scribbleway-overlay &

# Keep the script running and wait for child processes
echo "KDE manually started. Waiting..."
wait
