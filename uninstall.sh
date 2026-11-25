#!/usr/bin/env bash
# Remove oos-rpi-lcd35.
#
#   sudo ./uninstall.sh            stop and remove the service and program
#   sudo ./uninstall.sh --purge    also remove /etc/oos-rpi-lcd35
set -euo pipefail

NAME=oos-rpi-lcd35
PURGE=0

for arg in "$@"; do
    case "$arg" in
        --purge)   PURGE=1 ;;
        -h|--help) sed -n '2,5p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "Unknown option: $arg" >&2; exit 2 ;;
    esac
done

if [ "$(id -u)" -ne 0 ]; then
    echo "Run as root: sudo ./uninstall.sh" >&2
    exit 1
fi

systemctl disable --now "$NAME" 2>/dev/null || true
rm -f "/etc/systemd/system/$NAME.service" "/usr/local/bin/$NAME"
systemctl daemon-reload

if [ "$PURGE" -eq 1 ]; then
    rm -rf "/etc/$NAME"
    echo "$NAME removed, including its settings."
else
    echo "$NAME removed. Settings kept in /etc/$NAME (use --purge to remove them)."
fi
