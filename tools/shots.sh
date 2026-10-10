#!/bin/sh
# Renders every screenshot state into ./screenshots on a private virtual KWin, so nothing appears
# on the desktop. Usage: tools/shots.sh [filter]   (filter: substring of a state name)
set -e
cd "$(dirname "$0")/.."
SOCK=gomp-shots
if [ ! -S "$XDG_RUNTIME_DIR/$SOCK" ]; then
    kwin_wayland --virtual --no-lockscreen --socket "$SOCK" --width 2600 --height 1000 >/dev/null 2>&1 &
    sleep 2
fi
mkdir -p screenshots
GOMP_SHOT_FILTER="$1" WAYLAND_DISPLAY="$SOCK" QT_QPA_PLATFORM=wayland \
QT_LOGGING_RULES="*.debug=false;qml.debug=true;js.debug=true;qt.qpa.services=false" QT_FORCE_STDERR_LOGGING=1 \
    timeout 180 ./build/gomp --screenshots "$PWD/screenshots" 2>&1 | grep -v '^qml: saved' || true
