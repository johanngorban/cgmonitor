#!/usr/bin/env bash
# cgmonitor install script.
#
# Usage:
#   ./install.sh              # build + install to /usr/local
#   ./install.sh --prefix /opt
#   ./install.sh --uninstall
#   ./install.sh --no-systemd
#   ./install.sh --deps-only  # install deps and exit
#
# Supports: Arch, Debian/Ubuntu, Fedora/RHEL, Alpine.
# For other distros, install deps manually and run cmake + make yourself.

set -euo pipefail

PREFIX="/usr/local"
DO_SYSTEMD=1
DO_INSTALL=1
UNINSTALL=0
DEPS_ONLY=0
JOBS="$(nproc 2>/dev/null || echo 2)"
BUILD_DIR="build"

usage() {
    cat <<EOF
cgmonitor install.sh

Options:
  --prefix DIR     Install prefix (default /usr/local)
  --no-systemd     Do not install the systemd unit
  --deps-only      Install dependencies and stop
  --uninstall      Remove installed files
  --jobs N         Parallel make jobs (default $(nproc))
  -h, --help       Show this help
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --prefix)     PREFIX="$2"; shift 2 ;;
        --no-systemd) DO_SYSTEMD=0; shift ;;
        --deps-only)  DEPS_ONLY=1; shift ;;
        --uninstall)  UNINSTALL=1; shift ;;
        --jobs)       JOBS="$2"; shift 2 ;;
        -h|--help)    usage; exit 0 ;;
        *) echo "unknown arg: $1" >&2; usage; exit 2 ;;
    esac
done

SUDO=""
if [[ $EUID -ne 0 ]]; then
    if command -v sudo >/dev/null; then
        SUDO="sudo"
    fi
fi

# --- distro detection ----------------------------------------------------
DISTRO=""
if [[ -f /etc/os-release ]]; then
    . /etc/os-release
    DISTRO="${ID:-}"
    DISTRO_LIKE="${ID_LIKE:-}"
fi

install_deps() {
    case "$DISTRO" in
        arch|manjaro|endeavouros|cachyos)
            $SUDO pacman -S --needed --noconfirm \
                base-devel cmake pkgconf python \
                cjson libmicrohttpd sqlite git
            ;;
        debian|ubuntu|raspbian|linuxmint|pop)
            $SUDO apt-get update
            $SUDO apt-get install -y \
                build-essential cmake pkg-config python3 \
                libcjson-dev libmicrohttpd-dev libsqlite3-dev git
            ;;
        fedora|rhel|centos|rocky|almalinux)
            $SUDO dnf install -y \
                gcc make cmake pkgconf-pkg-config python3 \
                cjson-devel libmicrohttpd-devel sqlite-devel git
            ;;
        alpine)
            $SUDO apk add --no-cache \
                build-base cmake pkgconfig python3 \
                cjson-dev libmicrohttpd-dev sqlite-dev git
            ;;
        "")
            echo "Cannot detect distro. Install these manually: build tools, cmake," >&2
            echo "  pkg-config, python3, libcjson, libmicrohttpd, sqlite3." >&2
            return 1
            ;;
        *)
            # try to guess from ID_LIKE
            case "$DISTRO_LIKE" in
                *debian*|*ubuntu*)  DISTRO=debian install_deps; return ;;
                *fedora*|*rhel*)    DISTRO=fedora install_deps; return ;;
                *arch*)             DISTRO=arch   install_deps; return ;;
                *)
                    echo "Unsupported distro: $DISTRO (ID_LIKE=$DISTRO_LIKE)" >&2
                    echo "Install deps manually." >&2
                    return 1
                    ;;
            esac
            ;;
    esac
}

create_user() {
    if ! getent passwd cgmonitor >/dev/null 2>&1; then
        echo "Creating system user 'cgmonitor'…"
        $SUDO useradd --system --shell /usr/sbin/nologin \
                       --home-dir /var/lib/cgmonitor \
                       --create-home cgmonitor || true
    fi
    $SUDO mkdir -p /var/lib/cgmonitor /var/log/cgmonitor
    $SUDO chown -R cgmonitor:cgmonitor /var/lib/cgmonitor /var/log/cgmonitor || true
}

do_uninstall() {
    echo "Uninstalling…"
    if [[ $DO_SYSTEMD -eq 1 ]] && command -v systemctl >/dev/null; then
        $SUDO systemctl stop    cgmonitor.service 2>/dev/null || true
        $SUDO systemctl disable cgmonitor.service 2>/dev/null || true
    fi
    $SUDO rm -f  "$PREFIX/bin/cgmonitor"
    $SUDO rm -f  /lib/systemd/system/cgmonitor.service /etc/systemd/system/cgmonitor.service
    echo "Note: /etc/cgmonitor and /var/lib/cgmonitor are NOT removed."
    echo "Remove them manually if you don't want to keep the config and DB."
}

# --- main ---------------------------------------------------------------
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

if [[ $UNINSTALL -eq 1 ]]; then
    do_uninstall
    exit 0
fi

echo "==> Installing dependencies (distro: ${DISTRO:-unknown})"
install_deps || { echo "Dependency install failed."; exit 1; }

if [[ $DEPS_ONLY -eq 1 ]]; then
    echo "Deps installed. Stopping (--deps-only)."
    exit 0
fi

echo "==> Configuring CMake (prefix=$PREFIX)"
mkdir -p "$BUILD_DIR"
cmake -S . -B "$BUILD_DIR" \
      -DCMAKE_INSTALL_PREFIX="$PREFIX" \
      -DCMAKE_BUILD_TYPE=Release

echo "==> Building"
cmake --build "$BUILD_DIR" -j "$JOBS"

echo "==> Installing"
$SUDO cmake --install "$BUILD_DIR"

# Config file: only install if not already present.
ETC="/etc/cgmonitor"
$SUDO mkdir -p "$ETC"
if [[ ! -f "$ETC/cgmonitor.conf" ]]; then
    $SUDO cp "$PREFIX/etc/cgmonitor/cgmonitor.conf.example" "$ETC/cgmonitor.conf" 2>/dev/null \
        || $SUDO cp config/cgmonitor.conf.example "$ETC/cgmonitor.conf"
    echo "    Default config installed at $ETC/cgmonitor.conf"
else
    echo "    Existing config kept at $ETC/cgmonitor.conf"
fi

# systemd
if [[ $DO_SYSTEMD -eq 1 ]] && command -v systemctl >/dev/null; then
    create_user
    UNIT_DIR=/etc/systemd/system
    $SUDO cp systemd/cgmonitor.service "$UNIT_DIR/"
    $SUDO systemctl daemon-reload
    echo "==> systemd unit installed."
    echo "    Enable + start with:  sudo systemctl enable --now cgmonitor"
fi

echo
echo "Done. Start manually with:"
echo "  $PREFIX/bin/cgmonitor -c $ETC/cgmonitor.conf"
