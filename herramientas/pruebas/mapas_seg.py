#!/usr/bin/env python3
"""Prueba en vivo: segmentos de mapa que solo están en el formato viejo (.seg).

    herramientas/pruebas/mapas_seg.py <settings.ini de prueba> [carpeta de capturas]

Charla (GM) va con .move al Chaotic Temple (mapa 4, 32885 32652; solo hay 80017ffe.seg) y al norte de
las cuevas de Gludio, saca una captura de cada lugar y vuelve a donde estaba. Antes se veían negros.
"""
from comun import Prueba, comprobar, correr, estado, mover

LUGARES = [("chaotic_temple", 4, 32885, 32652), ("gludio_cueva", 4, 32728, 32925), ("gludio_cueva_norte", 4, 32740, 32880)]


def prueba():
    with Prueba() as p:
        c = p.entrar("Charla")
        for nombre, mapa, x, y in LUGARES:
            mover(c, mapa, x, y)
            c("esperar frames=60")  # que el hilo cargue los segmentos de alrededor
            comprobar(estado(c).get("mapa") == str(mapa), "no llegó al mapa %d" % mapa)
            p.captura(c, "mapa_" + nombre)


correr(prueba)
