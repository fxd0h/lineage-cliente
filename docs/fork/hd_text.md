# HD text (`texto_hd`)

The original bitmap fonts are drawn into the 640x480 frame and scaled with the game, so filters such as anime4k smear
them. With `texto_hd=1` (default) the UI text keeps its glyph shapes, advances, positions and colours but is drawn in a
separate pass **after** the scaling filter, at the real output resolution.

## Pipeline

1. `Graficos::texto` / `texto_html` check `Gpu::texto_hd()` and that the draw key is in a UI layer (>= `CAPA_UI`;
   world-layer text stays pixel). They queue each glyph with `Mezcla::TEXTO_HD` (instance mode bit 6) and a *field*
   region instead of the pixel glyph. Advances still come from the pixel metrics, so layout is byte-identical.
2. `sprite.vert` skips TEXTO_HD instances in the normal passes (mode 0/1).
3. `Gpu::pase_texto` runs after the scaling filter: on the swapchain (fixed view, viewport = letterbox rect), on the
   offscreen window image for `captura-final`, or on the final texture (wide view). Two draws over the same sorted list
   starting at the first text instance: *occluders* (every non-text, non-additive instance, depth only) and then the
   text (`texto.frag`, depth test). A window drawn after a piece of text therefore still hides it.
4. `texto.frag` thresholds the field at 0.5 with a screen-space smoothstep (`fwidth`), giving a one-pixel edge at any
   scale.

## Fields (`recursos/fuente_hd.*`)

A field is `HD_ESCALA` (8) texels per logical pixel, value 0.5 on the outline, with a 1-px margin. It is generated at
runtime from the user's own game files and cached in the atlas like any glyph (never committed).

- **Bitmap fonts** (eng.fnt, kor.fnt, the Departure Mono UI face): the 1-bit mask is upsampled and Gaussian-smoothed
  (sigma 0.42 px); the contour at level 0.44 is the glyph outline. This is an implicit-curve vectorization (the
  iso-contour of a smooth field, resolved by the GPU at any scale), not Bezier tracing. `NIVEL` is the corner knob:
  lower keeps squarer corners and stroke ends. Tests: `tests/test_fuente_hd.cpp`.
- **HTML dialog face**: real unhinted Liberation Sans outlines (SIL OFL, `licencias/LiberationSans-OFL.txt`) rasterized
  by stb_truetype at 8x the 11 ppem size, placed on the same pen/baseline as the hinted pixel glyph. Embedded subset:
  `fuente_html_hd_datos.inc`, from `tools/generar_fuente_html_hd.py`.

## Limits

- SPIR-V only (Linux/Vulkan). MULTIPLATFORM: the MSL twin of the text shaders is missing; macOS keeps pixel text.
- `captura` (logical, pre-filter) forces pixel text for that frame; use `captura-final` to see HD text.
- A translucent window above text hides it completely where it has any alpha (occluders are not blended).
- World-layer text (names, damage numbers) is not HD yet.
- The 4x6 HUD digit font (pantalla_juego.cpp) goes through `Graficos::glifo_hd_mascara`, outline passes included.
