#include "plataforma/socket.h"

#include <cstring>

#if defined(_WIN32)
// Winsock2: mismas funciones que POSIX salvo WSAStartup, closesocket, ioctlsocket y WSAGetLastError.
// Windows no tiene SIGPIPE, así que send no necesita banderas.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#endif

namespace lin {

namespace {

#if defined(_WIN32)
using Nativo = SOCKET;
Nativo nativo(std::intptr_t d) { return static_cast<Nativo>(d); }
std::intptr_t opaco(Nativo s) { return s == INVALID_SOCKET ? -1 : static_cast<std::intptr_t>(s); }
void cerrar_nativo(Nativo s) { ::closesocket(s); }
bool no_bloquear(Nativo s, bool si = true) {
    u_long v = si ? 1 : 0;
    return ::ioctlsocket(s, FIONBIO, &v) == 0;
}
bool reintentar() { return false; }  // Winsock no interrumpe llamadas con señales
bool bloquearia() { return ::WSAGetLastError() == WSAEWOULDBLOCK; }
constexpr int CERRAR_AMBOS = SD_BOTH;
constexpr int SIN_SIGPIPE = 0;
std::string ultimo_error() { return "WSA " + std::to_string(::WSAGetLastError()); }
#else
using Nativo = int;
Nativo nativo(std::intptr_t d) { return static_cast<Nativo>(d); }
std::intptr_t opaco(Nativo s) { return s < 0 ? -1 : s; }
void cerrar_nativo(Nativo s) { ::close(s); }
bool no_bloquear(Nativo s, bool si = true) {
    const int f = ::fcntl(s, F_GETFL);
    return f >= 0 && ::fcntl(s, F_SETFL, si ? f | O_NONBLOCK : f & ~O_NONBLOCK) == 0;
}
bool reintentar() { return errno == EINTR; }
bool bloquearia() { return errno == EAGAIN || errno == EWOULDBLOCK; }
constexpr int CERRAR_AMBOS = SHUT_RDWR;
#if defined(__linux__)
constexpr int SIN_SIGPIPE = MSG_NOSIGNAL;  // Linux: evita SIGPIPE por envío
#else
constexpr int SIN_SIGPIPE = 0;  // macOS: SO_NOSIGPIPE en el socket
#endif
std::string ultimo_error() { return std::strerror(errno); }
#endif

// recv/send con el largo que pide cada sistema (int en Windows, size_t en POSIX).
i64 recibir(Nativo s, void* destino, size_t n) {
#if defined(_WIN32)
    return ::recv(s, static_cast<char*>(destino), static_cast<int>(n > 0x40000000 ? 0x40000000 : n), 0);
#else
    return ::recv(s, destino, n, 0);
#endif
}
i64 enviar(Nativo s, const void* datos, size_t n) {
#if defined(_WIN32)
    return ::send(s, static_cast<const char*>(datos), static_cast<int>(n > 0x40000000 ? 0x40000000 : n), 0);
#else
    return ::send(s, datos, n, SIN_SIGPIPE);
#endif
}

// connect sin bloqueo: espera en tramos de 100 ms (para poder cancelar) hasta `timeout_ms`, y deja
// el socket otra vez bloqueante (lo usan leer_exacto y escribir).
bool conectar_con_plazo(Nativo s, const sockaddr* dir, int largo, int timeout_ms, const std::atomic<bool>* cancelar) {
    if (!no_bloquear(s)) return false;
    bool ok = ::connect(s, dir, largo) == 0;
#if defined(_WIN32)
    const bool en_curso = !ok && ::WSAGetLastError() == WSAEWOULDBLOCK;
#else
    const bool en_curso = !ok && errno == EINPROGRESS;
#endif
    if (en_curso) {
        for (int esperado = 0; esperado < timeout_ms && !(cancelar && cancelar->load()); esperado += 100) {
#if defined(_WIN32)
            // select y no WSAPoll: WSAPoll no avisaba de un connect fallido hasta Windows 10 2004.
            // En Windows el fallo llega por el conjunto de excepciones.
            fd_set escribir, fallo;
            FD_ZERO(&escribir);
            FD_ZERO(&fallo);
            FD_SET(s, &escribir);
            FD_SET(s, &fallo);
            timeval tv{0, 100 * 1000};
            const int r = ::select(0, nullptr, &escribir, &fallo, &tv);
            if (r < 0) break;
#else
            pollfd p{s, POLLOUT, 0};
            const int r = ::poll(&p, 1, 100);
            if (r < 0 && errno != EINTR) break;
#endif
            if (r > 0) {
                int error = 0;
                socklen_t n = sizeof error;
                ok = getsockopt(s, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &n) == 0 && error == 0;
                break;
            }
        }
    }
    return ok && no_bloquear(s, false);
}

void sin_sigpipe(Nativo s) {
#if defined(__APPLE__)
    int uno = 1;
    setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &uno, sizeof uno);
#else
    (void)s;
#endif
}

}  // namespace

