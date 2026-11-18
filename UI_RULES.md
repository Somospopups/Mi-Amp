# Reglas Básicas de UI, Animación y Assets — MI AMP

Estas reglas son obligatorias para cualquier cambio de interfaz, código C++ (`Source/`), assets (`Resources/rig/`) o previews en este repositorio.

---

## 1. Contención, Márgenes y Cero Superposición
- **Nada puede salir de su contenedor:** ningún botón, texto, slider, scrollbar ni control puede sobresalir de los bordes de su panel (`dock`, `GlassPopover`, paneles de vidrio) ni invadir sus esquinas redondeadas.
- **Márgenes de seguridad en paneles redondeados (`radius = 22`):**
  - Mantener margen mínimo libre respecto al borde superior, inferior y laterales para que ningún botón rectangular toque la curva de las esquinas.
  - En la botonera inferior derecha del dock de pedalera (`36, 623, 1478, 125`): dejar `>= 14 px` libres arriba y abajo (`y = 637..733`), `>= 24 px` respecto al borde derecho (`x_fin <= 1486` frente a `1514`) y `>= 16 px` de aire desde la flecha `>`.
- **Cero elementos encimados:** nunca superponer botones, menús, textos ni controles entre sí. Mantener separación mínima (`>= 4–6 px`) entre controles vecinos.
- **Títulos completos:** usar `drawFittedText` en encabezados de popovers para evitar truncamientos con `...` (por ejemplo `BUSCAR TONO · GUITARRA`).

---

## 2. Popovers del Menú Derecho (Vista de Ampli)
- Al abrir cualquier popover desde el menú derecho (`PRESETS`, `AFINADOR & DROP`, `BUSCAR TONO`, `CONFIG`, `DROP`), **ocultar el menú de opciones derecho** (`GUITARRA/BAJO`, botones laterales y medidores `IN/OUT`) para evitar superposiciones.
- Todos estos popovers deben incluir el botón **`←`** arriba a la izquierda para volver al menú de opciones.
- **Altura dinámica con tope:**
  - Comienzan en **`y = 25`** (borde superior del menú).
  - Su **tope máximo inferior es `y = 698` (`maxH = 673`)**.
  - **Deben terminar exactamente donde terminan sus controles** (con su margen inferior limpio), sin dejar espacio vacío muerto debajo.

---

## 3. Vista de Pedalera (`Floor Mode`)
- El botón **`CONFIG`** no se muestra en la pantalla de pedalera (solo en la vista principal del ampli).
- La botonera inferior derecha del dock se distribuye en 3 filas limpias y centradas:
  1. `PRESETS` | `AFINADOR`
  2. `BUSCAR TONO` (ancho completo)
  3. `VOLVER AL AMPLI` (ancho completo)

---

## 4. Miniaturas, Grillas y `CABINET OFF / SIN CAJA (DI)`
- **Miniaturas siempre al 100% de brillo:** todos los cabezales y cajas seleccionables en grillas y previews (`CabPanels.h`, `RigSelectorModal`) deben renderizarse con opacidad `1.0f`. Nunca atenuar ni "apagar" opciones no seleccionadas; la selección ya se distingue por su borde resaltado y la vista central.
- **`CABINET OFF / SIN CAJA (DI)`:** dibujar el gabinete real sombreado/desaturado (`makeDimmedCabImage` + `drawImage(..., false)`) conservando el detalle de textura y parlantes — nunca una silueta sólida plana ni `fillAlphaChannelWithCurrentBrush = true`.

---

## 5. Animación de Inicio (`Intro`)
- La cámara debe descender de forma **100% continua y subpíxel** (`drawImageTransformed` + `AffineTransform`) sobre `skatepark_tall_capa0.jpg`.
- **Prohibido hacer saltos discretos en mitad de la animación:** el zoom de `closeUpView` (`1.00 → 1.22`) debe interpolarse suavemente en sincronía con la bajada de cámara para que no haya cortes visibles en las palmeras ni en el cielo, coincidiendo exactamente con la vista final en reposo al terminar.

---

## 6. Assets RGBA y Autenticidad Visual
- **Bordes limpios (`Resources/rig/`):** todos los PNG de cabezales, cajas, micrófonos, perillas y switches deben mantener alfa descontaminado, sin halos ni artefactos en los bordes del lienzo.
- **Formato PNG obligatorio (`RGBA 32-bit` / `color_type = 6`):** **NUNCA** guardar assets críticos nuevos o regenerados como PNG8 indexado con paleta + `tRNS` (`color_type = 3`, por ej. cuantización `FASTOCTREE`) ni superar `1536 px` en el lado más largo en imágenes con alfa: en Windows con JUCE 8 (`libpng` + Direct2D), los PNG8 indexados con ciertos bloques `PLTE`/`tRNS` o alturas elevadas pueden decodificarse/renderizarse como una **silueta negra sólida (`#000000`)** manteniendo intacta la máscara alfa. Guardar siempre como **PNG 32-bit RGBA puro (`color_type = 6`, sin `PLTE` ni `tRNS`)**, con RGB puesto a `0` en píxeles donde `alpha == 0`.
- **Controles completos:** cada cabezal debe mostrar todas sus perillas, switches y faders reales alineados según `Source/AmpCatalogSpecs.h`.
- **Previews 100% reales:** cualquier captura o preview visual debe reflejar exactamente las posiciones y proporciones reales del plugin (no reubicar paneles en lugares donde no existen).

