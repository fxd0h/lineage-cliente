// Cliente Lineage multiplataforma (entregables 1–4: login, personajes, moverse, chat y estado).
#include <SDL3/SDL_main.h>  // Windows: WinMain para el subsistema GUI (WIN32_EXECUTABLE); en macOS no cambia nada
#include "app/app.h"

int main(int, char**) {
    lin::App app;
    if (!lin::iniciar_app(app)) {
        lin::cerrar_app(app);
        return 1;
    }
    lin::bucle_app(app);
    lin::cerrar_app(app);
    return 0;
}
