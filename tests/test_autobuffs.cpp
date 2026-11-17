// Auto-buffs: elegir qué buff relanzar (mundo/autobuffs.h).
#include <doctest/doctest.h>
#include <vector>
#include "mundo/autobuffs.h"
#include "proto/decodificar.h"

using namespace lin;

namespace {
CasilleroAuto casillero(u32 hechizo, i32 mp = 10, u64 duracion_ms = 60000) {
    CasilleroAuto c;
    c.hechizo = hechizo;
    c.mp = mp;
    c.duracion_ms = duracion_ms;
    return c;
}

EstadoJugador jugador(i32 mp = 100) {
    EstadoJugador j;
    j.mp = mp;
    return j;
}

void poner(EstadoJugador& j, u8 clase, u8 tipo, u64 fin) { j.efectos[j.n_efectos++] = {clase, tipo, fin}; }
}  // namespace

TEST_CASE("elegir_autobuff: el primero sin efecto, de izquierda a derecha") {
    EstadoJugador j = jugador();
    CasilleroAuto c[CASILLEROS_AUTO] = {casillero(43), casillero(42)};  // Haste, Enchant Mighty
    CHECK(elegir_autobuff(j, c, CASILLEROS_AUTO, 100000) == 0);
    poner(j, EA_HASTE, 1, 400000);  // Haste activo
    CHECK(elegir_autobuff(j, c, CASILLEROS_AUTO, 100000) == 1);
    poner(j, EA_FUERZA, 5, 400000);  // Enchant Mighty activo
    CHECK(elegir_autobuff(j, c, CASILLEROS_AUTO, 100000) == -1);
}

TEST_CASE("elegir_autobuff: se renueva con menos de MARGEN_AUTO_MS") {
    EstadoJugador j = jugador();
    CasilleroAuto c[1] = {casillero(43)};
    poner(j, EA_HASTE, 1, 100000 + MARGEN_AUTO_MS);
    CHECK(elegir_autobuff(j, c, 1, 100000) == -1);
    CHECK(elegir_autobuff(j, c, 1, 100001) == 0);
}

TEST_CASE("elegir_autobuff: efecto que no se ve, no antes de su duración") {
    EstadoJugador j = jugador();
    CasilleroAuto c[1] = {casillero(14, 10, 60000)};
    c[0].lanzado_ms = 100000;
    CHECK(elegir_autobuff(j, c, 1, 100000 + 59999) == -1);
    CHECK(elegir_autobuff(j, c, 1, 100000 + 60000) == 0);
}

TEST_CASE("elegir_autobuff: un efecto que se vio y terminó antes de tiempo se relanza") {
    EstadoJugador j = jugador();
    CasilleroAuto c[1] = {casillero(43, 10, 300000)};  // Haste, 300 s
    c[0].lanzado_ms = 100000;
    c[0].efecto_visto = true;  // tuvo ícono; ya no está (murió, se lo quitaron)
    CHECK(elegir_autobuff(j, c, 1, 160000) == 0);
}

TEST_CASE("elegir_autobuff: espera la respuesta del lanzado anterior aunque el efecto esté por vencer") {
    EstadoJugador j = jugador();
    CasilleroAuto c[1] = {casillero(43)};
    poner(j, EA_HASTE, 1, 101000);  // le queda 1 s
    c[0].lanzado_ms = 99000;
    CHECK(elegir_autobuff(j, c, 1, 100000) == -1);
    c[0].lanzado_ms = 100000 - ESPERA_RESPUESTA_MS;
    CHECK(elegir_autobuff(j, c, 1, 100000) == 0);
}

TEST_CASE("elegir_autobuff: MP, fallo, muerto e inmóvil") {
    EstadoJugador j = jugador(5);
    CasilleroAuto c[2] = {casillero(43, 10), casillero(42, 5)};
    CHECK(elegir_autobuff(j, c, 2, 100000) == 1);  // el primero no alcanza el MP: el siguiente
    c[1].reintentar_ms = 100000 + ESPERA_FALLO_MS;
    CHECK(elegir_autobuff(j, c, 2, 100000) == -1);
    CHECK(elegir_autobuff(j, c, 2, 100000 + ESPERA_FALLO_MS) == 1);
    j.inmovil = 1;
    CHECK(elegir_autobuff(j, c, 2, 200000) == -1);
    j.inmovil = 0;
    j.muerto = true;
    CHECK(elegir_autobuff(j, c, 2, 200000) == -1);
}

TEST_CASE("acepta_autobuff: solo buffs propios") {
    Hechizo h;
    h.nombre = "Haste";
    h.duracion_s = 300;
    CHECK(acepta_autobuff(h));
    h.de_ataque = true;
    CHECK_FALSE(acepta_autobuff(h));
    h.de_ataque = false;
    h.duracion_s = 0;  // una curación
    CHECK_FALSE(acepta_autobuff(h));
    h.duracion_s = 300;
    h.sobre_item = Hechizo::SOBRE_ARMA;  // Enchant Weapon: pide elegir el arma
    CHECK_FALSE(acepta_autobuff(h));
}

