#!/usr/bin/env python3
"""Genera src/recursos/fuente_html_datos.inc: bitmaps de glifos de 1 bit con hinting de Liberation Sans Regular (SIL OFL 1.1,
licencias/LiberationSans-OFL.txt) a 11 ppem. Reemplazan a la letra GDI "Arial" con la que el cliente 2009 dibuja
las páginas HTML de los NPC (ver recursos/fuente_ttf.cpp): lfHeight 13 equivale a 11 ppem (ascenso 10 + descenso 3 = 13, que es
exactamente lo que FreeType informa para esta fuente), calidad 0, y Windows XP dibuja Arial a ese tamaño con hinting y sin
suavizado. El hinting monocromo de FreeType (a través de Pillow) reproduce ese aspecto.

Necesita Pillow (solo para generar).  Uso: python3 tools/generar_fuente_html.py
"""
import os

from PIL import Image, ImageDraw, ImageFont

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# (fuente de origen, salida, prefijo de identificadores, etiqueta). La negrita es la letra de <b> (peso 700 del LOGFONT, 0x4359f0).
ESTILOS = [
    ("LiberationSans-Regular.ttf", "fuente_html_datos.inc", "FUENTE_HTML_", "Regular"),
    ("LiberationSans-Bold.ttf", "fuente_html_negrita_datos.inc", "FUENTE_HTML_B_", "Bold"),
]
RANGOS = [(0x20, 0x7E), (0xA0, 0xFF), (0x2013, 0x2014), (0x2018, 0x201E), (0x2026, 0x2026), (0x20AC, 0x20AC)]
PPEM, ASCENSO, ALTO = 11, 10, 13
PLUMA = 2  # columna de la pluma en las filas de 16 bits: deja que los glifos sobresalgan hasta 2 px a la izquierda del origen


def generar(origen_ttf, salida, pre, etiqueta):
    ORIGEN = os.path.join(RAIZ, "datos", "fuentes", origen_ttf)
    SALIDA = os.path.join(RAIZ, "src", "recursos", salida)
    fuente = ImageFont.truetype(ORIGEN, PPEM, layout_engine=ImageFont.Layout.BASIC)
    assert fuente.getmetrics() == (ASCENSO, ALTO - ASCENSO), fuente.getmetrics()
    cps, anchos, filas = [], [], []
    for lo, hi in RANGOS:
        for cp in range(lo, hi + 1):
            if not fuente.getmask(chr(cp), mode="1").getbbox() and cp not in (0x20, 0xA0):
                continue  # no está en la fuente
            im = Image.new("L", (16, ALTO), 0)
            d = ImageDraw.Draw(im)
            d.fontmode = "1"
            d.text((PLUMA, ASCENSO), chr(cp), font=fuente, fill=255, anchor="ls")
            rows = []
            for y in range(ALTO):
                v = 0
                for x in range(16):
                    if im.getpixel((x, y)):
                        v |= 0x8000 >> x
                rows.append(v)
            cps.append(cp)
            anchos.append(int(fuente.getlength(chr(cp), mode="1")))
            filas.append(rows)
    out = [
        f"// Generado por tools/generar_fuente_html.py desde datos/fuentes/{origen_ttf} (SIL OFL 1.1).",
        "// Glifos de 1 bit con hinting a 11 ppem. No editar a mano.",
        f"constexpr int {pre}PLUMA = {PLUMA};  // columna de la pluma dentro de las filas de 16 bits (bit 15 = columna 0)",
        f"constexpr int {pre}ASCENSO = {ASCENSO};",
        f"constexpr int {pre}N = {len(cps)};",
        f"constexpr unsigned short {pre}CP[{pre}N] = {{" + ", ".join(f"0x{c:x}" for c in cps) + "};",
        f"constexpr unsigned char {pre}ANCHO[{pre}N] = {{" + ", ".join(str(a) for a in anchos) + "};",
        f"constexpr unsigned short {pre}FILAS[{pre}N][{ALTO}] = {{",
    ]
    for r in filas:
        out.append("    {" + ", ".join(f"0x{v:04x}" for v in r) + "},")
    out.append("};")
    with open(SALIDA, "w", encoding="utf-8") as f:
        f.write("\n".join(out) + "\n")
    print(f"{SALIDA}: {len(cps)} glyphs")


def main():
    for estilo in ESTILOS:
        generar(*estilo)


if __name__ == "__main__":
    main()
