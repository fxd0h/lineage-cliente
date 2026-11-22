#!/usr/bin/env python3
"""Genera src/recursos/fuente_html_hd_datos.inc: Liberation Sans Regular (SIL OFL 1.1, licencias/LiberationSans-OFL.txt)
recortada a los mismos puntos de código que la letra de píxeles (tools/generar_fuente_html.py), sin hinting. La pasada de texto HD
(texto_hd=1) rasteriza sus contornos reales a la escala de pantalla; la diagramación sigue usando los avances enteros con hinting de la
letra de píxeles, así las líneas se cortan exactamente igual que antes.

Necesita fonttools (solo para generar).  Uso: python3 tools/generar_fuente_html_hd.py
"""
import os
import subprocess
import sys
import tempfile

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# (fuente de origen, salida, prefijo de identificadores). La negrita es la letra de <b>.
ESTILOS = [
    ("LiberationSans-Regular.ttf", "fuente_html_hd_datos.inc", "FUENTE_HTML_HD"),
    ("LiberationSans-Bold.ttf", "fuente_html_negrita_hd_datos.inc", "FUENTE_HTML_B_HD"),
]
RANGOS = "U+0020-007E,U+00A0-00FF,U+2013-2014,U+2018-201E,U+2026,U+20AC"


def generar(origen_ttf, salida, pre):
    ORIGEN = os.path.join(RAIZ, "datos", "fuentes", origen_ttf)
    SALIDA = os.path.join(RAIZ, "src", "recursos", salida)
    with tempfile.TemporaryDirectory() as tmp:
        recortada = os.path.join(tmp, "recortada.ttf")
        subprocess.run([sys.executable, "-m", "fontTools.subset", ORIGEN, "--unicodes=" + RANGOS,
                        "--layout-features=", "--no-hinting", "--desubroutinize", "--output-file=" + recortada],
                       check=True)
        with open(recortada, "rb") as f:
            datos = f.read()
    lineas = [
        f"// Generado por tools/generar_fuente_html_hd.py desde datos/fuentes/{origen_ttf} (SIL OFL 1.1).",
        "// No editar a mano.",
        f"constexpr unsigned int {pre}_TAM = {len(datos)};",
        f"constexpr unsigned char {pre}_DATOS[] = {{",
    ]
    for i in range(0, len(datos), 16):
        lineas.append("    " + ", ".join(f"0x{b:02x}" for b in datos[i:i + 16]) + ",")
    lineas.append("};")
    with open(SALIDA, "w", encoding="utf-8") as f:
        f.write("\n".join(lineas) + "\n")
    print(f"{SALIDA}: {len(datos)} bytes")


def main():
    for estilo in ESTILOS:
        generar(*estilo)


if __name__ == "__main__":
    main()