TEST_CASE("pociones en un casillero: se reconocen por $N o ícono y su efecto por el ícono del servidor") {
    Inventario inv;
    auto agregar = [&](ObjId id, std::string_view nombre, u32 cantidad, u16 gfx) {
        const Slot s = inv.items.insertar(id);
        inv.items.col<I_CANTIDAD>()[s] = cantidad;
        inv.items.col<I_GFX>()[s] = gfx;
        inv.items.col<I_NOMBRE>()[s] = {static_cast<u32>(inv.textos.size()), static_cast<u16>(nombre.size())};
        inv.textos.insert(inv.textos.end(), nombre.begin(), nombre.end());
        return s;
    };
    const Slot roja = agregar(1, "$26 (10)", 10, 80), azul = agregar(2, "$232 (5)", 5, 75);
    const Slot sab = agregar(3, "Wisdom", 1, 1529);  // sin $N (3.80C): por el ícono
    CHECK(pocion_auto_de_item(inv, roja) == 0);
    CHECK(pocion_auto_de_item(inv, azul) == POCION_AUTO);
    CHECK(pocion_auto_de_item(inv, sab) == POCION_AUTO + 1);
    CHECK(buscar_pocion_auto(inv, POCION_AUTO) == 2);
    inv.items.col<I_CANTIDAD>()[azul] = 0;
    CHECK(buscar_pocion_auto(inv, POCION_AUTO) == 0);
    CHECK(pocion_auto(43) == nullptr);
    CHECK(pocion_auto(POCION_AUTO + 1)->tipo == ICONO_SABIDURIA);
    // La mayor de haste es "$1652 $234": no se confunde con la común.
    CHECK(pocion_auto_de_item(inv, agregar(4, "$234 (3)", 3, 77)) == POCION_AUTO + 2);
    CHECK(pocion_auto_de_item(inv, agregar(5, "$1652 $234 (3)", 3, 621)) == POCION_AUTO + 3);

    // Sin MP y sin efecto se toma; con el ícono activo, no.
    EstadoJugador j = jugador(0);
    CasilleroAuto c[1] = {casillero(POCION_AUTO, 0, 600000)};
    CHECK(elegir_autobuff(j, c, 1, 100000) == 0);
    poner(j, EA_ICONO, ICONO_POCION_AZUL, 400000);
    CHECK(elegir_autobuff(j, c, 1, 100000) == -1);
    CHECK(resta_efecto_de_hechizo(j, POCION_AUTO, 100000) == 300000);
    CHECK(resta_efecto_de_hechizo(j, POCION_AUTO + 1, 100000) == 0);
    // Haste: el del hechizo o la poción vale para las dos pociones; la waffle no es la de brave.
    poner(j, EA_HASTE, 1, 200000);
    CHECK(resta_efecto_de_hechizo(j, POCION_AUTO + 2, 100000) == 100000);
    CHECK(resta_efecto_de_hechizo(j, POCION_AUTO + 3, 100000) == 100000);
    poner(j, EA_BRAVE, 3, 200000);
    CHECK(resta_efecto_de_hechizo(j, POCION_AUTO + 4, 100000) == 0);
    CHECK(resta_efecto_de_hechizo(j, POCION_AUTO + 5, 100000) == 100000);
}

TEST_CASE("S_SkillIconWisdomPotion: el tiempo viene dividido por 4") {
    TablaOpcodes t;
    tabla_opcodes("jp363", t);
    Arena arena(1024);
    std::vector<Evento> ev;
    std::vector<u8> p = {40, 0x39, 0x2c, 75, 0};  // 300 s
    REQUIRE(decodificar(p, t, arena, ev));
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].tipo == Ev::ICONO_EFECTO);
    CHECK(ev[0].v[IE_CLASE] == IE_ICONO);
    CHECK(ev[0].v[IE_TIPO] == ICONO_SABIDURIA);
    CHECK(ev[0].v[IE_SEG] == 300);
}

TEST_CASE("TurnoAuto: varios vencidos a la vez salen de a uno, en orden y espaciados") {
    EstadoJugador j = jugador();
    CasilleroAuto c[CASILLEROS_AUTO] = {casillero(43), casillero(42), casillero(26)};
    TurnoAuto turno;
    std::vector<std::pair<int, u64>> salidos;
    for (u64 t = 100000; t < 104000; t += 16) {  // frames de 16 ms
        if (!turno.libre(t)) continue;
        const int i = elegir_autobuff(j, c, CASILLEROS_AUTO, t);
        if (i < 0) continue;
        c[i].lanzado_ms = t;
        c[i].efecto_visto = true;
        poner(j, EA_HECHIZO, static_cast<u8>(c[i].hechizo), t + 60000);  // el efecto llega
        turno.usado(t);
        salidos.push_back({i, t});
    }
    REQUIRE(salidos.size() == 3);
    for (size_t k = 0; k < salidos.size(); ++k) CHECK(salidos[k].first == static_cast<int>(k));
    for (size_t k = 1; k < salidos.size(); ++k) CHECK(salidos[k].second - salidos[k - 1].second >= ESPACIO_AUTO_MS);
    turno.usado(200000);  // un hechizo que salió tarde corre el turno; uno anterior no lo adelanta
    turno.usado(150000);
    CHECK_FALSE(turno.libre(200999));
    CHECK(turno.libre(201000));
}
