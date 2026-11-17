#!/bin/bash
# Detiene el servidor L1JGO-Whale y su PostgreSQL.
cd "$(dirname "$0")"
# MULTIPLATFORM: Homebrew on macOS, apt layout on Linux.
if [ "$(uname)" = Darwin ]; then PG="$(brew --prefix postgresql@16)/bin"; else PG=/usr/lib/postgresql/16/bin; fi
pkill -f "bin/l1jgo" || true
"$PG/pg_ctl" -D datos -m fast stop >/dev/null 2>&1 || true
