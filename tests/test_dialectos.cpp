// Protocol dialects (opcodes.cpp DIALECTOS): tw380 "original" vs "login38", plus the tw380 profile fixes.
#include <doctest/doctest.h>
#include "comun.h"
#include "plataforma/rutas.h"
#include "proto/comandos.h"
#include "proto/decodificar.h"

using namespace lin;

namespace {

struct Pkt {
    std::vector<u8> b;
    explicit Pkt(u8 op) { b.push_back(op); }
    Pkt& c(u32 v) { b.push_back(static_cast<u8>(v)); return *this; }
    Pkt& h(u32 v) { return c(v).c(v >> 8); }
    Pkt& d(u32 v) { return h(v).h(v >> 16); }
    Pkt& s(const char* t) { while (*t) c(static_cast<u8>(*t++)); return c(0); }
    Pkt& pad() { while (b.size() % 8) b.push_back(0); return *this; }  // Whale pads every body to 8
};

std::vector<Evento> dec(const TablaOpcodes& t, const Pkt& p) {
    Arena arena(4096);
    std::vector<Evento> ev;
    decodificar({p.b.data(), p.b.size()}, t, arena, ev);
    return ev;
}

TablaOpcodes tabla(const char* dialecto) {
    TablaOpcodes t;
    REQUIRE(tabla_opcodes("tw380", t, dialecto));
    return t;
}

}  // namespace

TEST_CASE("Dialectos: nombres validos y rechazo de desconocidos") {
    TablaOpcodes t;
    CHECK(tabla_opcodes("tw380", t));
    CHECK(tabla_opcodes("tw380", t, "original"));
    CHECK(tabla_opcodes("tw380", t, "login38"));
    CHECK(t.perfil.bytes_mr == 4);
    CHECK_FALSE(tabla_opcodes("tw380", t, "nope"));
    CHECK_FALSE(tabla_opcodes("jp363", t, "login38"));  // a dialect belongs to one profile
    CHECK(tabla_opcodes("jp363", t, "original"));
    for (std::string_view d : DIALECTOS_CONOCIDOS) CHECK(tabla_opcodes("tw380", t, d));
}

TEST_CASE("Dialectos: servidor= con y sin dialecto, el lanzador sigue valiendo") {
    Config c = leer_config("servidor=W|127.0.0.1|7001|tw380|fx|login38\nservidor=V|h|7001|tw380|fx\n"
                           "servidor=L|h|7001|tw380|fx|./lanzar.sh a|b\nservidor=M|h|7001|tw380|fx|login38|./lanzar.sh\n");
    REQUIRE(c.servidores.size() == 4);
    CHECK(c.servidores[0].dialecto == "login38");
    CHECK(c.servidores[0].lanzador.empty());
    CHECK(c.servidores[1].dialecto == "original");
    CHECK(c.servidores[2].dialecto == "original");
    CHECK(c.servidores[2].lanzador == "./lanzar.sh a|b");
    CHECK(c.servidores[3].dialecto == "login38");
    CHECK(c.servidores[3].lanzador == "./lanzar.sh");
    const std::string texto = escribir_config(c);
    CHECK(texto.find("servidor=W|127.0.0.1|7001|tw380|fx|login38\n") != std::string::npos);
    CHECK(texto.find("servidor=V|h|7001|tw380|fx\n") != std::string::npos);
    Config c2 = leer_config(texto);
    CHECK(c2.servidores[3].dialecto == "login38");
    CHECK(c2.servidores[3].lanzador == "./lanzar.sh");
    CHECK(c2.servidores[2].lanzador == "./lanzar.sh a|b");
}

TEST_CASE("tw380: opcodes corregidos (SELECTTARGET 54, BLESSOFEVA 126, DELSKILL 160, FARATTACK 123)") {
    for (const char* d : {"original", "login38"}) {
        TablaOpcodes t = tabla(d);
        CHECK(t.srv(54) == Srv::SELECTTARGET);
        CHECK(t.srv(236) == Srv::DESCONOCIDO);
        CHECK(t.srv(126) == Srv::BLESSOFEVA);
        CHECK(t.srv(160) == Srv::DELSKILL);
        CHECK(t.cli(Cli::FARATTACK) == 123);
        // bow: C_FarAttack with the same body; melee stays 229
        Escritor el = cmd::atacar(t, 0x11223344, 5, 6, true), ec = cmd::atacar(t, 0x11223344, 5, 6, false);
        Bytes lejos = el.cuerpo(), cerca = ec.cuerpo();
        CHECK(lejos[0] == 123);
        CHECK(cerca[0] == 229);
        CHECK(lejos.size() == cerca.size());
        CHECK(std::memcmp(lejos.data() + 1, cerca.data() + 1, 8) == 0);
    }
    TablaOpcodes jp;
    REQUIRE(tabla_opcodes("jp363", jp));
    CHECK_FALSE(jp.tiene(Cli::FARATTACK));
    Escritor ej = cmd::atacar(jp, 1, 2, 3, true);
    CHECK(ej.cuerpo()[0] == jp.cli(Cli::ATTACK));  // other profiles keep the melee packet
}

