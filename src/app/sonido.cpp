// Entregable 10: música por zona, sonidos de ambiente y sonidos de las animaciones.
//  - Música: la del login (music.def "Login") fuera del juego; dentro, la de la zona donde está
//    el personaje (se recalcula cada medio segundo), y la de "Death" mientras está muerto.
//  - Ambiente: el MAIN_SOUND de la zona en bucle y, de cada grupo de ONE_SOUND, un sonido al azar
//    cada tantos segundos (entre la mitad y una vez y media del intervalo del grupo). De día o de
//    noche según la hora del juego (S_GameTime; noche fuera de 6:00–17:59, como L1GameTime).
//  - Animaciones: los frames de la tabla de sprites con sonido ("[n" o "<n") suenan al empezar a
//    dibujarse; más bajo cuanto más lejos del personaje (nada a 20 celdas).
#include <cstdlib>
#include "app/app.h"

namespace lin {

namespace {

constexpr i32 SONIDO_AGUA = 330;  // el ambiente bajo el agua (0x14a en el cliente original)

constexpr u64 MS_ZONA = 500;
constexpr double CELDAS_SILENCIO = 20;
// El valle escondido de esta versión (2005) es una copia del mapa 69, que es el que nombran
// music.def y envsound.def.
MapId mapa_de_sonido(MapId m) { return m == 2005 ? 69 : m; }

u64 al_azar(u64 intervalo_ms) { return intervalo_ms / 2 + static_cast<u64>(std::rand()) % (intervalo_ms + 1); }

}  // namespace

void cargar_sonidos(App& a) {
    a.rec.texto("music.def", a.texto_music_def);
    a.rec.texto("envsound.def", a.texto_envsound_def);
    a.sonidos.cargar({reinterpret_cast<const char*>(a.texto_music_def.data()), a.texto_music_def.size()},
                     {reinterpret_cast<const char*>(a.texto_envsound_def.data()), a.texto_envsound_def.size()});
    if (a.sonidos.musica.empty()) LIN_LOG("Aviso: no se encontró music.def (música por zona)");
    if (!a.audio.abrir(a.cfg.carpeta_juego, a.cfg.carpetas_extra)) LIN_LOG("Aviso: sin audio");
    aplicar_volumenes(a);
}

void aplicar_volumenes(App& a) {
    if (a.mudo) a.audio.volumenes(0, 0);
    else a.audio.volumenes(a.cfg.volumen_musica, a.cfg.volumen_efectos);
}

namespace {
constexpr u64 MS_COMBATE = 15000;  // 0x3a98: the combat music lasts this long after the last fight event

bool combate_activado(const App& a) { return !(a.cfg.opciones_orig & (1 << OPC_COMBATMUSIC)); }  // bit set = off (0x7030f0)
bool suena_pista_combate(const App& a) {
    const i32 n = a.audio.musica_actual();
    return n >= 0 && (n == a.sonidos.musica_caza || n == a.sonidos.musica_pk);
}
}  // namespace

// 0x4c53a0: every fight event (the player attacks or casts at `objetivo`, or something attacks the player)
// refreshes the combat clock. Against a character the PK track plays ("PlayrtKilling"), otherwise the
// hunting one; if either already plays nothing restarts.
void combate_musica(App& a, Slot objetivo, u64 ahora) {
    a.t_combate = ahora;
    if (!combate_activado(a) || a.pantalla != Pantalla::JUEGO || suena_pista_combate(a)) return;
    const bool pj = objetivo != SIN_SLOT && tipo_objeto(a, objetivo) == TIPO_PERSONAJE;
    const i32 pista = pj ? a.sonidos.musica_pk : a.sonidos.musica_caza;
    if (pista < 0) return;
    a.pista_combate = pista;
    a.audio.musica(pista, 0, ahora);
}

void actualizar_sonido(App& a, u64 ahora) {
    if (a.pantalla == Pantalla::LOGIN || a.pantalla == Pantalla::PERSONAJES || a.pantalla == Pantalla::CREAR) {
        a.audio.musica(a.sonidos.musica_login, 0, ahora);
        a.audio.ambiente(-1);
        a.ambiente = nullptr;
        a.ambiente_agua = false;  // si no, al volver bajo el agua no se notaría el cambio y no sonaría
    } else if (a.pantalla == Pantalla::JUEGO && ahora >= a.t_zona_sonido + MS_ZONA) {
        a.t_zona_sonido = ahora;
        const TablaObjetos& o = a.mundo.objetos;
        Slot yo = o.buscar(a.mundo.jugador.id);
        if (yo != SIN_SLOT) {
            const MapId mapa = mapa_de_sonido(a.mundo.mapa);
            const i32 x = o.col<O_X>()[yo], y = o.col<O_Y>()[yo];
            if (o.col<O_ESTADO>()[yo] == ESTADO_MUERTO && a.sonidos.musica_muerte >= 0) {
                a.audio.musica(a.sonidos.musica_muerte, 0, ahora);
            } else if (a.t_combate && ahora < a.t_combate + MS_COMBATE && combate_activado(a) && suena_pista_combate(a)) {
                // Fighting: the combat track stays until 15 s after the last event (or the option is cleared).
            } else if (const ZonaMusica* z = a.sonidos.musica_en(mapa, x, y)) {
                // Back to the zone music: a combat track fades out first (0x479ed0).
                if (suena_pista_combate(a)) a.audio.musica_fundido(z->musica, z->intervalo_s, ahora);
                else a.audio.musica(z->musica, z->intervalo_s, ahora);
            } else if (suena_pista_combate(a)) {
                a.audio.musica_fundido(-1, 0, ahora);
            } else {
                a.audio.musica(-1, 0, ahora);
            }
            // Bajo el agua suena el agua en bucle (sonido 330, de sub_4891e0) en lugar del ambiente de la zona.
            const u32 hora = static_cast<u32>(hora_de_juego(a)) % 86400 / 3600;
            const bool agua = a.mundo.submarino;
            const AmbienteZona* amb = agua ? nullptr : a.sonidos.ambiente_en(mapa, x, y, hora < 6 || hora >= 18);
            if (amb != a.ambiente || agua != a.ambiente_agua) {
                a.ambiente = amb;
                a.ambiente_agua = agua;
                a.audio.ambiente(agua ? SONIDO_AGUA : amb ? amb->principal : -1);
                for (u32 i = 0; amb && i < amb->n_grupos && i < 8; ++i) {
                    const GrupoSonido& g = a.sonidos.grupos[a.sonidos.grupos_ambiente[amb->primer_grupo + i]];
                    a.proximo_ambiente[i] = ahora + al_azar(g.intervalo_s * 1000ull);
                }
            }
        }
    }
    // Sonidos sueltos del ambiente.
    if (a.pantalla == Pantalla::JUEGO && a.ambiente) {
        for (u32 i = 0; i < a.ambiente->n_grupos && i < 8; ++i) {
            if (ahora < a.proximo_ambiente[i]) continue;
            const GrupoSonido& g = a.sonidos.grupos[a.sonidos.grupos_ambiente[a.ambiente->primer_grupo + i]];
            if (g.cantidad) a.audio.efecto(a.sonidos.ids_grupos[g.primero + static_cast<u32>(std::rand()) % g.cantidad], 0.6f, ahora);
            a.proximo_ambiente[i] = ahora + al_azar(std::max<u64>(g.intervalo_s, 5) * 1000ull);
        }
    }
    a.audio.actualizar(ahora);
}

void sonido_de_frame(App& a, u32& ultimo, const FrameAccion& f, double x, double y, u64 ahora, bool sonar_primero, u8 dir) {
    const u32 indice = static_cast<u32>(&f - &a.sprites.frame(0)) + 1;
    if (indice == ultimo) return;
    const bool primero = ultimo == 0 && !sonar_primero;
    ultimo = indice;
    if (primero) return;
    // Sufijo "]N" (el topo levanta tierra al avanzar): el gráfico N nace en la celda del objeto. Los
    // efectos mismos (sonar_primero) no encadenan otros.
    if (f.efecto && !sonar_primero) {
        EfectoVisual ef;
        ef.gfx = f.efecto;
        ef.dir = dir;
        ef.x0 = ef.x1 = x;
        ef.y0 = ef.y1 = y;
        ef.inicio = ahora;
        agregar_efecto(a.mundo, ef);
    }
    if (f.n_sonidos == 0) return;
    // Up to 3 ids per frame: one is picked at random each time the frame shows.
    const u32 k = f.n_sonidos > 1 ? static_cast<u32>(std::rand()) % f.n_sonidos : 0;
    const i32 id = k == 0 ? f.sonido : f.sonido_mas[k - 1];
    if (id < 0) return;
    const u8 modo = f.modo_sonido[k];
    double d = 0;
    const TablaObjetos& o = a.mundo.objetos;
    if (Slot yo = o.buscar(a.mundo.jugador.id); yo != SIN_SLOT)
        d = std::max(std::abs(x - o.col<O_X>()[yo]), std::abs(y - o.col<O_Y>()[yo]));
    const float ganancia = static_cast<float>(1.0 - d / CELDAS_SILENCIO);
    a.audio.efecto(static_cast<u32>(id), ganancia, ahora, modo == SONIDO_SOLAPA, modo == SONIDO_UNICO);
}

}  // namespace lin
