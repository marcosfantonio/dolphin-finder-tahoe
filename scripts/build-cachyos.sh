#!/usr/bin/env bash
set -euo pipefail

usage() {
    cat <<'EOF'
Uso: ./scripts/build-cachyos.sh [--install-deps]

  --install-deps  Instala dependências ausentes com sudo pacman.
EOF
}

install_deps=false
for arg in "$@"; do
    case "$arg" in
        --install-deps) install_deps=true ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Opção desconhecida: $arg" >&2; usage >&2; exit 2 ;;
    esac
done

if [[ ! -f /etc/arch-release ]] && ! grep -qi cachyos /etc/os-release 2>/dev/null; then
    echo "Aviso: este script foi feito para CachyOS/Arch Linux e pacman." >&2
fi

packages=(
    base-devel cmake extra-cmake-modules ninja qt6-base qt6-multimedia qt6-tools
    kcmutils knewstuff kcoreaddons ki18n kdbusaddons kbookmarks kconfig kio
    kparts solid kiconthemes kcompletion ktextwidgets knotifications kcrash
    kwindowsystem kwidgetsaddons kcodecs kguiaddons kcolorscheme kfilemetadata
    baloo baloo-widgets
)

missing=()
for package in "${packages[@]}"; do
    if ! pacman -Q "$package" &>/dev/null; then
        missing+=("$package")
    fi
done

if ((${#missing[@]})); then
    printf 'Dependências ausentes: %s\n' "${missing[*]}" >&2
    if [[ "$install_deps" == true ]]; then
        sudo pacman -S --needed "${missing[@]}"
    else
        printf '\nInstale-as com:\n  sudo pacman -S --needed %s\n\n' "${missing[*]}" >&2
        echo "Ou rode novamente com --install-deps." >&2
        exit 1
    fi
fi

repo_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${BUILD_DIR:-$repo_dir/build}"
prefix="${DOLPHIN_FINDER_PREFIX:-$HOME/.local/opt/dolphin-finder}"
jobs="${JOBS:-2}"

cmake -S "$repo_dir" -B "$build_dir" -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DBUILD_TESTING=OFF \
    -DCMAKE_INSTALL_PREFIX="$prefix"
cmake --build "$build_dir" --parallel "$jobs"
cmake --install "$build_dir"

profile="$HOME/.local/share/dolphin-finder-test"
mkdir -p "$profile/config" "$profile/data" "$profile/cache" "$profile/state" \
    "$HOME/.local/bin" "$HOME/.local/share/applications"

# Seed the isolated profile once, preserving any existing per-app choices.
if [[ ! -e "$profile/config/kdeglobals" && -f "$HOME/.config/kdeglobals" ]]; then
    cp -a "$HOME/.config/kdeglobals" "$profile/config/kdeglobals"
fi
if [[ ! -e "$profile/config/Kvantum" && -d "$HOME/.config/Kvantum" ]]; then
    cp -a "$HOME/.config/Kvantum" "$profile/config/Kvantum"
fi
if [[ ! -e "$profile/config/dolphinrc" ]]; then
    cat > "$profile/config/dolphinrc" <<'EOF'
[General]
ShowSelectionToggle=false
EOF
fi

cat > "$HOME/.local/bin/dolphin-finder" <<EOF
#!/usr/bin/env bash
set -euo pipefail
prefix="$prefix"
profile="\${DOLPHIN_FINDER_PROFILE:-\$HOME/.local/share/dolphin-finder-test}"
export XDG_CONFIG_HOME="\$profile/config"
export XDG_DATA_HOME="\$profile/data"
export XDG_CACHE_HOME="\$profile/cache"
export XDG_STATE_HOME="\$profile/state"
export XDG_DATA_DIRS="\$HOME/.local/share:\${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
export QT_PLUGIN_PATH="\$prefix/lib/plugins:\${QT_PLUGIN_PATH:-}"
export QML2_IMPORT_PATH="\$prefix/lib/qml:\${QML2_IMPORT_PATH:-}"
exec "\$prefix/bin/dolphin" --new-window "\$@"
EOF
chmod +x "$HOME/.local/bin/dolphin-finder"

cat > "$HOME/.local/share/applications/org.kde.dolphin-finder.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Dolphin Finder (Teste)
Comment=Dolphin com painel e controles integrados estilo macOS
Exec=$HOME/.local/bin/dolphin-finder %U
Icon=org.kde.dolphin
Terminal=false
Categories=Qt;KDE;System;FileTools;FileManager;
StartupNotify=true
StartupWMClass=org.kde.dolphin-finder
EOF

if command -v update-desktop-database &>/dev/null; then
    update-desktop-database "$HOME/.local/share/applications" || true
fi

printf '\nInstalação concluída. Execute: %s\n' "$HOME/.local/bin/dolphin-finder"
printf 'O Dolphin do sistema não foi substituído.\n'
