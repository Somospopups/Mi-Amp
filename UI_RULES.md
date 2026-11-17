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
