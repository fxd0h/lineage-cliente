# Revisión de los shaders y su costo (2026-11-22)

Revisión del camino de dibujo de un cuadro (`Gpu::presentar`, `src/render/gpu.cpp`) y de cada shader de
`src/render/shaders/`, con lo que cuesta en la GPU y lo que se puede mejorar. Las cuentas son para la ventana
casi a pantalla completa de una MacBook (3024 × 1890 píxeles reales), vista amplia, escala interna 3: la grilla
lógica mide 1008 × 604 (0,61 Mpx) y la textura de destino `destino_`, 3024 × 1812 (5,48 Mpx).

## Resumen

- **Corregido:** con un filtro de escalado (xBR, FSR, CRT, Anime4K) el mundo temblaba al moverse en diagonal.
  La cámara se mueve de a fracciones de píxel lógico; el filtro lee un punto por píxel lógico, y los sprites que
  caían a mitad de bloque se le corrían de a saltos que en x y en y llegaban en cuadros distintos. Ahora, con un
  filtro activo, cada instancia se ubica en el píxel lógico entero (`Gpu::dibujar_escalado`).
- **Hecho: con un filtro activo, la escena va a la grilla lógica.** Antes se dibujaba a escala² (9 veces los
  píxeles a escala 3) y el filtro usaba uno de cada 9. Ahora las pasadas de escena, agua y brillo trabajan sobre
  0,61 Mpx en vez de 5,48 (~89 % menos a escala 3, 94 % a escala 4) y el filtro escribe la imagen final en
  `salida_` (sección "Recomendaciones", 1).
- **En una GPU de Apple (M5) nada de esto limita:** todos los modos quedan por debajo de 6 ms por cuadro, con
  la pantalla a 120 Hz. Importa en GPUs integradas de PC (Intel, AMD de gama baja) y en Linux con render por
  software (lavapipe), donde el ancho de banda es 5 a 10 veces menor.

## Las pasadas de un cuadro

| # | Pasada | Cuándo | Resolución | Lecturas por píxel | Notas |
|---|---|---|---|---|---|
| 1 | Subidas (atlas, instancias, luz) | siempre | — | — | `SDL_UploadToGPUTexture`; buffers de transferencia reciclados (`cycle`) |
| 2 | Escena: `vs_sprite` / `fs_sprite` | siempre | destino (5,48 Mpx) × sobredibujo (~2-3) | 1 | una llamada instanciada por tramo de mezcla (normal, screen, multiplicar) |
| 3 | Ola / visión: copia + `fs_mundo` | agua o visión especial | destino | 1 + copia de 22 MB | la copia entera de `destino_` a `copia_` cada cuadro |
| 4 | Brillo: `fs_brillo` | noche, luces, tinte | destino | 2 (texturas chicas) | mezcla de multiplicar: lee y escribe el destino |
| 5 | Efectos e interfaz sobre el mundo | si hubo 3 o 4 | destino × sobredibujo | 1 | mismo shader que 2 |
| 6 | Filtro de escalado | `escalado` ≠ entero | destino | xBR 11, FSR 12, CRT 2 | lee `destino_` en el centro de cada bloque de escala × escala |
| 6' | Anime4K | `escalado=anime4k` | 4 × grilla, 1 × 2 grilla, 1 × destino | 9 por convolución | intermedios RGBA16F; 3 texturas extra |
| 7 | Texto HD: capa + composición | `texto_hd=1` y hay texto | destino (vista fija: ventana) | 2 (capa) + 2 (composición) | borra dos destinos (RGBA8 + R8) y redibuja los paneles como atenuadores |
| 8 | Copia a la ventana | siempre | ventana | 1 | vista fija: lineal a la ventana (≤ 2×) |

Cada pasada a pantalla completa en RGBA8 mueve unos 44 MB (leer + escribir 5,48 Mpx). Un cuadro con agua, luz,
filtro y texto HD hace 6 a 8 de esas: 260 a 350 MB por cuadro, unos 16 a 21 GB/s a 60 fps. Una M5 tiene más de
100 GB/s; una GPU integrada de PC, 25 a 50 GB/s compartidos con la CPU.

## Mediciones

`perf on` ahora también informa el tiempo de GPU por cuadro (`gpu X ms`): mientras está activo, cada cuadro
espera a que la GPU termine su comando. MacBook con M5, ventana de 1512 × 945 puntos (3024 × 1890 píxeles),
interfaz original, parado en Talking Island; dos corridas de 5 s por caso:

| Caso | GPU (ms), corrida 1 | GPU (ms), corrida 2 |
|---|---|---|
| entero, vista amplia, texto HD | 3,41 | 5,26 |
| xBR | 3,96 | 4,48 |
| FSR | 3,95 | 4,40 |
| CRT | 3,28 | 5,26 |
| Anime4K | 5,69 | 5,67 |
| entero, sin texto HD | 4,35 | 4,38 |
| FSR, vista fija | 3,59 | 4,91 |
| entero, vista fija | 5,69 | 2,09 |

La diferencia entre corridas (hasta 3,6 ms en el mismo caso) es mayor que la diferencia entre filtros: con tan
poca carga la GPU de Apple baja su reloj y el tiempo medido refleja más la frecuencia que el trabajo. Lo único
estable es que Anime4K es el más caro (5,7 ms en las dos corridas). Para comparar filtros con precisión hace
falta una GPU que trabaje cerca de su límite (una integrada de PC) o Instruments (Metal System Trace).

