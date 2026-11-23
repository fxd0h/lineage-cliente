#include "plataforma/lanzador.h"
#include <cstdio>
#include <thread>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

namespace lin {

namespace {

std::string ejecutar(const std::string& comando) {
    std::string salida;
    FILE* f = popen(comando.c_str(), "r");  // through the system shell, like a terminal line
    if (!f) return salida;
    char buf[256];
    while (std::fgets(buf, sizeof buf, f)) salida += buf;
    pclose(f);
    return salida;
}

}  // namespace

u16 puerto_en_salida(std::string_view salida) {
    u16 puerto = 0;
    for (size_t i = 0; i < salida.size();) {
        if (salida[i] < '0' || salida[i] > '9') { ++i; continue; }
        unsigned n = 0;
        size_t j = i;
        for (; j < salida.size() && salida[j] >= '0' && salida[j] <= '9'; ++j) n = n < 100000 ? n * 10 + (salida[j] - '0') : n;
        if (n >= 1 && n <= 65535) puerto = static_cast<u16>(n);
        i = j;
    }
    return puerto;
}

void Lanzador::iniciar(const std::string& comando) {
    estado_ = std::make_shared<Estado>();
    comando_ = comando;
    // Detached so closing the client never waits for a slow launcher; the shared state outlives the thread's use.
    std::thread([e = estado_, comando] {
        e->puerto = puerto_en_salida(ejecutar(comando));
        e->listo.store(true);
    }).detach();
}

bool Lanzador::terminado(u16& puerto) {
    if (!estado_ || !estado_->listo.load()) return false;
    puerto = estado_->puerto;
    estado_.reset();
    return true;
}

void Lanzador::parar() {
    if (comando_.empty()) return;
    ejecutar(comando_ + " stop");
    comando_.clear();
}

}  // namespace lin