---

## 7. Estética Oficial de Vidrio Equilibrado (Referencia Única Obligatoria)
**Todos los paneles de vidrio del plugin** (selector izquierdo de `CAJAS / MICRÓFONOS`, menú derecho, cápsulas laterales del cabezal, barra inferior de pedalera, `GlassPopover` de Presets/Afinador/Buscar Tono/Config, `StudioFxModal`, `RigSelectorModal`, `UpdateOverlayComponent` y tarjetas internas) **DEBEN ser exactamente iguales**, usando el mismo tono **"Vidrio: Equilibrado (Legible)"** definido en `Source/MiAmpGlassTheme.h`:

- **Panel principal de vidrio (`paintFrostedGlassPanel`):**
  - Gradiente diagonal (`top-left (x, y)` → `bottom-right (right, bottom)`):
    - Inicio: `rgba(84, 82, 72, 143)` (`0.56` alfa)
    - Fin: `rgba(34, 37, 34, 204)` (`0.80` alfa)
  - Borde (`1.5 px`): `rgba(255, 243, 210, 168)` (`0.66` alfa)
  - Radio estándar: `22 px` (`20 px` en el panel izquierdo de cajas/mics)
- **Botones y tipografía sobre vidrio:** usar tinta `#FFFAF0` (`255, 250, 240`) con sombra sutil de `1 px` (`rgba(0, 0, 0, 165)`) para garantizar legibilidad 10/10 sobre el cielo claro del fondo.
- **Sub-paneles / Contenedores interiores (`paintGlassSubPanel`):**
  - Gradiente vertical (`top` → `bottom`): `rgba(255, 250, 238, 41)` → `rgba(230, 218, 190, 23)`, borde `1.2 px` `rgba(255, 246, 222, 107)`
- **Tarjetas / Celdas seleccionables (`paintGlassCard`):**
  - Normal: gradiente vertical `rgba(255, 250, 238, 56)` → `rgba(236, 224, 198, 31)`, borde `1.2 px` `rgba(255, 246, 222, 122)`
  - Hover: gradiente vertical `rgba(255, 252, 244, 87)` → `rgba(242, 232, 208, 56)`, borde `1.4 px` `rgba(255, 246, 222, 198)`
  - Seleccionada: relleno `rgba(166, 223, 201, 71)`, borde `1.8 px` `rgb(166, 223, 201)`
- **Regla estricta:** cualquier nuevo panel de vidrio debe llamar directamente a `miamp_theme::paintFrostedGlassPanel`, `paintGlassSubPanel` y `paintGlassCard`.

---

## 8. Selector Unificado de Cajas y Micrófonos (`CAJAS ↔ MICRÓFONOS`)
- **Un solo panel izquierdo** (`x = 28, y = 25, w = 420, h = 716`, o `595` si la consola inferior está abierta) con toggle en la cabecera **`[ CAJAS | MICRÓFONOS ]`**.
- **Modo `CAJAS`:** Hero grande arriba **sin flechas `< >` laterales en la caja** (ya son innecesarias al tener todas las cajas visibles abajo) + **grilla `5×2`** debajo mostrando los **10 modelos de guitarra / 9 de bajo completos sin recorte lateral**.
- **Íconos propios (sin repetir la caja Mint):** `CUSTOM WAV IR` usa ícono propio de onda `.WAV IR` y `SIN CAJA (DI)` usa ícono propio rojo `DIRECT DI`.
- **Modo `MICRÓFONOS`:** el mismo Hero grande muestra los micrófonos arrastrables sobre la grilla del gabinete y debajo aparecen los controles completos (`MI-57`, `MI-87`, distancia `CLOSE/MED/FAR`, fase `Ø`, `MIC MIX`, `ROOM` y botón `← VOLVER A CAJAS`).
- **Identidad MI AMP:** usar exclusivamente la marca `MI AMP` en UI, código C++, recursos y nombres de archivo (prohibido reutilizar nombres externos anteriores).

---

## 9. Encuadre de Cabezal y Flechas Fijas (`Cápsula Fina`)
- **Tamaño de cabezal en Primer Plano (`closeUpView == true`):**
  - **Guitarra:** `centerX = 675`, `actualW = 860`, `cabTopY = 555` (cuando se abre `CAJAS`, transiciona a `870 / 680 / 505`).
  - **Bajo:** `centerX = 665`, `actualW = 960`, `cabTopY = 555` (cuando se abre `CAJAS`, transiciona a `870 / 680 / 505`).
- **Flechitas laterales (`HeadSideArrowButton` — Cápsula Fina):**
  - Forma de cápsula vertical redondeada (`30 × 66 px`, `rad = width * 0.5f`) con el vidrio equilibrado oficial y un único chevron fino redondeado.
  - **Posición 100% fija por instrumento** (`ay = 348` en guitarra, `ay = 340` en bajo; `335` con cajón abierto): **jamás** deben moverse arriba/abajo o a los costados al cambiar entre distintos modelos de cabezal.