### Después de la recomendación 1

Mismas condiciones, dos corridas:

| Caso | GPU (ms), corrida 1 | GPU (ms), corrida 2 |
|---|---|---|
| entero, vista amplia, texto HD | 5,40 | 5,40 |
| xBR | 5,19 | 4,99 |
| FSR | 5,21 | 5,30 |
| CRT | 3,96 | 3,43 |
| Anime4K | 5,44 | 4,86 |
| entero, sin texto HD | 4,50 | 4,46 |
| FSR, vista fija | 5,46 | 5,91 |
| entero, vista fija | 4,28 | 4,13 |

En la M5 no se ve la mejora: "entero", cuyo camino no cambió, también pasa de 3,4 a 5,4 ms entre series, y
la variación por la frecuencia de la GPU sigue tapando las diferencias. La ganancia es de cuenta (9 veces menos
fragmentos y ancho de banda en las pasadas de escena con un filtro) y se va a notar donde la GPU trabaja cerca de
su límite.

## Hallazgos por shader

- **`sprite.vert` / `sprite.frag` (escena):** bien. Una sola llamada instanciada por tramo, el vertex shader
  descarta las instancias de otra pasada moviéndolas fuera del recorte (sin costo de fragmentos), las ramas son
  uniformes por instancia y el atlas es un arreglo de texturas. El `discard` con alfa 0 no importa porque no hay
  búfer de profundidad.
- **`mundo.frag` (ola y visión):** correcto, pero obliga a copiar `destino_` entero a `copia_` cada cuadro
  mientras hay agua (22 MB). Con la escena a la grilla lógica (recomendación 1) la copia baja a 2,4 MB.
- **`brillo.frag`:** barato (dos lecturas de texturas chicas); lo caro es la mezcla de multiplicar a pantalla
  completa, que lee y escribe el destino.
- **`escalado_xbr.frag`:** 11 lecturas, bien acotado. Lee `destino_` con muestreo por vecino en el centro de cada
  bloque: correcto solo si cada bloque es de un solo color, que era lo que fallaba al moverse (corregido).
- **`escalado_fsr.frag`:** 12 lecturas y la cuenta de pesos de EASU por cada una: el de más cálculo por píxel. EASU
  está pensado para escalar hasta 2×; a escala 3 o 4 suaviza más de lo que afila (no tiene la pasada RCAS).
- **`escalado_crt.frag`:** el más barato (2 lecturas). La máscara de tríadas va por píxel de `destino_`; con la
  vista fija, el escalado lineal a la ventana la remuestrea y puede dar moiré.
- **Anime4K (`anime4k_c1..c4`, `d2s`, `resample`):** las cuatro convoluciones van a la grilla lógica (baratas en
  píxeles, caras en cuentas: 9 matrices de 4 × 4 por píxel en la primera y 18 en las otras tres). A escala 3 o 4 la imagen de 2× se estira con
  un filtro lineal al final (`resample`), que suaviza. Es el modo más caro medido.
- **`texto.frag` / `texto_comp.frag` (texto HD):** cada cuadro con texto borra dos destinos del tamaño de la
  salida, redibuja como atenuadores todas las instancias no aditivas que van después del primer texto (paneles y
  ventanas enteros) y compone a pantalla completa: unas tres pasadas a pantalla completa, aunque haya cinco
  palabras en pantalla.

## Recomendaciones (de más a menos impacto)

1. **Hecho (2026-11-22). Con un filtro activo, dibujar la escena a la grilla lógica.** `destino_` del tamaño de la grilla (0,61 Mpx en
   vez de 5,48) para las pasadas 2 a 5, y el filtro escribe directo a la resolución de salida. Ahorra ~89 % de esas
   pasadas a escala 3 (94 % a escala 4), le da al filtro su entrada exacta (ya no hace falta leer en el centro de
   cada bloque ni ubicar las instancias en el píxel lógico) y achica la copia del agua. Costo: los shaders de
   filtro y Anime4K dejan de multiplicar por la escala (`u.z = 1`), la captura lógica y `a_grilla` se ajustan, y las
   imágenes del pack HD pierden su detalle con un filtro (hoy ya lo pierden: el filtro lee un punto por bloque).
2. **Vista fija con filtro: escalar una sola vez.** Hoy el filtro escala a la escala interna (hasta 4) y después la
   copia a la ventana reescala de forma lineal (≤ 2×): doble remuestreo, más blando y una pasada más. El filtro
   podría escribir directo al rectángulo de la ventana (`u.zw` = tamaño de `rect_`).
3. **Texto HD más barato:** limitar la composición al rectángulo que ocupa el texto (o a los rectángulos de las
   ventanas), y no redibujar como atenuadores las instancias que no tienen texto debajo.
4. **Agua sin copia entera:** con la recomendación 1 ya baja 9 veces; si no, copiar solo las filas del mundo.

## Cómo se midió

```sh
cmake -S . -B build/perf -G Ninja -DCMAKE_BUILD_TYPE=Release -DLIN_CONTROL=ON && cmake --build build/perf
# con herramientas/lin_ctl.py: un settings.ini por caso (escalado, vista, texto_hd), entrar al juego, `perf on`,
# esperar 5 s y leer las líneas "perf:" del registro (gpu X ms = promedio del último segundo).
```
