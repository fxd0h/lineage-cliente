// Throwaway helper: dumps decrypted Text.pak files of a game folder to /tmp/t380/ (mkdir it first).
// g++ -std=c++23 -O1 -I../src dump_text.cpp ../src/recursos/{pak,des}.cpp ../src/plataforma/archivo_mapeado.cpp -o /tmp/dump_text
// /tmp/dump_text "<game folder>/" desc-c.tbl
#include "recursos/pak.h"
#include <cstdio>
using namespace lin;
int main(int c, char** v) {
    Recursos r; r.abrir(v[1]);
    for (int i = 2; i < c; ++i) { std::vector<u8> d; if (!r.texto(v[i], d)) { std::fprintf(stderr, "no %s\n", v[i]); continue; }
        std::string o = std::string("/tmp/t380/") + v[i]; FILE* f = std::fopen(o.c_str(), "wb"); std::fwrite(d.data(), 1, d.size(), f); std::fclose(f); }
}
