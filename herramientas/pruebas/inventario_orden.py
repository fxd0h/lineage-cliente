#!/usr/bin/env python3
"""Prueba en vivo: ordenar el inventario y seleccionar varios con Shift+clic.

    herramientas/pruebas/inventario_orden.py <settings.ini de prueba> [carpeta de capturas]

Charla abre el inventario, aprieta "Sort" (equipado primero, después tipo de uso y nombre), marca los
dos últimos ítems con Shift+clic y los arrastra a la primera celda. El orden es solo del cliente.
"""
from comun import Prueba, campos, comprobar, correr, ventana


def items(c):
    return [campos(l) for l in c("inventario")]


def celda(v, i):
    x, y = v[0], v[1]
    return x + 14 + (i % 6) * 36 + 17, y + 30 + (i // 6) * 36 + 17


def prueba():
    with Prueba() as p:
        c = p.entrar("Charla", volver=False)
        c("abrir inventario")
        c("esperar ventana=inventario")
        v = ventana(c, "inventario")
        antes = items(c)
        comprobar(len(antes) >= 4, "Charla tiene menos de 4 ítems")

        c("clic %d %d" % (v[0] + 184, v[1] + 293))  # Sort
        c("esperar frames=3")
        orden = items(c)
        comprobar(sorted(i["id"] for i in orden) == sorted(i["id"] for i in antes), "Sort perdió o agregó ítems")
        clave = [("equipado" not in i, int(i["uso"]), i["nombre"]) for i in orden]
        comprobar(clave == sorted(clave), "Sort no dejó equipado, uso y nombre en orden: %s" % clave)
        p.captura(c, "inventario_orden")

        # Shift+clic en los dos últimos de la primera página y arrastrar uno de ellos a la celda 0.
        n = min(len(orden), 42)
        a, b = n - 2, n - 1
        c("shift on")
        c("clic %d %d" % celda(v, a))
        c("esperar frames=2")
        c("clic %d %d" % celda(v, b))
        c("esperar frames=2")
        c("shift off")
        p.captura(c, "inventario_seleccion")
        c("arrastrar %d %d %d %d" % (celda(v, b) + celda(v, 0)))
        c("esperar frames=30")  # arrastrar son 22 pasos de mouse, uno por frame
        despues = [i["id"] for i in items(c)]
        esperado = [orden[a]["id"], orden[b]["id"]] + [i["id"] for k, i in enumerate(orden) if k not in (a, b)]
        comprobar(despues == esperado, "la selección no quedó al principio:\n%s\n%s" % (despues, esperado))


correr(prueba)
