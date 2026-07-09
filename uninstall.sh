#!/usr/bin/env bash

# Exit immediately if any command exits with a non-zero status
set -e

# Resolve home directory
if [ -z "$HOME" ]; then
    HOME=$(getent passwd "$USER" | cut -d: -f6)
fi
if [ -z "$HOME" ]; then
    echo "Error: Could not resolve home directory (\$HOME)." >&2
    exit 1
fi

# Helper function to remove empty directory tree
remove_empty_dir() {
    local dir="$1"
    # Remove only if it exists, is a directory, and is empty
    if [ -d "$dir" ] && [ ! -L "$dir" ]; then
        if [ -z "$(ls -A "$dir")" ]; then
            echo "Removing empty directory: $dir"
            rmdir "$dir"
        fi
    fi
}

# Print usage
print_usage() {
    echo "Usage: $0 [options]"
    echo "Options:"
    echo "  -y, --yes, --non-interactive   Automatically uninstall without confirmation prompting"
    echo "  -h, --help                     Show this help message"
}

# Parse command line options
NON_INTERACTIVE=false
while [[ "$#" -gt 0 ]]; do
    case $1 in
        -y|--yes|--non-interactive)
            NON_INTERACTIVE=true
            shift
            ;;
        -h|--help)
            print_usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            print_usage
            exit 1
            ;;
    esac
done

# Check if interactive
CONFIRM=false
if [ "$NON_INTERACTIVE" = true ]; then
    CONFIRM=true
elif [ -t 0 ]; then
    read -p "Are you sure you want to uninstall Scribbleway from ~/.local? [y/N] " -n 1 -r || REPLY="n"
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        CONFIRM=true
    fi
else
    echo "Running in non-interactive environment without -y/--yes/--non-interactive. Aborting." >&2
    exit 1
fi

if [ "$CONFIRM" != true ]; then
    echo "Uninstallation canceled."
    exit 0
fi

echo "=== Uninstalling Scribbleway ==="

# 1. Delete binary
if [ -f "$HOME/.local/bin/scribbleway-overlay" ] || [ -L "$HOME/.local/bin/scribbleway-overlay" ]; then
    echo "Removing binary: $HOME/.local/bin/scribbleway-overlay"
    rm -f "$HOME/.local/bin/scribbleway-overlay"
fi

# 2. Delete QML backend plugin and metadata directories
for dir in \
    "$HOME/.local/lib/qml/org/kde/scribbleway" \
    "$HOME/.local/lib/x86_64-linux-gnu/qml/org/kde/scribbleway" \
    "$HOME/.local/share/plasma/plasmoids/org.kde.scribbleway"; do
    if [ -d "$dir" ] || [ -L "$dir" ]; then
        echo "Removing: $dir"
        rm -rf "$dir"
    fi
done

# 3. Delete desktop entries and autostart files
for file in \
    "$HOME/.local/share/applications/scribbleway.desktop" \
    "$HOME/.local/etc/xdg/autostart/scribbleway-autostart.desktop" \
    "$HOME/.config/autostart/scribbleway-autostart.desktop"; do
    if [ -f "$file" ] || [ -L "$file" ]; then
        echo "Removing file: $file"
        rm -f "$file"
    fi
done

# 4. Delete systemd environment configuration
if [ -f "$HOME/.config/environment.d/10-scribbleway.conf" ]; then
    echo "Removing systemd environment configuration..."
    rm -f "$HOME/.config/environment.d/10-scribbleway.conf"
    
    # Remove instantly from running session
    if command -v systemctl &>/dev/null; then
        if systemctl --user is-systemd-running &>/dev/null; then
            echo "Removing environment variables instantly from running systemd session..."
            systemctl --user unset-environment QML2_IMPORT_PATH QT_PLUGIN_PATH
            if command -v dbus-update-activation-environment &>/dev/null; then
                dbus-update-activation-environment --systemd QML2_IMPORT_PATH QT_PLUGIN_PATH &>/dev/null || true
            fi
        fi
    fi
fi
# Clean up empty parent directories
for dir in \
    "$HOME/.local/lib/qml/org/kde" \
    "$HOME/.local/lib/qml/org" \
    "$HOME/.local/lib/qml" \
    "$HOME/.local/lib/x86_64-linux-gnu/qml/org/kde" \
    "$HOME/.local/lib/x86_64-linux-gnu/qml/org" \
    "$HOME/.local/lib/x86_64-linux-gnu/qml" \
    "$HOME/.local/share/plasma/plasmoids" \
    "$HOME/.local/share/plasma" \
    "$HOME/.local/etc/xdg/autostart" \
    "$HOME/.local/etc/xdg" \
    "$HOME/.local/etc" \
    "$HOME/.config/environment.d"; do
    remove_empty_dir "$dir"
done

echo "Files and directories removed successfully."

# 4. Output shell environment variable cleanup instructions
echo
echo "=== Shell Environment Variable Cleanup ==="
echo "If you added environment variables during installation, they might still exist in your shell profiles."
echo "You can check ~/.bashrc and ~/.profile for the block labeled:"
echo "  # Added by Scribbleway installer"
echo
echo "To clean them up, manually edit those files and remove the block:"
echo "  # Added by Scribbleway installer"
echo "  export QML2_IMPORT_PATH=\"\$HOME/.local/lib/qml:\$HOME/.local/lib/x86_64-linux-gnu/qml\${QML2_IMPORT_PATH:+:\$QML2_IMPORT_PATH}\""
echo "  export QT_PLUGIN_PATH=\"\$HOME/.local/lib/plugins\${QT_PLUGIN_PATH:+:\$QT_PLUGIN_PATH}\""
echo
echo "Scribbleway uninstallation complete!"
