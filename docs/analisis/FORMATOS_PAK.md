# Formatos de .idx/.pak y archivos cifrados

**Estado:** el formato clásico, el DES de Text y el AES de `list.spz` están implementados (`src/recursos/pak.cpp`,
`des.cpp`, `aes.cpp`). `_EXT`, `_RMS` y los tiles Remastered están documentados acá para implementarlos cuando
tengamos un cliente con esos índices para probar.

**Fuentes:** [srwh1234/L1MapViewer](https://github.com/srwh1234/L1MapViewer) (`Reader/L1IdxReader.cs`,
`Reader/L1PakReader.cs`, `Algorithm.cs`, `Helper/L1MapHelper.cs`) y [tony1223/PakViewer](https://github.com/tony1223/PakViewer)
(`PakReader.cs`, `src/Lin.Helper.Core/Pak/IdxHandler.cs`). **L1MapViewer no tiene licencia:** de ahí tomamos el
formato y las constantes (datos), no el código; nuestra implementación es propia (AES según FIPS-197).

Todos los enteros son little-endian.

## 1. Cómo reconocer el índice

| Primeros bytes | Formato | Entrada | Estado |
|---|---|---|---|
| `u32` cantidad (sin marca) | clásico | 28 bytes | implementado |
| `_EXTB$` | extendido de bloques | largo variable | sin implementar |
| `_EXT` (sin `B$` después) | extendido | 128 bytes | sin implementar (§3) |
| `_RMS` | Remastered | 276 bytes | sin implementar (§3) |
| `IDXC 02 01` | propio de Lineage Justice (cifrado por bloques) | ? | **no**: ver §6 |

## 2. Clásico (implementado)

- `.idx`: `u32 n`, después `n` entradas de 28 bytes: `u32 offset`, `char nombre[20]` (terminado en 0),
  `u32 tamaño`.
- **Text.idx:** las `n × 28` bytes de entradas van cifradas con DES-ECB, clave `~!@#%^$<`
  (`CLAVE_DES_LINEAGE`), solo los bloques completos de 8; el resto queda igual. Los archivos de Text.pak
  también van con DES (`Recursos::texto`).
- **Sprites:** `SpriteNN.pak` con `NN = (suma de bytes del nombre en minúsculas) & 15`, después `Sprite.pak`.

### 2.1 Archivos con AES dentro de Text/Data (implementado para list.spz)

Algunos archivos, una vez sacados del pak (y del DES si van en Text), tienen una segunda capa:

| Archivo | Bytes en claro al principio | Cómo se reconoce |
|---|---|---|
| `list.spz` | 5 (`S1354` en la instalación de 2009) | empieza con `S` |
| `.xml`, `.json`, `.ui` | 4 | empieza con `X`; al descifrar, la `X` se cambia por `<` |

El resto va con **AES-128-CBC sin relleno**:
- Clave `DC 84 01 21 2A 40 20 0A DD 25 B9 A7 0D B9 C9 4E`.
- Vector inicial `3E 09 78 AA C4 D5 30 63 30 0C 5F 9A 80 7F 22 46`.
- Solo los bloques completos de 16 bytes. Los `n % 16` bytes finales no pasan por AES: cada uno va con XOR
  contra el byte de la misma posición del **último bloque completo todavía cifrado**.

Implementación: `descifrar_aes_lineage` (`src/recursos/aes.cpp`), probada con FIPS-197 C.1 y NIST SP 800-38A
F.2.2 (`tests/test_aes.cpp`). El cliente descifra el `list.spz` de Text.pak cuando no hay una lista en texto
plano (`cargar_lista_sprites` en `src/app/app.cpp`). El de la instalación de 2009 da 13.540 entradas, ids
0–13543; trae, entre otras, las filas reales de los polymorphs 75/80 que antes copiábamos a mano.

Los `.xml` no los necesitamos hoy (son la UI del Remastered). Para leerlos basta llamar a
`descifrar_aes_lineage` desde el byte 4 y cambiar la `X` inicial por `<`.

## 3. `_EXT` y `_RMS` (para implementar)

Las dos fuentes coinciden en la entrada. La de PakViewer (`IdxHandler.cs`, `FromExtEntry`) es la más completa:

```
Cabecera (8 bytes):   char marca[4] ("_EXT" o "_RMS"), i32 cantidad
Entrada:              u32 offset en el .pak
                      i32 tamaño sin comprimir
                      i32 tamaño guardado (comprimido; 0 si no lo está)
                      i32 compresión: 0 ninguna, 1 zlib, 2 brotli
                      char nombre[112] (_EXT, entrada de 128) o nombre[260] (_RMS, entrada de 276),
                      terminado en 0; en _RMS puede traer subcarpetas ("data/xxx.html")
```

- **Leer un archivo:** se leen `tamaño guardado` bytes (o `tamaño sin comprimir` si no está comprimido) desde
  el offset y se descomprimen. zlib trae su cabecera (`78 9C`/`78 DA`); brotli es un flujo crudo.
- **Text en `_EXT`:** en algunas versiones (V880, según PakViewer) las entradas y los archivos van además con
  el DES de §2. PakViewer prueba primero sin descifrar y, si los nombres no tienen sentido, con DES; para las
  entradas, el DES empieza en el byte 8 (después de la cabecera), no en el 4.
- **`_EXTB$`** (PakViewer, `ExtBHandler`): después de la marca de 6 bytes, `i32 cantidad` y entradas de largo
  variable: `u32 offset`, `i32 tamaño`, `i32 tamaño comprimido`, nombre terminado en 0. Datos en brotli y, si
  falla, zlib sin los 2 bytes de cabecera. PakViewer documenta otra variante con entradas de 128 bytes en
  `PakReader.cs:470`. Hay que verificar cuál corresponde con un archivo real antes de implementar.

### Plan

1. `Pak::abrir`: mirar los primeros 4 bytes y elegir el lector. El clásico queda como está.
2. Guardar por entrada el tamaño guardado y la compresión (hoy `Entrada` tiene offset, nombre y tamaño).
3. `Pak::buscar` devuelve una vista al archivo mapeado. Con compresión hay que descomprimir a un buffer:
   cambiar a `bool leer(nombre, std::vector<u8>& salida)` para los paks comprimidos o cachear el resultado,
   cuidando los llamadores que hoy guardan la vista (sprites y tiles).
4. Dependencias: zlib y brotli no están en `cmake/dependencias.cmake`. Las dos se pueden traer con
   FetchContent (brotli: el decodificador, `c/dec` y `c/common`, unos 10 archivos C).
5. Nombres con subcarpeta (`data/…`): `Recursos::texto` busca por nombre suelto; buscar también con el
   prefijo, o normalizar quitando la carpeta.
6. Text vs Data: en los clientes nuevos las tablas, HTML y `list.spz` están en `Data` con prefijo `data/`. Si
   no hay `Text.idx`, abrir `Data` como texto y aceptar la carpeta en `carpeta_juego_valida`
   (`src/plataforma/rutas.cpp`), que hoy exige `Text.idx`.
7. Pruebas: un índice sintético de cada formato con dos entradas (una sin comprimir y una en zlib o
   brotli), sin datos del juego.

## 4. Mapas y tiles Remastered (para implementar)

Según `Helper/L1MapHelper.cs`:

- **Detección:** L1MapViewer usa la carpeta `bin32/` de la instalación. Mejor detectarlo por el tamaño de los
  tiles (abajo), que no depende de la instalación.
- **`.s32`:** mismo formato que el clásico (capa 1: 64×128 celdas de `u8 índice, u16 til, u8 ?`; capa 2:
  `u16 n` × 6 bytes; capa 3: 64×64 × 4 bytes de atributos; capa 4: objetos). En Remastered los `.seg` se
  ignoran y solo se usan los `.s32`.
- **Tiles al doble:** cada medio rombo mide 48×24 en vez de 24×12, así que un bloque de mapa ocupa
  64·48·2 × 64·24·2 px (6144 × 3072) en vez de 3072 × 1536. En `drawTilBlockR` el origen de cada celda es
  `x0 = −48·(x/2)`, `y0 = 63·24 − 24·(x/2)`, y los tiles rectos (tipos 0/1/8/9/16/17) tienen 48 filas.
- Los tiles irregulares (tipos 34/35, con cabecera `x, y, ancho, alto`) ya los resolvemos por el bit 2 en
  `src/recursos/tiles.cpp`; en Remastered vienen con las mismas cabeceras y dimensiones dobles.

### Plan

1. `tiles.cpp`: leer el tamaño del tile en vez de suponer 24×12 (el largo del bloque recto lo dice: 24 filas
   clásico, 48 Remastered) y guardar la escala en la tabla de tiles.
2. Render: dibujar el bloque Remastered a la mitad (a la cuadrícula lógica de 24×12, igual que hoy) o
   aprovecharlo como textura de doble resolución en las vistas con escala ≥ 2.
3. Probar con un `.til` Remastered de cada tipo y un `.s32` contra una captura de L1MapViewer.

## 5. Nombres de zona (opcional)

`zone3desc-c.tbl` (Text) y `zone3-c.xml` (Tile o Data; `zone3.tbl`/`zone2-c.tbl` en clientes viejos) dan el
nombre de cada zona del mapa (`<zoneinfo><zone><zone3desc>…</zone3desc><map>…</map>`). Servirían para mostrar
el nombre de la zona en el minimapa.

## 6. Lineage Justice (`IDXC`): no

El cliente nuevo de Justice usa índices `IDXC 02 01` con el cuerpo cifrado por bloques de 16 bytes, y su
launcher prohíbe expresamente analizarlo. No aparece en ninguna de las fuentes de arriba. Para leerlo hace
falta que sus operadores publiquen el formato o lo autoricen. Sus mapas sueltos (`map/`) sí son el formato
clásico y se pueden usar con `juego_extra=`.
