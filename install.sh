#!/usr/bin/env bash
set -euo pipefail

cat <<'EOF'

 ____   ___  _     ____  _   _ ___ _   _   _____ ___ _   _ ____  _____ ____
|  _ \ / _ \| |   |  _ \| | | |_ _| \ | | |  ___|_ _| \ | |  _ \| ____|  _ \
| | | | | | | |   | |_) | | | || ||  \| | | |_   | ||  \| | | | |  _| | |_) |
| |_| | |_| | |___|  __/| |_| || || |\  | |  _|  | || |\  | |_| | |___|  _ <
|____/ \___/|_____|_|    \___/|___|_| \_| |_|   |___|_| \_|____/|_____|_| \_\

                 DOLPHIN FINDER by fantonio

EOF

install_deps=false
default_choice="ask"
for arg in "$@"; do
    case "$arg" in
        --install-deps) install_deps=true ;;
        --default) default_choice=yes ;;
        --no-default) default_choice=no ;;
        -h|--help)
            cat <<'EOF'
Usage: ./install.sh [options]

  --install-deps  Install missing CachyOS/Arch dependencies with pacman.
  --default       Make Dolphin Finder the default directory handler.
  --no-default    Keep the current default file manager.
EOF
            exit 0
            ;;
        *) echo "Unknown option: $arg" >&2; exit 2 ;;
    esac
done

if [[ ! -f /etc/arch-release ]] && ! grep -qi cachyos /etc/os-release 2>/dev/null; then
    echo "Warning: this installer targets CachyOS/Arch Linux." >&2
fi

packages=(base-devel cmake extra-cmake-modules ninja qt6-base qt6-multimedia qt6-tools qt6-svg
    kcmutils knewstuff kcoreaddons ki18n kdbusaddons kbookmarks kconfig kio kparts
    solid kiconthemes kcompletion ktextwidgets knotifications kcrash kwindowsystem
    kwidgetsaddons kcodecs kguiaddons kcolorscheme kfilemetadata baloo baloo-widgets)
missing=()
for package in "${packages[@]}"; do
    pacman -Q "$package" &>/dev/null || missing+=("$package")
