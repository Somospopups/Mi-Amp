# MI AMP — Guía Completa de Arquitectura y Desarrollo (`PROJECT_HANDOFF.md`)

Documento de referencia técnica integral para retomar el desarrollo de **MI AMP** (Guitar + Bass Suite — VST3, AU y Standalone en JUCE / C++20).

---

## 1. Visión General del Producto
**MI AMP** es una suite de amplificación dual (**Guitarra** y **Bajo**) con escenario 3D fotorealista, pedalboard interactivo, sección dual de microfonía/IRs y rack de efectos de estudio:
- **Stage principal**: `1550 × 768` lógico (`stageScale()`), con intro animada de cámara descendente (`2.9 s` sobre un único fondo vertical continuo `1550 × 1924`), modo **Close-Up** (`1.22×`) y **Full Rig** (`Console` abierta o cerrada).
- **Modo Floor / Pedalboard**: vista cenital (`1472 × 720`) con paleta izquierda de pedales drag-and-drop y pedalboard interactivo.
- **Formato de compilación**: JUCE 7/8 + CMake (`CMakeLists.txt`) con GitHub Actions Windows x64 (`VST3` + `Standalone` + instalador NSIS `.exe`) y macOS.

---

## 2. Mapa de Archivos en `Source/`

| Archivo | Responsabilidad |
|---|---|
| `Source/PluginProcessor.h` / `.cpp` | `AudioProcessor`, APVTS (~296 parámetros), ruteo Guitar vs Bass, cadena de pedales dinámica (`slot[10]`), carga de `.wav` Custom IR (`loadCustomCabinetIrFile`), estado XML/presets. |
| `Source/PluginEditor.h` / `.cpp` | Escenario principal (`paintHeadScene`, `paintFloorScene`, `paintOverChildren`), animación de intro, `computeRigStageLayout()`, controles dinámicos sobre `headR`, popovers laterales, consola inferior, pedalboard y drag-and-drop. |
| `Source/AmpCatalogSpecs.h` | Catálogo visual y calibración normalizada (`0..1`) de **18 cabezales** (9 guitarra + 9 bajo) y **10 cajas guitarra / 9 cajas bajo** (`grilleX/Y/W/H`, `knobs`, `switches`, `sliders`). |
| `Source/PedalCatalogSpecs.h` | Especificaciones visuales y parámetros de los pedales de **Guitarra (13)** y **Bajo (14)**. |
| `Source/MiAmpGlassTheme.h` | **Referencia única obligatoria de `Vidrio Claro`**: `paintFrostedGlassPanel`, `paintGlassSubPanel` y `paintGlassCard` compartidas por todos los paneles, popovers y modales. |
| `Source/RigStageComponents.h` | Componentes de UI del escenario: `RigKnob`, `RigSwitch`, `RigSlider`, `PedalPaletteItem`, `makeDimmedCabImage`, `RigSelectorModal`. |
| `Source/CabPanels.h` | Panel unificado izquierdo (`28, 25, 420, 716`) con toggle `[ CAJAS | MICRÓFONOS ]`, Hero sin flechas laterales, grilla `5×2`, íconos propios `.WAV IR` y `DIRECT DI`, y controles Dual Mics (`MI-57`/`MI-87`, `MIC MIX`, `ROOM`). |
| `Source/StudioFxModal.h` | Modal de efectos de estudio post-cab (Gate/Comp/EQ/Delay/Reverb/Limiter master). |
| `Source/ToneFinderEngine.h` | Buscador inteligente de presets/tonos por género, instrumento y artista; sincronizado con los índices de heads/cabs actuales (`7` = `4x12 MI CLASSIC`, `6` = `8x10 MI CLASSIC`, etc.). |
| `Source/PresetExporter.h` | Exportación e importación de presets en disco. |
| `Source/Localization.h` | Traducciones i18n (`ES` / `EN`). |
| `Source/AutoUpdater.h` | Chequeo de actualizaciones en GitHub Releases (`Somospopups/Mi-Amp`). |
| `Source/StandaloneAudio*.{h,cpp}` | Puente de audio/dispositivos para la versión Standalone. |

### DSP en `Source/DSP/`
- `MiAmpGuitarChainDSP.h`: 9 topologías de cabezales de guitarra (preamp, tone stack, power amp saturación/sag, presence/resonance).
- `MiAmpBassChainDSP.h`: 9 topologías de cabezales de bajo (incluyendo GEQ 9-bandas en `PRO-450`, Dual-Engine Microtubes en `M-900`, Bi-Amp en `RB-800`, etc.).
- `MiAmpCabMicDSP.h`: motor de cabinas + microfonía dual + convolución **Custom WAV IR** + modo bypass `CAB OFF`.
- `MiAmpBoutiquePedalsDSP.h` + `XVBassDSP.h` + `XVBassTuner.h` + `XVPitchShifter.h`: pedales boutique, afinador cromático y pitch shifter polifónico.
- `MiAmpStudioFxDSP.h`: cadena de efectos de estudio master.
- `MiAmpCabIRs.h` / `XVCabIRs.h`: respuestas al impulso embebidas en código.

