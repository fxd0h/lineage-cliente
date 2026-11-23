#!/bin/sh
# Lanzador que `cmake --install` deja en <prefix>/bin/lineage: abre <prefix>/share/lineage-cliente/lineage, que
# busca datos/ junto a sí (un enlace simbólico no serviría en todos los sistemas).
d=$(dirname "$0")
exec "$d/../share/lineage-cliente/lineage" "$@"
