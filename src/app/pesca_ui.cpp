// Fishing, as the original client (S3EP1U):
//  - S_Fishing (S_DoActionGFX with action 71 and the bobber cell, handler 0x487140) for the own character puts the bobber
//    (gfx 6299) on that cell, plays 4535.wav and shows the fishing cursor 16 (0x48721b);
//  - S_PacketBox 55 turns the bobber into the bite (gfx 6385, 0x4918fb);
//  - a left click in the world while it is out sends C_FishClick with the bite flag, plays 4536.wav when there was no
//    bite, and ends it (0x469220);
//  - S_CharVisualUpdate of the own character (0x487000) and the server messages 1136 / 1138 (0x484bf0) end it too.
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