bool iniciar_red() {
#if defined(_WIN32)
    static const bool ok = [] {
        WSADATA d;
        return ::WSAStartup(MAKEWORD(2, 2), &d) == 0;
    }();
    return ok;
#else
    return true;
#endif
}

bool SocketTcp::conectar(const std::string& host, u16 puerto, int timeout_ms, const std::atomic<bool>* cancelar) {
    cerrar();
    if (!iniciar_red()) return false;
    addrinfo pista{};
    pista.ai_family = AF_UNSPEC;
    pista.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    std::string p = std::to_string(puerto);
    if (getaddrinfo(host.c_str(), p.c_str(), &pista, &res) != 0) return false;
    for (addrinfo* a = res; a && !(cancelar && cancelar->load()); a = a->ai_next) {
        Nativo s = ::socket(a->ai_family, a->ai_socktype, a->ai_protocol);
        if (opaco(s) < 0) continue;
        if (conectar_con_plazo(s, a->ai_addr, static_cast<int>(a->ai_addrlen), timeout_ms, cancelar)) {
            int uno = 1;
            setsockopt(s, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&uno), sizeof uno);
            sin_sigpipe(s);
            fd_ = opaco(s);
            break;
        }
        cerrar_nativo(s);
    }
    freeaddrinfo(res);
    return fd_ >= 0;
}

bool SocketTcp::leer_exacto(u8* destino, size_t n) {
    size_t leido = 0;
    while (leido < n) {
        const i64 r = recibir(nativo(fd_), destino + leido, n - leido);
        if (r <= 0) {
            if (r < 0 && reintentar()) continue;
            return false;
        }
        leido += static_cast<size_t>(r);
    }
    return true;
}

bool SocketTcp::escribir(const u8* datos, size_t n) {
    size_t enviado = 0;
    while (enviado < n) {
        const i64 r = enviar(nativo(fd_), datos + enviado, n - enviado);
        if (r <= 0) {
            if (r < 0 && reintentar()) continue;
            return false;
        }
        enviado += static_cast<size_t>(r);
    }
    return true;
}

void SocketTcp::cerrar() {
    // exchange: si dos hilos cierran a la vez, solo uno cierra el descriptor (el otro podría cerrar
    // uno ya reutilizado por otro archivo).
    if (const std::intptr_t fd = fd_.exchange(-1); fd >= 0) {
        ::shutdown(nativo(fd), CERRAR_AMBOS);
        cerrar_nativo(nativo(fd));
    }
}

// --- Conexiones locales del control en vivo ------------------------------------------------------

namespace {
bool es_tcp(const std::string& direccion) { return direccion.rfind("tcp:", 0) == 0; }
}  // namespace