TEST_CASE("tw380: BLESSOFEVA y DELSKILL decodifican") {
    TablaOpcodes t = tabla("original");
    auto ev = dec(t, Pkt(126).d(77).h(120).pad());
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].tipo == Ev::ICONO_EFECTO);
    CHECK(ev[0].id == 77);
    CHECK(ev[0].v[IE_SEG] == 120);
    Pkt del(160);
    del.c(32);
    for (int i = 0; i < 28; ++i) del.c(i == 18 ? 248 : 0);
    del.d(0).d(0).pad();
    ev = dec(t, del);
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].tipo == Ev::HECHIZOS);
    CHECK(ev[0].v[HZ_AGREGAR] == 0);
}

TEST_CASE("Dialectos: lista de personajes (93) y personaje nuevo (127), HP/MP h o d") {
    for (u8 op : {93, 127}) {
        // original: h hp, h mp (stock 3.80C)
        Pkt o(op);
        o.s("Ana").s("").c(1).c(0).h(500).h(300).h(200).c(10).c(5).c(12).c(11).c(10).c(9).c(8).c(7).c(0).d(0).c(0).pad();
        auto ev = dec(tabla("original"), o);
        REQUIRE(ev.size() == 1);
        CHECK(ev[0].v[PJ_HP] == 300);
        CHECK(ev[0].v[PJ_MP] == 200);
        CHECK(ev[0].v[PJ_NIVEL] == 5);
        CHECK(ev[0].v[PJ_FUE] == 12);
        // login38: d hp, d mp (handler/charlist.go:75)
        Pkt l(op);
        l.s("Ana").s("").c(1).c(0).h(500).d(70000).d(80000).c(10).c(5).c(12).c(11).c(10).c(9).c(8).c(7).c(0).d(0).c(0).pad();
        ev = dec(tabla("login38"), l);
        REQUIRE(ev.size() == 1);
        CHECK(ev[0].v[PJ_HP] == 70000);
        CHECK(ev[0].v[PJ_MP] == 80000);
        CHECK(ev[0].v[PJ_CA] == 10);
        CHECK(ev[0].v[PJ_NIVEL] == 5);
        CHECK(ev[0].v[PJ_FUE] == 12);
        CHECK(ev[0].v[PJ_INT] == 7);
    }
}

TEST_CASE("Dialectos: STATUS (8), AC c o d") {
    auto armar = [](bool ancho) {
        Pkt p(8);
        p.d(99).c(50).d(1234).c(10).c(11).c(12).c(13).c(14).c(15).d(100).d(200).d(300).d(400);
        if (ancho) p.d(static_cast<u32>(-5)); else p.c(0x30);
        p.d(0).c(77).c(88).h(32768 + 100).h(1).h(2).h(3).h(4).d(0).pad();
        return p;
    };
    auto ev = dec(tabla("login38"), armar(true));
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].tipo == Ev::ESTADO_PROPIO);
    CHECK(ev[0].v[EP_HPMAX] == 200);
    CHECK(ev[0].v[EP_MPMAX] == 400);
    CHECK(ev[0].v[EP_CA] == -5);
    CHECK(ev[0].v[EP_COMIDA] == 77);
    CHECK(ev[0].v[EP_PESO] == 88);
    CHECK(ev[0].v[EP_LEGAL] == static_cast<i16>(32768 + 100));
    ev = dec(tabla("original"), armar(false));
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].v[EP_CA] == (0x30 - 256));
    CHECK(ev[0].v[EP_COMIDA] == 77);
    CHECK(ev[0].v[EP_PESO] == 88);
}

