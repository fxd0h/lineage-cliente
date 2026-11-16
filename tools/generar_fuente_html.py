#!/usr/bin/env python3
"""Genera src/recursos/fuente_html_datos.inc: Liberation Sans Regular (SIL OFL 1.1, licencias/LiberationSans-OFL.txt)
recortada a ASCII, Latin-1 y puntuación. Sustituye a Arial (métricas compatibles) en las páginas HTML de la
interfaz original: el cliente de 2009 dibuja esas páginas con la fuente GDI "Arial" (ver recursos/fuente_ttf.cpp).

Requiere fonttools (solo para generar).  Uso: python3 tools/generar_fuente_html.py
"""
import os
import subprocess
import sys
import tempfile

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ORIGEN = os.path.join(RAIZ, "datos", "fuentes", "LiberationSans-Regular.ttf")
SALIDA = os.path.join(RAIZ, "src", "recursos", "fuente_html_datos.inc")
RANGOS = "U+0020-007E,U+00A0-00FF,U+2013-2014,U+2018-201E,U+2026,U+20AC"


def main():
    with tempfile.TemporaryDirectory() as tmp:
        recortada = os.path.join(tmp, "recortada.ttf")
        subprocess.run([sys.executable, "-m", "fontTools.subset", ORIGEN, "--unicodes=" + RANGOS,
                        "--layout-features=", "--no-hinting", "--desubroutinize", "--output-file=" + recortada], check=True)
        with open(recortada, "rb") as f:
            datos = f.read()
    lineas = [
        "// Generado por tools/generar_fuente_html.py desde datos/fuentes/LiberationSans-Regular.ttf (SIL OFL 1.1).",
        "// No editar a mano.",
        f"constexpr unsigned int FUENTE_HTML_TAM = {len(datos)};",
        "constexpr unsigned char FUENTE_HTML_DATOS[] = {",
    ]
    for i in range(0, len(datos), 16):
        lineas.append("    " + ", ".join(f"0x{b:02x}" for b in datos[i:i + 16]) + ",")
    lineas.append("};")
    with open(SALIDA, "w", encoding="utf-8") as f:
        f.write("\n".join(lineas) + "\n")
    print(f"{SALIDA}: {len(datos)} bytes")


if __name__ == "__main__":
    main()
