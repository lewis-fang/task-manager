#!/usr/bin/env bash
# KSAT - run script (Linux)
# Ensures the Qt MySQL driver is found when it is not installed system-wide
# (e.g. apt install libqt5sql5-mysql is missing). Falls back to the bundled
# driver in /tmp/opencode if present.
set -euo pipefail

cd "$(dirname "$0")"

BIN="${1:-build/KSAT}"
[ -x "$BIN" ] || { echo "Error: $BIN not found. Run: bash build.sh" >&2; exit 1; }

DRIVER_DIR="/tmp/opencode/qtdriver/usr/lib/x86_64-linux-gnu/qt5/plugins"
SYSTEM_MYSQL_DRIVER="/usr/lib/x86_64-linux-gnu/qt5/plugins/sqldrivers/libqsqlmysql.so"
if [ -d "$DRIVER_DIR" ] && [ ! -f "$SYSTEM_MYSQL_DRIVER" ]; then
    QT_PLUGIN_PATH="$DRIVER_DIR${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
    export QT_PLUGIN_PATH
fi

exec "$BIN" "$@"