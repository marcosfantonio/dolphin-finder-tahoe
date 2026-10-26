#!/usr/bin/env bash
set -euo pipefail

cat <<'EOF'
DOLPHIN FINDER by fantonio — Uninstaller

This removes the user-installed Dolphin Finder launcher, profile, icon and
local installation. The source repository and other applications are preserved.
EOF

for arg in "$@"; do
    case "$arg" in
        -h|--help)
            printf '\nUsage: ./uninstall.sh\n'
            exit 0
            ;;
        *) echo "Unknown option: $arg" >&2; exit 2 ;;
    esac
done

host_config="${DOLPHIN_FINDER_SYSTEM_CONFIG_HOME:-${XDG_CONFIG_HOME:-$HOME/.config}}"
if command -v xdg-mime &>/dev/null &&
    [[ "$(XDG_CONFIG_HOME="$host_config" XDG_DATA_HOME="$HOME/.local/share" xdg-mime query default inode/directory)" == org.kde.dolphin-finder.desktop ]]; then
    echo "Dolphin Finder is the default file manager. Select another application in System Settings after uninstalling."
fi

rm -f "$HOME/.local/bin/dolphin-finder"
rm -f "$HOME/.local/share/applications/org.kde.dolphin-finder.desktop"
rm -f "$HOME/.local/share/icons/hicolor/512x512/apps/org.kde.dolphin-finder.png"
rm -rf "$HOME/.local/opt/dolphin-finder"
rm -rf "$HOME/.local/share/dolphin-finder"

if command -v update-desktop-database &>/dev/null; then
    update-desktop-database "$HOME/.local/share/applications" || true
fi

echo "Dolphin Finder user files removed."
echo "The source repository and other applications were preserved."
