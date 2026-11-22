#!/usr/bin/env bash
# Lanza una instancia de PRUEBA del cliente en una pantalla virtual invisible (Xvfb + Vulkan por software lavapipe), para
# que las pruebas automáticas nunca muestren una ventana, roben el foco ni tapen la pantalla del usuario.
# Uso: herramientas/lanzar_prueba.sh <settings.ini> [archivo de log]
# Imprime el PID del cliente; su socket de control en vivo es /tmp/lineage-control-<pid>.sock.
# Se detiene con: kill <pid>   (nunca pkill: el usuario puede estar jugando en otra instancia)
set -euo pipefail
raiz="$(cd "$(dirname "$0")/.." && pwd)"
config="${1:?usage: lanzar_prueba.sh <settings.ini> [log]}"
log="${2:-/dev/null}"
export LIN_CONFIG="$config" LIN_NO_FOCUS=1
export SDL_VIDEODRIVER=x11 VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json
# El render por software usa mucha CPU: se limita a 2 hilos con la prioridad más baja para que la máquina siga usable.
export LP_NUM_THREADS=2
setsid nice -n 19 xvfb-run -a -s "-screen 0 1280x1024x24" "$raiz/build/debug/lineage" > "$log" 2>&1 < /dev/null &
for _ in $(seq 50); do
    pid=$(pgrep -n -f "^$raiz/build/debug/lineage$" || true)
    [[ -n "$pid" && -S /tmp/lineage-control-$pid.sock ]] && { echo "$pid"; exit 0; }
    sleep 0.2
done
echo "lanzar_prueba: client did not start (see $log)" >&2
exit 1
