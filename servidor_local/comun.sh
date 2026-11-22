# Funciones comunes (se usa con `source`) para macOS (Homebrew) y Linux (apt):
# instalar_paquetes, maria_bin, java_bin y sed_i.
SO="$(uname -s)"

instalar_paquetes() {
    if [ "$SO" = Darwin ]; then
        for p in openjdk mariadb; do
            brew list --versions "$p" >/dev/null || brew install "$p"
        done
    else
        local faltan=()
        command -v java >/dev/null || faltan+=(default-jre-headless)
        command -v mariadb-install-db >/dev/null || faltan+=(mariadb-server)
        command -v curl >/dev/null || faltan+=(curl)
        command -v git >/dev/null || faltan+=(git)
        if [ ${#faltan[@]} -gt 0 ]; then
            sudo apt-get update -qq && sudo apt-get install -y "${faltan[@]}"
        fi
    fi
}

# Binario de MariaDB por nombre (mariadb, mariadb-admin, mariadb-install-db, mariadbd...).
# En Linux los binarios están en el PATH (mariadbd en /usr/sbin).
maria_bin() {
    if [ "$SO" = Darwin ]; then
        echo "$(brew --prefix mariadb)/bin/$1"
    else
        command -v "$1" 2>/dev/null || { [ -x "/usr/sbin/$1" ] && echo "/usr/sbin/$1"; } || echo "$1"
    fi
}

java_bin() {
    if [ "$SO" = Darwin ]; then echo "$(brew --prefix openjdk)/bin/java"; else command -v java; fi
}

# sed -i portable (BSD exige argumento de sufijo, GNU no)
sed_i() { if [ "$SO" = Darwin ]; then sed -i '' "$@"; else sed -i "$@"; fi; }