---

## 3. Catálogo Actual de Cabezales y Cajas (`Source/AmpCatalogSpecs.h`)

### Guitarra — 9 Cabezales (`gtr_head_model`, `0..8`)
`0`: `MI AMP 800` · `1`: `MI AMP PLEXI` · `2`: `MI AMP RECTO` · `3`: `MI AMP HG-120` · `4`: `MI AMP AC-30` · `5`: `MI AMP ROCKER` · `6`: `MI AMP BE-100` · `7`: `MI AMP TWIN` · `8`: `MI AMP CLASSIC` (Mint original).

### Guitarra — 10 Cajas (`gtr_cab`, `0..9`)
- `0`: `4x12 GREENBACK` (`cab_4x12_green.png`)
- `1`: `4x12 BRIT V30` (`cab_4x12_britv30.png`)
- `2`: `4x12 OVERSIZED` (`cab_4x12_recto.png`)
- `3`: `4x12 HI-GAIN` (`cab_4x12_evh.png`)
- `4`: `2x12 CLASS-A` (`cab_2x12_classa.png`)
- `5`: `4x12 TANGERINE` (`cab_4x12_tangerine.png`)
- `6`: `2x12 BLACKFACE` (`cab_2x12_twin.png`)
- `7`: **`4x12 MI CLASSIC`** (`cab_4x12_mint.png`, **880×888 px**, 32-bit RGBA, Mint Tolex original, default)
- `8`: `CUSTOM WAV IR` (usa `cab_4x12_mint.png`, carga `.wav` externo)
- `9`: `CAB OFF` (`isOff = true`, muestra imagen atenuada de la caja original sin grill/mics)

### Bajo — 9 Cabezales (`bass_head_model`, `0..8`)
`0`: `MI AMP CL-300` · `1`: `MI AMP PRO-450` (`bass_head_svt3pro.png`) · `2`: `MI AMP BASSMAN` · `3`: `MI AMP RB-800` (`bass_head_gk800.png`) · `4`: `MI AMP M-900` · `5`: `MI AMP B-15` · `6`: `MI AMP OR-200` · `7`: `MI AMP DB-750` (`bass_head_aguilar.png`) · `8`: `MI AMP CLASSIC` (`bass_head_mint.png`).

### Bajo — 9 Cajas (`bass_cab_model`, `0..8`)
- `0`: `1x15 VINTAGE` (`bass_cab_1x15_b15.png`)
- `1`: `4x10 TOURING` (`bass_cab_4x10_aguilar.png`)
- `2`: `4x10 HI-PUNCH` (`bass_cab_4x10_gk.png`)
- `3`: `6x10 HEAVY` (`bass_cab_6x10_heavy.png`)
- `4`: `8x10 CLASSIC` (`bass_cab_8x10_classic.png`)
- `5`: `8x10 TANGERINE` (`bass_cab_8x10_tangerine.png`)
- `6`: **`8x10 MI CLASSIC`** (`bass_cab_8x10_mint.png`, **600×1423 px**, 32-bit RGBA, Mint Tolex original, default)
- `7`: `CUSTOM WAV IR` (usa `bass_cab_8x10_mint.png`, carga `.wav` externo)
- `8`: `CAB OFF` (`isOff = true`, salida DI directa)

---

## 4. Documentación de UI, Assets y Fondos en el Repo
Revisar siempre estos archivos antes de modificar gráficos o layouts:
- **`UI_RULES.md`**:
  - Reglas estrictas de contención del dock/console (`14px`/`28px`/`16px`), popovers derechos que nunca tapan el dock, thumbnails sin opacidad heredada (`g.setOpacity (1.0f);`), manejo de cajas `CAB OFF` vía `makeDimmedCabImage`, y sincronización de controles en `syncHeadControls()` sin pisar popovers activos.
- **`FONDO_GUIA.md` + `scripts/crear_fondos_mi_amp.py`**:
  - Especificación exacta de `Resources/rig/skatepark_tall_capa0.jpg` (`1550×1924`) y `Resources/floor_bg.png` (`1472×720`) para agregar o reemplazar locaciones sin romper la intro de `2.9s`, el zoom `closeUpView` (`1.22×`), ni el pedalboard cenital.
- **Resolución y formato de assets en `Resources/rig/`**:
  - Todos los cabezales son `1200 px` de ancho RGBA sin fondo ni patas sobrantes.
  - Si un cabezal tipo rack (`PRO-450`, `RB-800`, `DB-750`) tiene esquineros cromados, dejar `≥ 5 px` de aire transparente en los bordes laterales y asegurar que el riel horizontal inferior sea continuo por encima de las patas de goma.
  - Las cajas originales (`4x12 MI CLASSIC` `880×888` y `8x10 MI CLASSIC` `600×1423`) deben guardarse siempre en **PNG 32-bit RGBA puro (`color_type = 6`, sin paleta `PLTE`/`tRNS`)** y mantenerse bajo `1536 px` de altura (evitando el bug de silueta negra en Windows JUCE 8 `libpng` + Direct2D) y por encima de `580 px` de ancho para que `closeUpView` se vea 100% nítido y en color real de 24-bit sin cuantización.
  - Todo nuevo recurso agregado en `Resources/` debe declararse en `CMakeLists.txt` (`juce_add_binary_data`) para compilar en `BinaryData`.