DescLocal local_escuchar(const std::string& direccion, int max_conexiones, std::string& descripcion) {
    if (!iniciar_red()) {
        descripcion = "no se pudo iniciar la red";
        return SIN_DESC;
    }
    Nativo s;
    bool ok;
    if (es_tcp(direccion)) {
        // Solo 127.0.0.1: no se escucha en la red. Cualquier usuario de la máquina puede conectarse,
        // a diferencia del socket de archivo; el control solo existe en las versiones de desarrollo.
        sockaddr_in dir{};
        dir.sin_family = AF_INET;
        dir.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        dir.sin_port = htons(static_cast<u16>(std::strtoul(direccion.c_str() + 4, nullptr, 10)));
        s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        ok = opaco(s) >= 0 && ::bind(s, reinterpret_cast<sockaddr*>(&dir), sizeof dir) == 0 &&
             ::listen(s, max_conexiones) == 0;
        socklen_t largo = sizeof dir;
        if (ok && ::getsockname(s, reinterpret_cast<sockaddr*>(&dir), &largo) == 0)
            descripcion = "tcp:127.0.0.1:" + std::to_string(ntohs(dir.sin_port));
    } else {
#if defined(_WIN32)
        // AF_UNIX existe desde Windows 10 1803, pero Python en Windows no lo ofrece (lin_ctl.py).
        descripcion = "en Windows el control va por TCP (LIN_CONTROL=tcp:<puerto>)";
        return SIN_DESC;
#else
        sockaddr_un dir{};
        dir.sun_family = AF_UNIX;
        if (direccion.size() >= sizeof dir.sun_path) {
            descripcion = "la ruta es demasiado larga (" + std::to_string(direccion.size()) + " bytes): " + direccion;
            return SIN_DESC;
        }
        std::memcpy(dir.sun_path, direccion.c_str(), direccion.size() + 1);
        ::unlink(direccion.c_str());
        s = ::socket(AF_UNIX, SOCK_STREAM, 0);
        const mode_t antes = ::umask(0077);  // solo el usuario
        ok = s >= 0 && ::bind(s, reinterpret_cast<sockaddr*>(&dir), sizeof dir) == 0 && ::listen(s, max_conexiones) == 0;
        ::umask(antes);
        descripcion = direccion;
#endif
    }
    if (!ok || !no_bloquear(s)) {
        descripcion = "no se pudo abrir " + direccion + " (" + ultimo_error() + ")";
        if (opaco(s) >= 0) cerrar_nativo(s);
        return SIN_DESC;
    }
    return opaco(s);
}

DescLocal local_aceptar(DescLocal escucha) {
    const Nativo s = ::accept(nativo(escucha), nullptr, nullptr);
    if (opaco(s) < 0) return SIN_DESC;
    // En Windows el socket aceptado hereda el modo no bloqueante; en POSIX no.
    no_bloquear(s);
    sin_sigpipe(s);
    return opaco(s);
}

i64 local_recibir(DescLocal d, char* destino, size_t n) {
    for (;;) {
        const i64 r = recibir(nativo(d), destino, n);
        if (r > 0) return r;
        if (r == 0) return -1;  // el otro lado cerró
        if (reintentar()) continue;
        return bloquearia() ? 0 : -1;
    }
}

i64 local_enviar(DescLocal d, const char* datos, size_t n) {
    for (;;) {
        const i64 r = enviar(nativo(d), datos, n);
        if (r >= 0) return r;
        if (reintentar()) continue;
        return bloquearia() ? 0 : -1;
    }
}

void local_cerrar(DescLocal d) {
    if (d >= 0) cerrar_nativo(nativo(d));
}

void local_dejar_de_escuchar(DescLocal escucha, const std::string& direccion) {
    if (escucha < 0) return;
    cerrar_nativo(nativo(escucha));
#if !defined(_WIN32)
    if (!es_tcp(direccion)) ::unlink(direccion.c_str());
#else
    (void)direccion;
#endif
}

}  // namespace lin
