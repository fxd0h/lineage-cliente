// Pesca, como el cliente original (S3EP1U):
//  - S_Fishing (S_DoActionGFX con la acción 71 y la celda del corcho, manejador 0x487140) para el personaje propio pone el corcho
//    (gfx 6299) en esa celda, reproduce 4535.wav y muestra el cursor de pesca 16 (0x48721b);
//  - S_PacketBox 55 convierte el corcho en la picada (gfx 6385, 0x4918fb);
//  - un clic izquierdo en el mundo mientras está afuera manda C_FishClick con el flag de picada, reproduce 4536.wav si no hubo
//    picada, y la termina (0x469220);
//  - S_CharVisualUpdate del personaje propio (0x487000) y los mensajes del servidor 1136 / 1138 (0x484bf0) también la terminan.
#include "app/app.h"

namespace lin {

namespace {
constexpr int ACC_PESCA = 71;
constexpr u32 SONIDO_LANZAR = 4535, SONIDO_FALLO = 4536;
}  // namespace

void eventos_pesca(App& a, const Evento& e) {
    App::Pesca& p = a.pesca;
    switch (e.tipo) {
    case Ev::ACCION_OBJETO:
        if (e.v[0] == ACC_PESCA && e.id == a.mundo.jugador.id && !p.activa) {
            p = {true, false, e.v[1], e.v[2], a.entrada.ahora_ms};
            a.audio.efecto(SONIDO_LANZAR, 1.0f, a.entrada.ahora_ms);
        }
        break;
    case Ev::PESCA_PICADA:
        if (p.activa) p.picada = true;
        break;
    case Ev::CAMBIO_VISUAL:
        if (e.id == a.mundo.jugador.id) p.activa = false;
        break;
    case Ev::MENSAJE_SERVIDOR:
        if (e.v[MS_ID] == 1136 || e.v[MS_ID] == 1138) p.activa = false;
        break;
    default:
        break;
    }
}

bool clic_pesca(App& a) {
    App::Pesca& p = a.pesca;
    if (!p.activa) return false;
    if (!p.picada) a.audio.efecto(SONIDO_FALLO, 1.0f, a.entrada.ahora_ms);
    enviar(a, cmd::click_pesca(a.op, p.picada));
    p.activa = false;
    return true;
}

}  // namespace lin