---

## 5. Flujo de Build & Distribución
- **Compilación automática en la nube**: al hacer `git push origin main`, `.github/workflows/build_windows.yml` compila Release `VST3` + `Standalone` + instalador NSIS `.exe` en `windows-latest` (y build macOS si corresponde).
- **Compilación local en Windows**: ejecutar `build_windows.bat` (requiere CMake + Visual Studio Build Tools; opcional NSIS para el `.exe`).
- **Previo web interactivo (`miamp_preview/`)**: si se agregan cabezales, cajas, pedales o parámetros, actualizar `miamp_preview/build_catalog.py` (sincronizado con `Source/AmpCatalogSpecs.h`) y re-empaquetar si se usa standalone.

---

## 6. Estado Alcanzado en esta Entrega (`v28`)
- **Encuadre por Instrumento y Flechas Fijas (`Source/PluginEditor.cpp` + `Source/CabPanels.h`):**
  - En Primer Plano (`closeUpView`), **Guitarra** usa `centerX=675, actualW=860, cabTopY=555` (zoom fondo `1.18×`) y **Bajo** usa `centerX=665, actualW=960, cabTopY=555` (zoom fondo `1.24×`), acomodando ambos a `870 / 680 / 505` al abrir el panel de `CAJAS`.
  - **`HeadSideArrowButton` (`Cápsula Fina`)**: pastilla vertical redondeada (`30 × 66 px`, 1 único chevron fino) en **coordenadas fijas (`133`/`1167`, `y=344` en pantalla principal)** inmóviles entre Guitarra, Bajo y todos los cabezales.
- **Transición Guitarra ↔ Bajo (`#4 Elevación de Estudio Suave`, `0.62s`) y Pantalla Principal Centrada (v29)**:
  - Crossfade sedoso de `0.62s` con desplazamiento sutil (`-5px` salida / `+8px` entrada), **sin parpadeo** y **con zoom de fondo fijo en `1.18f`** en Primer Plano.
  - En la pantalla principal (`!isFloorMode`), el menú derecho (`1291, 173, 224, 422`) y los popovers derechos (`1206, (768-h)/2`) van centrados verticalmente y **todos los popups** (`GlassPopover`, `CabPickerPanel`, `StudioFxModal`, `RigBrowserOverlay`) animan suavemente apertura y cierre sin superponerse (`closeFloatingOverlays`).
- **Blindaje de Audio Standalone sin DAW (`StandaloneAudioImpl.cpp` + `PluginProcessor.cpp`)**:
  - Arranque con entrada estrictamente silenciada (`0.0f`) hasta verificar interfaz USB/ASIO/Line-In real; bloqueo estricto de micrófonos integrados/Realtek/Intel Smart Sound/High Definition Audio/loopback en español e inglés; sin persistir bypass manual de micrófono entre reinicios.
  - Sanitización `NaN`/`Inf`, limitador suave de seguridad (`±0.94f`) y corte automático anti-acople sostenido en `processBlock()`.
- **Selector Unificado `CAJAS ↔ MICRÓFONOS` (`Source/CabPanels.h`)**:
  - Un solo panel izquierdo (`28, 25, 420, 716`) con pill toggle **`[ CAJAS | MICRÓFONOS ]`** en la cabecera, Hero sin flechas `< >` laterales, grilla **`5×2`** completa e íconos propios para **`.WAV IR`** y **`DIRECT DI`**.
- **Vidrio Equilibrado Único (`Source/MiAmpGlassTheme.h` + `UI_RULES.md` §7 y §9)**:
  - Todos los paneles, popovers, modales, cápsulas y botones usan el mismo **Vidrio Equilibrado (Legible)** (`(84,82,72,143) → (34,37,34,204)`, borde `(255,243,210,168)`) con tinta `#FFFAF0` y sombra `1 px` para lectura nítida sobre cielo claro.
- **Identidad 100% `MI AMP`**: eliminada toda referencia anterior en UI, código C++, recursos y nombres de archivo (`MiAmpCabIRs.h`, `cab_4x12_mint.png`, `head_miamp_mint.png`, `knob_miamp_mint.png`).
- **Assets RGBA 32-bit (`ct=6`)**: `cab_4x12_mint.png` (`880×888`) y `bass_cab_8x10_mint.png` (`600×1423`) y guía de fondos (`FONDO_GUIA.md` + `scripts/crear_fondos_mi_amp.py`).
