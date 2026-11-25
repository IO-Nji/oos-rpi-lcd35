#!/usr/bin/env bash
# Build and install oos-rpi-lcd35 as a systemd service. Safe to run again to upgrade.
#
#   sudo ./install.sh              build, install, enable and start
#   sudo ./install.sh --no-deps    do not install build tools through the package manager
#   sudo ./install.sh --no-start   install without enabling or starting the service
#
# Packagers: set DESTDIR to stage the files without touching the running system.
set -euo pipefail

NAME=oos-rpi-lcd35
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DESTDIR="${DESTDIR:-}"
INSTALL_DEPS=1
START=1

for arg in "$@"; do
    case "$arg" in
        --no-deps)  INSTALL_DEPS=0 ;;
        --no-start) START=0 ;;
        -h|--help)  sed -n '2,8p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "Unknown option: $arg" >&2; exit 2 ;;
    esac
done

say() { printf '\n==> %s\n' "$*"; }

if [ -n "$DESTDIR" ]; then
    INSTALL_DEPS=0
    START=0
elif [ "$(id -u)" -ne 0 ]; then
    echo "Run as root: sudo ./install.sh" >&2
    exit 1
fi

missing_tools() {
    command -v cmake >/dev/null && command -v g++ >/dev/null && command -v make >/dev/null && return 1
    return 0
}

if missing_tools; then
    if [ "$INSTALL_DEPS" -eq 0 ]; then
        echo "Missing build tools. Needed: g++, make, cmake." >&2
        exit 1
    fi
    say "Installing build tools"
    if command -v apt-get >/dev/null; then
        apt-get update
        apt-get install -y build-essential cmake
    elif command -v dnf >/dev/null; then
        dnf install -y gcc-c++ make cmake
    elif command -v pacman >/dev/null; then
        pacman -S --needed --noconfirm base-devel cmake
    elif command -v zypper >/dev/null; then
        zypper --non-interactive install gcc-c++ make cmake
    else
        echo "Unknown package manager. Install g++, make and cmake yourself, then run:" >&2
        echo "sudo ./install.sh --no-deps" >&2
        exit 1
    fi
fi

# Build as the invoking user so the build directory is not left owned by root
as_builder() {
    if [ "$(id -u)" -eq 0 ] && [ -n "${SUDO_USER:-}" ] && [ "$SUDO_USER" != root ]; then
        sudo -u "$SUDO_USER" -- "$@"
    else
        "$@"
    fi
}

if [ ! -e "$SRC/core/CMakeLists.txt" ]; then
    say "Fetching the shared dashboard code"
    as_builder git -C "$SRC" submodule update --init
fi

# One compiler at a time per 500 MB of memory: a Pi 3A+ has 512 MB
JOBS=$(( $(awk '/MemTotal/ {print $2}' /proc/meminfo) / 500000 ))
[ "$JOBS" -ge 1 ] || JOBS=1
[ "$JOBS" -le "$(nproc)" ] || JOBS=$(nproc)

say "Building (this takes several minutes on a small Pi)"
as_builder cmake -S "$SRC" -B "$SRC/build" -DCMAKE_BUILD_TYPE=Release >/dev/null
as_builder cmake --build "$SRC/build" --parallel "$JOBS"

say "Installing"
install -Dm755 "$SRC/build/$NAME" "$DESTDIR/usr/local/bin/$NAME"
install -Dm644 "$SRC/packaging/$NAME.service" "$DESTDIR/etc/systemd/system/$NAME.service"
if [ -e "$DESTDIR/etc/$NAME/config.conf" ]; then
    echo "Keeping existing /etc/$NAME/config.conf"
else
    install -Dm644 "$SRC/packaging/config.conf" "$DESTDIR/etc/$NAME/config.conf"
fi

if [ "$START" -eq 0 ]; then
    [ -n "$DESTDIR" ] || systemctl daemon-reload
    echo "Installed. Start it with: sudo systemctl enable --now $NAME"
    exit 0
fi

say "Starting the service"
systemctl daemon-reload
systemctl enable "$NAME" >/dev/null
systemctl restart "$NAME"

sleep 3
if systemctl is-active --quiet "$NAME"; then
    echo "$NAME is running. Settings: /etc/$NAME/config.conf"
else
    echo "$NAME did not stay running. Recent log:" >&2
    journalctl -u "$NAME" -n 15 --no-pager >&2 || true
    exit 1
fi