TEST_CASE("Dialectos: ABILITY_SCORES (174), resistencias c o h") {
    auto ev = dec(tabla("original"), Pkt(174).c(8).c(10).c(20).c(30).c(40).pad());
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].tipo == Ev::DEFENSA);
    CHECK(ev[0].v[DF_FUEGO] == 10);
    CHECK(ev[0].v[DF_TIERRA] == 40);
    ev = dec(tabla("login38"), Pkt(174).c(8).h(10).h(20).h(300).h(static_cast<u32>(-4)).pad());
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].v[DF_FUEGO] == 10);
    CHECK(ev[0].v[DF_AGUA] == 20);
    CHECK(ev[0].v[DF_VIENTO] == 300);
    CHECK(ev[0].v[DF_TIERRA] == -4);
}

TEST_CASE("Dialectos: MAGIC_STATUS (37), MR c o d") {
    auto ev = dec(tabla("original"), Pkt(37).c(3).c(15).pad());
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].tipo == Ev::SP_MR);
    CHECK(ev[0].v[0] == 3);
    CHECK(ev[0].v[1] == 15);
    ev = dec(tabla("login38"), Pkt(37).c(3).d(1000).pad());
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].v[0] == 3);
    CHECK(ev[0].v[1] == 1000);
    ev = dec(tabla("login38"), Pkt(37).c(3).d(static_cast<u32>(-20)).pad());
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].v[1] == -20);
}

TEST_CASE("Dialectos: RANGESKILLS (42), objetivos de 5 o 10 bytes") {
    auto cabecera = [](Pkt& p, int tipo, int n) {
        p.c(18).d(500).h(100).h(200).c(3).d(9).h(1810).c(tipo).h(0).h(n);
    };
    // original: d id, c mark
    Pkt o(42);
    cabecera(o, 1, 2);
    o.d(11).c(0x20).d(12).c(0).pad();
    auto ev = dec(tabla("original"), o);
    std::vector<i32> ids;
    for (const Evento& e : ev) if (e.tipo == Ev::EFECTO) ids.push_back(static_cast<i32>(e.id));
    CHECK(ids == std::vector<i32>{11, 12});
    // login38: d id, h hit, d damage
    Pkt l(42);
    cabecera(l, 1, 3);
    l.d(11).h(0x20).d(250).d(12).h(0).d(0).d(13).h(0x20).d(99999).pad();
    ev = dec(tabla("login38"), l);
    ids.clear();
    for (const Evento& e : ev) if (e.tipo == Ev::EFECTO) ids.push_back(static_cast<i32>(e.id));
    CHECK(ids == std::vector<i32>{11, 12, 13});
    // area (type 0): one effect on the caster, targets skipped by the dialect stride without error
    Pkt a(42);
    cabecera(a, 0, 2);
    a.d(11).h(0x20).d(250).d(12).h(0).d(0).pad();
    ev = dec(tabla("login38"), a);
    int efectos = 0;
    for (const Evento& e : ev) efectos += e.tipo == Ev::EFECTO;
    CHECK(efectos == 1);
}

TEST_CASE("Dialectos: S_CharReset (64) sub 1 y 2, AC c/h o d") {
    // sub 1: d hp, d mp, AC, c maxLevel
    auto ev = dec(tabla("original"), Pkt(64).c(1).d(60).d(30).c(10).c(50).pad());
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].tipo == Ev::REINICIO);
    CHECK(ev[0].v[RE_HP] == 60);
    CHECK(ev[0].v[RE_AC] == 10);
    CHECK(ev[0].v[RE_MAX] == 50);
    ev = dec(tabla("login38"), Pkt(64).c(1).d(60).d(30).d(10).c(50).pad());
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].v[RE_AC] == 10);
    CHECK(ev[0].v[RE_MAX] == 50);
    // sub 2: c level, c max, d hp, d mp, AC, 6 x c stats
    ev = dec(tabla("login38"), Pkt(64).c(2).c(9).c(52).d(700).d(300).d(static_cast<u32>(-12)).c(1).c(2).c(3).c(4).c(5).c(6).pad());
    REQUIRE(ev.size() == 1);
    CHECK(ev[0].v[RE_NIVEL] == 9);
    CHECK(ev[0].v[RE_HP] == 700);
    CHECK(ev[0].v[RE_AC] == -12);
    CHECK(ev[0].v[RE_FUE] == 1);
    CHECK(ev[0].v[RE_FUE + 5] == 6);
}
