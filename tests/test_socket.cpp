// Sockets: el TCP del juego y las conexiones locales del control en vivo. Sin llamadas al sistema
// directas, para que corran igual en macOS, Linux y Windows: el lado "servidor" usa las conexiones
// locales (tcp:0 en 127.0.0.1).
#include <doctest/doctest.h>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <string>
#include "plataforma/socket.h"

using namespace lin;

namespace {

// Espera (con un límite de vueltas) a que la conexión local tenga algo o se cierre.
i64 recibir_con_espera(DescLocal d, char* buf, size_t n) {
    for (int i = 0; i < 1000000; ++i)
        if (const i64 r = local_recibir(d, buf, n); r != 0) return r;
    return 0;
}

DescLocal aceptar_con_espera(DescLocal escucha) {
    for (int i = 0; i < 1000000; ++i)
        if (const DescLocal d = local_aceptar(escucha); d != SIN_DESC) return d;
    return SIN_DESC;
}

// Escucha en 127.0.0.1, en un puerto libre que elige el sistema.
DescLocal escuchar(u16& puerto) {
    std::string donde;
    const DescLocal e = local_escuchar("tcp:0", 4, donde);
    if (e != SIN_DESC) puerto = static_cast<u16>(std::atoi(donde.c_str() + 14));  // "tcp:127.0.0.1:"
    return e;
}

}  // namespace

TEST_CASE("SocketTcp: conecta sin bloqueo y después lee y escribe bloqueante") {
    u16 puerto = 0;
    const DescLocal servidor = escuchar(puerto);
    REQUIRE(servidor != SIN_DESC);
    SocketTcp s;
    REQUIRE(s.conectar("127.0.0.1", puerto, 2000));
    const DescLocal cliente = aceptar_con_espera(servidor);
    REQUIRE(cliente != SIN_DESC);
    const char hola[4] = {1, 2, 3, 4};
    REQUIRE(local_enviar(cliente, hola, 2) == 2);
    REQUIRE(local_enviar(cliente, hola + 2, 2) == 2);
    u8 leido[4] = {};
    CHECK(s.leer_exacto(leido, 4));  // bloqueante otra vez: espera los 4 bytes
    CHECK(leido[3] == 4);
    CHECK(s.escribir(reinterpret_cast<const u8*>(hola), 4));
    char eco[4] = {};
    i64 total = 0;
    while (total < 4) {
        const i64 r = recibir_con_espera(cliente, eco + total, static_cast<size_t>(4 - total));
        if (r <= 0) break;
        total += r;
    }
    CHECK(total == 4);
    s.cerrar();
    s.cerrar();  // dos veces no cierra un descriptor ajeno
    char buf[4];
    CHECK(recibir_con_espera(cliente, buf, sizeof buf) == -1);  // el otro lado cerró
    local_cerrar(cliente);
    local_dejar_de_escuchar(servidor, "tcp:0");
}

TEST_CASE("SocketTcp: un puerto cerrado falla, y cancelar corta la espera") {
    u16 puerto = 0;
    const DescLocal e = escuchar(puerto);
    REQUIRE(e != SIN_DESC);
    local_dejar_de_escuchar(e, "tcp:0");  // nadie escucha en ese puerto
    SocketTcp s;
    CHECK_FALSE(s.conectar("127.0.0.1", puerto, 2000));
    std::atomic<bool> cancelar{true};
    const auto t0 = std::chrono::steady_clock::now();
    CHECK_FALSE(s.conectar("127.0.0.1", puerto, 5000, &cancelar));
    CHECK(std::chrono::steady_clock::now() - t0 < std::chrono::milliseconds(500));
}

TEST_CASE("control en vivo: TCP en 127.0.0.1 con puerto libre") {
    std::string donde;
    const DescLocal escucha = local_escuchar("tcp:0", 4, donde);
    REQUIRE(escucha != SIN_DESC);
    REQUIRE(donde.rfind("tcp:127.0.0.1:", 0) == 0);
    const int puerto = std::atoi(donde.c_str() + 14);
    CHECK(puerto > 0);
    CHECK(local_aceptar(escucha) == SIN_DESC);  // no bloquea si no hay nadie

    SocketTcp cliente;
    REQUIRE(cliente.conectar("127.0.0.1", static_cast<u16>(puerto)));
    const DescLocal c = aceptar_con_espera(escucha);
    REQUIRE(c != SIN_DESC);

    char buf[16] = {};
    CHECK(local_recibir(c, buf, sizeof buf) == 0);  // todavía no llegó nada
    REQUIRE(cliente.escribir(reinterpret_cast<const u8*>("estado\n"), 7));
    CHECK(recibir_con_espera(c, buf, sizeof buf) == 7);
    CHECK(std::string(buf, 7) == "estado\n");

    CHECK(local_enviar(c, "ok\n", 3) == 3);
    u8 r[3];
    REQUIRE(cliente.leer_exacto(r, 3));
    CHECK(std::string(reinterpret_cast<char*>(r), 3) == "ok\n");

    cliente.cerrar();
    CHECK(recibir_con_espera(c, buf, sizeof buf) == -1);  // el otro lado cerró
    local_cerrar(c);
    local_dejar_de_escuchar(escucha, "tcp:0");
}

#if !defined(_WIN32)
TEST_CASE("control en vivo: socket de archivo") {
    const char* tmp = std::getenv("TMPDIR");
    std::string ruta = tmp && *tmp && std::string(tmp).size() < 80 ? tmp : "/tmp/";  // sun_path: ~104 bytes
    if (ruta.back() != '/') ruta += '/';
    ruta += "lin-test-control.sock";
    std::string donde;
    const DescLocal escucha = local_escuchar(ruta, 4, donde);
    REQUIRE(escucha != SIN_DESC);
    CHECK(donde == ruta);
    CHECK(local_aceptar(escucha) == SIN_DESC);
    local_dejar_de_escuchar(escucha, ruta);

    // Una ruta que no entra en sockaddr_un da el motivo, sin abrir nada.
    CHECK(local_escuchar(std::string(200, 'x'), 4, donde) == SIN_DESC);
    CHECK(donde.find("demasiado larga") != std::string::npos);
}
#endif
