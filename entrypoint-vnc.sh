#!/bin/bash
set -e

# Setup VNC configuration directories
mkdir -p "$HOME/.vnc" "$HOME/.config/tigervnc"

# Set VNC password non-interactively (password: scribble)
echo -e "scribble\nscribble\nn" | vncpasswd

# Write the xstartup script to launch KDE Plasma X11
cat << 'EOF' > "$HOME/.vnc/xstartup"
#!/bin/sh
unset SESSION_MANAGER
unset DBUS_SESSION_BUS_ADDRESS
export QT_QPA_PLATFORM=xcb

# Start the KDE Plasma X11 desktop environment
exec dbus-run-session startplasma-x11
EOF
chmod +x "$HOME/.vnc/xstartup"
cp "$HOME/.vnc/xstartup" "$HOME/.config/tigervnc/xstartup"
# Write VNC config options (geometry, depth, session) to config file
cat << 'EOF' > "$HOME/.vnc/config"
geometry=1280x720
depth=24
session=custom
EOF
cp "$HOME/.vnc/config" "$HOME/.config/tigervnc/config"

# Clean up any stale sockets/locks from prior container runs
vncserver -kill :1 &>/dev/null || true
rm -f /tmp/.X1-lock /tmp/.X11-unix/X1

# Launch the VNC server on display :1 (TCP port 5901)
vncserver :1

# Keep the container running by tailing the TigerVNC logs
echo "=========================================================================="
echo " KDE Plasma 6 Desktop VNC server is running!"
echo " Connect using a VNC Viewer (e.g., Remmina, TigerVNC, RealVNC)"
echo " Host: localhost:5901"
echo " Password: scribble"
echo "=========================================================================="
tail -F "$HOME/.vnc"/*.log "$HOME/.config/tigervnc"/*.log 2>/dev/null
