#!/usr/bin/env bash

# Exit immediately if any command exits with a non-zero status
set -e

# Print commands and their arguments as they are executed (optional, but good for debugging)
# set -x

# Resolve home directory
if [ -z "$HOME" ]; then
    HOME=$(getent passwd "$USER" | cut -d: -f6)
fi
if [ -z "$HOME" ]; then
    echo "Error: Could not resolve home directory (\$HOME)." >&2
    exit 1
fi

# Print usage
print_usage() {
    echo "Usage: $0 [options]"
    echo "Options:"
    echo "  -y, --yes, --non-interactive   Automatically accept environment variable setup and run without prompting"
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

# Check for existence of required desktop files (Fail Fast)
if [ ! -f scribbleway.desktop ]; then
    echo "Error: scribbleway.desktop not found in current directory." >&2
    exit 1
fi
if [ ! -f scribbleway-autostart.desktop ]; then
    echo "Error: scribbleway-autostart.desktop not found in current directory." >&2
    exit 1
fi

# Check if cmake is available
if ! command -v cmake &> /dev/null; then
    echo "Error: cmake is required but not found in PATH." >&2
    exit 1
fi

echo "=== Configuring build ==="
cmake -B build -S . -DCMAKE_INSTALL_PREFIX="$HOME/.local" -DCMAKE_BUILD_TYPE=Release -DKDE_INSTALL_QMLDIR=lib/qml

echo "=== Building project ==="
NUM_CORES=$(nproc 2>/dev/null || echo 2)
cmake --build build -j"$NUM_CORES"

echo "=== Installing project ==="
# Clean up any conflicting dangling symlinks or directories from previous incomplete installations
for dir in "$HOME/.local/lib/qml/org/kde/scribbleway" "$HOME/.local/lib/x86_64-linux-gnu/qml/org/kde/scribbleway"; do
    if [ -L "$dir" ]; then
        echo "Removing conflicting symbolic link: $dir"
        rm -f "$dir"
    fi
done
cmake --install build

echo "=== Copying and adjusting Desktop files ==="
mkdir -p "$HOME/.local/share/applications"
mkdir -p "$HOME/.config/autostart"


# Replace Exec command with the absolute path to scribbleway-overlay in local bin
sed "s#Exec=scribbleway-overlay#Exec=\"$HOME/.local/bin/scribbleway-overlay\"#g" scribbleway.desktop > "$HOME/.local/share/applications/scribbleway.desktop"
sed "s#Exec=scribbleway-overlay#Exec=\"$HOME/.local/bin/scribbleway-overlay\"#g" scribbleway-autostart.desktop > "$HOME/.config/autostart/scribbleway-autostart.desktop"
chmod +x "$HOME/.local/share/applications/scribbleway.desktop"
chmod +x "$HOME/.config/autostart/scribbleway-autostart.desktop"

echo "Desktop files successfully configured."

# Helper function to check if the environment is already configured
check_already_configured() {
    # Check files for our specific block to be safe against future runs
    local files_configured=false
    for file in "$HOME/.bashrc" "$HOME/.profile"; do
        if [ -f "$file" ]; then
            if grep -q "Added by Scribbleway installer" "$file"; then
                files_configured=true
            fi
        fi
    done

    if [ "$files_configured" = true ]; then
        return 0
    fi
    return 1
}

# Helper function to apply environment profile updates
apply_env_updates() {
    local applied=false
    local has_profile=false

    # Check if at least one of the profiles exists as a file
    if [ -f "$HOME/.bashrc" ] || [ -f "$HOME/.profile" ]; then
        has_profile=true
    fi

    if [ "$has_profile" = false ]; then
        echo "Warning: Neither ~/.bashrc nor ~/.profile was found."
        echo "Creating ~/.profile to configure environment variables."
        touch "$HOME/.profile"
    fi

    # 1. Update shell profiles
    for file in "$HOME/.bashrc" "$HOME/.profile"; do
        if [ -f "$file" ]; then
            if grep -q "Added by Scribbleway installer" "$file"; then
                echo "Environment variables already configured in $file."
            else
                echo "Adding environment variables to $file..."
                cat << 'EOF' >> "$file"

# Added by Scribbleway installer
export QML2_IMPORT_PATH="$HOME/.local/lib/qml:$HOME/.local/lib/x86_64-linux-gnu/qml${QML2_IMPORT_PATH:+:$QML2_IMPORT_PATH}"
export QT_PLUGIN_PATH="$HOME/.local/lib/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
EOF
                applied=true
            fi
        fi
    done

    # 2. Configure systemd user session environment (for plasmashell and widgets)
    local env_dir="$HOME/.config/environment.d"
    local env_file="$env_dir/10-scribbleway.conf"
    
    mkdir -p "$env_dir"
    if [ -f "$env_file" ] && grep -q "Added by Scribbleway installer" "$env_file"; then
        echo "Systemd environment already configured in $env_file."
    else
        echo "Configuring systemd user session environment variables..."
        cat << EOF > "$env_file"
# Added by Scribbleway installer
QML2_IMPORT_PATH="$HOME/.local/lib/qml:$HOME/.local/lib/x86_64-linux-gnu/qml:\${QML2_IMPORT_PATH}"
QT_PLUGIN_PATH="$HOME/.local/lib/plugins:\${QT_PLUGIN_PATH}"
EOF
        applied=true
    fi

    # 3. Apply changes instantly to currently running systemd user session & DBus
    if command -v systemctl &>/dev/null; then
        if systemctl --user is-systemd-running &>/dev/null; then
            echo "Applying environment variables instantly to running systemd session..."
            systemctl --user set-environment QML2_IMPORT_PATH="$HOME/.local/lib/qml:$HOME/.local/lib/x86_64-linux-gnu/qml${QML2_IMPORT_PATH:+:$QML2_IMPORT_PATH}"
            systemctl --user set-environment QT_PLUGIN_PATH="$HOME/.local/lib/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
            if command -v dbus-update-activation-environment &>/dev/null; then
                dbus-update-activation-environment --systemd QML2_IMPORT_PATH QT_PLUGIN_PATH &>/dev/null || true
            fi
        fi
    fi

    if [ "$applied" = true ]; then
        echo "Environment variables added successfully!"
        echo "Please restart your desktop session or log out/in for changes to fully propagate."
    fi
}

echo "=== Checking Environment Variables ==="
# Determine interactive/non-interactive behavior
PROMPT_ENV=false
AUTO_ENV=false

if [ "$NON_INTERACTIVE" = true ]; then
    AUTO_ENV=true
elif [ -t 0 ]; then
    PROMPT_ENV=true
fi

if check_already_configured; then
    echo "QML/Qt environment variables are already configured."
else
    if [ "$AUTO_ENV" = true ]; then
        apply_env_updates
    elif [ "$PROMPT_ENV" = true ]; then
        echo
        read -p "Would you like to add the required QML/Qt environment variables to your shell profiles (~/.bashrc and ~/.profile)? [y/N] " -n 1 -r || REPLY="n"
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            apply_env_updates
        else
            echo "Skipped shell environment variable configuration."
            echo "Remember to set them manually if QML plugins fail to load:"
            echo "  export QML2_IMPORT_PATH=\"\$HOME/.local/lib/qml:\$HOME/.local/lib/x86_64-linux-gnu/qml\${QML2_IMPORT_PATH:+:\$QML2_IMPORT_PATH}\""
            echo "  export QT_PLUGIN_PATH=\"\$HOME/.local/lib/plugins\${QT_PLUGIN_PATH:+:\$QT_PLUGIN_PATH}\""
        fi
    else
        echo
        echo "Non-interactive mode: Skipping automatic shell profile updates."
        echo "To apply them automatically, run this script with --yes / --non-interactive."
        echo "Otherwise, add the following to your ~/.bashrc or ~/.profile:"
        echo "  export QML2_IMPORT_PATH=\"\$HOME/.local/lib/qml:\$HOME/.local/lib/x86_64-linux-gnu/qml\${QML2_IMPORT_PATH:+:\$QML2_IMPORT_PATH}\""
        echo "  export QT_PLUGIN_PATH=\"\$HOME/.local/lib/plugins\${QT_PLUGIN_PATH:+:\$QT_PLUGIN_PATH}\""
    fi
fi

echo
echo "Scribbleway installation complete!"
echo "You can run the application using 'scribbleway-overlay' if \$HOME/.local/bin is in your PATH."
