// Server launcher: a user command that opens a local proxy to a server and prints its port (see Servidor::lanzador).
#pragma once
#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include "core/tipos.h"

namespace lin {

class Lanzador {
public:
    // Runs the command in the background; a previous run still in flight is left to finish on its own.
    void iniciar(const std::string& comando);
    bool en_curso() const { return estado_ && !estado_->listo.load(); }
    // True once, when the run ends: puerto is the port it printed (0 = none, the command failed).
    bool terminado(u16& puerto);
    // Runs "<command> stop" for the last command started (blocking), and forgets it.
    void parar();

private:
    struct Estado {
        std::atomic<bool> listo{false};
        u16 puerto = 0;
    };
    std::shared_ptr<Estado> estado_;
    std::string comando_;  // last one started, for parar()
};

// The last number 1..65535 in a launcher's output ("port 25134" -> 25134); 0 if there is none.
u16 puerto_en_salida(std::string_view salida);

}  // namespace lin
