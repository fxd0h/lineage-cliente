// Socket TCP bloqueante mínimo para el hilo de red, y conexiones locales no bloqueantes para el
// control en vivo. Los descriptores son opacos (intptr_t): int en POSIX, SOCKET en Windows.
#pragma once
#include <atomic>
#include <cstdint>
#include <string>
#include "core/tipos.h"

namespace lin {

// Inicia la red del sistema una vez (WSAStartup en Windows; nada en POSIX). Se puede llamar desde
// cualquier hilo y las veces que haga falta.
bool iniciar_red();

class SocketTcp {
public:
    ~SocketTcp() { cerrar(); }
    // Espera la conexión hasta `timeout_ms`; si `cancelar` se pone en true, abandona en ≤ 100 ms.
    // shortcut: la resolución del nombre (getaddrinfo) no se puede cancelar; con una IP es inmediata.
    bool conectar(const std::string& host, u16 puerto, int timeout_ms = 5000, const std::atomic<bool>* cancelar = nullptr);
    // Lee exactamente n bytes (bloquea). false si se cerró la conexión o hubo error.
    bool leer_exacto(u8* destino, size_t n);
    bool escribir(const u8* datos, size_t n);
    // Desbloquea un leer_exacto pendiente desde otro hilo (shutdown) y libera el socket.
    void cerrar();

private:
    // Atómico: cerrar() llega desde otro hilo que el que conecta y lee.
    std::atomic<std::intptr_t> fd_{-1};
};

// Conexiones locales del control en vivo (solo desarrollo). `direccion` es la ruta de un socket de
// archivo (AF_UNIX, solo POSIX) o "tcp:<puerto>" en 127.0.0.1 (puerto 0: uno libre). Todo es no
// bloqueante y se revisa una vez por frame.
using DescLocal = std::intptr_t;
constexpr DescLocal SIN_DESC = -1;

// Abre la escucha. En `descripcion` deja dónde quedó (la ruta, o "tcp:127.0.0.1:<puerto>") o, si
// falla, el motivo.
DescLocal local_escuchar(const std::string& direccion, int max_conexiones, std::string& descripcion);
DescLocal local_aceptar(DescLocal escucha);  // SIN_DESC si no hay ninguna esperando
// Bytes leídos; 0 si todavía no llegó nada; -1 si la conexión se cerró o falló.
i64 local_recibir(DescLocal d, char* destino, size_t n);
// Bytes enviados; 0 si el buffer del sistema está lleno; -1 si falló.
i64 local_enviar(DescLocal d, const char* datos, size_t n);
void local_cerrar(DescLocal d);
// Cierra la escucha y, si era un socket de archivo, lo borra.
void local_dejar_de_escuchar(DescLocal escucha, const std::string& direccion);

}  // namespace lin
