#!/usr/bin/env bash
# Launch a TEST client instance on an invisible virtual display (Xvfb + lavapipe software Vulkan), so
# automated tests never show a window, steal focus or cover the user's screen.
# Usage: herramientas/lanzar_prueba.sh <settings.ini> [log file]
# Prints the PID of the client; its live-control socket is /tmp/lineage-control-<pid>.sock.
# Stop it with: kill <pid>   (never pkill: the user may be playing in another instance)
set -euo pipefail
raiz="$(cd "$(dirname "$0")/.." && pwd)"
config="${1:?usage: lanzar_prueba.sh <settings.ini> [log]}"
log="${2:-/dev/null}"
export LIN_CONFIG="$config" LIN_NO_FOCUS=1
export SDL_VIDEODRIVER=x11 VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json
# Software rendering is CPU-heavy: keep it to 2 threads at the lowest priority so the user's machine stays usable.
export LP_NUM_THREADS=2
setsid nice -n 19 xvfb-run -a -s "-screen 0 1280x1024x24" "$raiz/build/debug/lineage" > "$log" 2>&1 < /dev/null &
for _ in $(seq 50); do
    pid=$(pgrep -n -f "^$raiz/build/debug/lineage$" || true)
    [[ -n "$pid" && -S /tmp/lineage-control-$pid.sock ]] && { echo "$pid"; exit 0; }
    sleep 0.2
done
echo "lanzar_prueba: client did not start (see $log)" >&2
exit 1
