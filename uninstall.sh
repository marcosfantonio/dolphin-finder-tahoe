#!/usr/bin/env bash
set -euo pipefail

cat <<'EOF'
DOLPHIN FINDER by fantonio — Uninstaller

This removes the user-installed Dolphin Finder launcher, profile, icon and
local installation. The source repository and /usr/bin/dolphin are preserved.
EOF

restore_default=false
for arg in "$@"; do
    case "$arg" in
        --restore-dolphin) restore_default=true ;;
        -h|--help)
            printf '\nUsage: ./uninstall.sh [--restore-dolphin]\n'
            printf '  --restore-dolphin  Set KDE Dolphin as the directory handler again.\n'
            exit 0
            ;;
        *) echo "Unknown option: $arg" >&2; exit 2 ;;
    esac
done

if [[ "$restore_default" == false && -t 0 ]]; then
    printf '\nRestore KDE Dolphin as the default directory handler? [y/N] '
    read -r answer
    [[ "$answer" =~ ^([yYsS]|[yYsS][eE][sS]|[yYsS][iI][mM])$ ]] && restore_default=true
fi

rm -f "$HOME/.local/bin/dolphin-finder"
rm -f "$HOME/.local/share/applications/org.kde.dolphin-finder.desktop"
rm -f "$HOME/.local/share/icons/hicolor/512x512/apps/org.kde.dolphin-finder.png"
rm -rf "$HOME/.local/opt/dolphin-finder"
rm -rf "$HOME/.local/share/dolphin-finder"

if [[ "$restore_default" == true ]] && command -v xdg-mime &>/dev/null; then
    xdg-mime default org.kde.dolphin.desktop inode/directory || true
    echo "KDE Dolphin was selected as the default directory handler."
else
    echo "The current default directory handler was left unchanged."
fi

if command -v update-desktop-database &>/dev/null; then
    update-desktop-database "$HOME/.local/share/applications" || true
fi

echo "Dolphin Finder user files removed."
echo "The source repository and system Dolphin were preserved."