done
if ((${#missing[@]})); then
    printf 'Missing dependencies: %s\n' "${missing[*]}"
    if [[ "$install_deps" == true ]]; then
        sudo pacman -S --needed "${missing[@]}"
    else
        printf 'Run again with --install-deps to install them.\n' >&2
        exit 1
    fi
fi

repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
build_dir="${BUILD_DIR:-$repo_dir/build}"
prefix="${DOLPHIN_FINDER_PREFIX:-$HOME/.local/opt/dolphin-finder}"

current_commit="unknown"
if command -v git &>/dev/null && git -C "$repo_dir" rev-parse HEAD &>/dev/null; then
    current_commit="$(git -C "$repo_dir" rev-parse HEAD)"
fi
installed_commit_file="$prefix/share/dolphin-finder/installed-commit"
installed_commit=""
if [[ -f "$installed_commit_file" ]]; then
    installed_commit="$(head -n 1 "$installed_commit_file")"
fi

if [[ -n "$installed_commit" && "$installed_commit" != "$current_commit" && "$current_commit" != unknown ]]; then
    printf '\nAn installed Dolphin Finder version was found.\n'
    printf 'Installed commit: %s\n' "${installed_commit:0:12}"
    printf 'Available commit:  %s\n' "${current_commit:0:12}"
    if [[ -t 0 ]]; then
        printf 'Update the installed application now? [Y/n] '
        read -r answer
        case "$answer" in
            [nN]|[nN][oO])
                echo "Installation cancelled; the current version was preserved."
                exit 0
                ;;
        esac
    else
        echo "Non-interactive install: updating automatically."
    fi
fi

cmake -S "$repo_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTING=OFF \
    -DKDE_INSTALL_USE_QT_SYS_PATHS=OFF \
    -DKDE_INSTALL_PLUGINDIR=lib/plugins -DKDE_INSTALL_QMLDIR=lib/qml \
    -DCMAKE_INSTALL_PREFIX="$prefix"
cmake --build "$build_dir" --parallel "${JOBS:-2}"
cmake --install "$build_dir"
# Remove legacy generic FileManager1 activation files from earlier installs.
rm -f "$prefix/share/dbus-1/services/org.kde.dolphin.FileManager1.service" \
    "$prefix/share/dbus-1/services/org.freedesktop.FileManager1.service" \
    "$prefix/lib/systemd/user/plasma-dolphin.service"
# Migrate only this application's private prefix; never modify system entries.
rm -f "$prefix/share/applications/org.kde.dolphin.desktop" \
    "$prefix/share/metainfo/org.kde.dolphin.appdata.xml" \
    "$prefix/share/icons/hicolor/scalable/apps/org.kde.dolphin.svg"
mkdir -p "$prefix/share/dolphin-finder"
printf '%s\n' "$current_commit" > "$prefix/share/dolphin-finder/installed-commit"

profile="$HOME/.local/share/dolphin-finder"
icon_dir="$HOME/.local/share/icons/hicolor/512x512/apps"
mkdir -p "$profile/config" "$profile/data" "$profile/cache" "$profile/state" \
    "$HOME/.local/bin" "$HOME/.local/share/applications" "$icon_dir"

icon_source="$repo_dir/src/Dolphin.png"
cp -f "$icon_source" "$icon_dir/org.kde.dolphin-finder.png"

if [[ ! -e "$profile/config/kdeglobals" && -f "$HOME/.config/kdeglobals" ]]; then
    cp -a "$HOME/.config/kdeglobals" "$profile/config/kdeglobals"
fi
if [[ ! -e "$profile/config/Kvantum" && -d "$HOME/.config/Kvantum" ]]; then
    cp -a "$HOME/.config/Kvantum" "$profile/config/Kvantum"
fi
if [[ ! -e "$profile/config/dolphinrc" ]]; then
    printf '[General]\nShowSelectionToggle=false\n' > "$profile/config/dolphinrc"
fi

cat > "$HOME/.local/bin/dolphin-finder" <<EOF
#!/usr/bin/env bash
set -euo pipefail
prefix="$prefix"
profile="\${DOLPHIN_FINDER_PROFILE:-\$HOME/.local/share/dolphin-finder}"
export DOLPHIN_FINDER_SYSTEM_CONFIG_HOME="\${DOLPHIN_FINDER_SYSTEM_CONFIG_HOME:-\${XDG_CONFIG_HOME:-\$HOME/.config}}"
export XDG_CONFIG_HOME="\$profile/config"
export XDG_DATA_HOME="\$profile/data"
export XDG_CACHE_HOME="\$profile/cache"
export XDG_STATE_HOME="\$profile/state"
export XDG_DATA_DIRS="\$HOME/.local/share:\${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
export QT_PLUGIN_PATH="\$prefix/lib/plugins:\${QT_PLUGIN_PATH:-}"
export QML2_IMPORT_PATH="\$prefix/lib/qml:\${QML2_IMPORT_PATH:-}"
exec "\$prefix/bin/dolphin-finder" --new-window "\$@"
EOF
chmod +x "$HOME/.local/bin/dolphin-finder"

cat > "$HOME/.local/share/applications/org.kde.dolphin-finder.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Dolphin Finder
Comment=Finder-inspired KDE file manager
Exec=$HOME/.local/bin/dolphin-finder %U
Icon=$icon_dir/org.kde.dolphin-finder.png
Terminal=false
Categories=Qt;KDE;System;FileTools;FileManager;
MimeType=inode/directory;
StartupNotify=true
StartupWMClass=dolphin-finder
DBusActivatable=false
EOF

command -v update-desktop-database &>/dev/null && update-desktop-database "$HOME/.local/share/applications" || true

if [[ "$default_choice" == ask && -t 0 ]]; then
    printf '\nMake Dolphin Finder the default file manager? [y/N] '
    read -r answer
    [[ "$answer" =~ ^([yYsS]|[yYsS][eE][sS]|[yYsS][iI][mM])$ ]] && default_choice=yes || default_choice=no
fi
if [[ "$default_choice" == yes ]]; then
    # The installer can be run from Finder's terminal, which inherits its isolated
    # XDG_CONFIG_HOME. Write the association to the user's real desktop instead.
    host_config="${DOLPHIN_FINDER_SYSTEM_CONFIG_HOME:-${XDG_CONFIG_HOME:-$HOME/.config}}"
    if ! command -v xdg-mime &>/dev/null; then
        echo "xdg-mime is required to set the default file manager." >&2
        exit 1
    fi
    if ! XDG_CONFIG_HOME="$host_config" XDG_DATA_HOME="$HOME/.local/share" \
        xdg-mime default org.kde.dolphin-finder.desktop inode/directory; then
        echo "Failed to set Dolphin Finder as the default directory handler." >&2
        exit 1
    fi
    if [[ "$(XDG_CONFIG_HOME="$host_config" XDG_DATA_HOME="$HOME/.local/share" xdg-mime query default inode/directory)" != org.kde.dolphin-finder.desktop ]]; then
        echo "Dolphin Finder was not registered as the default directory handler." >&2
        exit 1
    fi
    echo "Dolphin Finder is now the default directory handler."
else
    echo "The current default file manager was preserved."
fi
printf '\nInstalled Dolphin Finder with the Dolphin Finder icon.\n'
