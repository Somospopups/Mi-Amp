# Guía para Crear Nuevos Fondos en MI AMP

Esta guía resume exactamente cómo deben armarse los **dos archivos de fondo** de una locación nueva para que funcione desde la **animación inicial**, pase por la **vista completa y close-up del cabezal**, y coincida con el **fondo del pedalboard**.

---

## 1. Los 2 archivos que componen una locación

| Uso en la app | Archivo actual | Medidas exactas | Formato |
|---|---|---|---|
| **Intro + Escenario del Amp (`fullView` y `closeUpView`)** | `Resources/rig/skatepark_tall_capa0.jpg` | **`1550 × 1924 px`** | `.jpg` RGB (`q=88`, ~350–550 KB) |
| **Fondo del Pedalboard (`paintFloorScene`)** | `Resources/floor_bg.png` | **`1472 × 720 px`** | `.png` RGB (~400–700 KB) |

> **Regla de oro:** ambos archivos deben pertenecer al **mismo lugar y mismo material de suelo** (por ejemplo: concreto de skatepark, piso de madera de estudio, escenario de madera oscura, baldosa industrial, etc.).

---

## 2. Cómo diseñar el fondo vertical (`1550 × 1924 px`)

El motor (`Source/PluginEditor.cpp` → `paintHeadScene`) recorre **una sola imagen vertical continua** de arriba hacia abajo durante la animación inicial de **2.9 segundos**:

1. **Zona Superior / Animación del comienzo (`y = 0 .. 768`):**
   - Al arrancar el plugin (`introRunning`), la cámara muestra el rectángulo superior `1550 × 768`.
   - El logo **`intro_logo.png`** se dibuja centrado alrededor de `x = 325..1225`, `y = 195..445`.
   - **Requisito:** dejar **cielo abierto, techo limpio o fondo uniforme con buen contraste** en `y = 140 .. 460` para que el logo se lea perfecto.
2. **Zona Media / Paneo de descenso (`y = 600 .. 1156`):**
   - La cámara desciende suavemente `1156 px` hasta llegar al escenario final (`y = 1156 .. 1924`).
   - Acá van copas de árboles, techos, luminarias de estudio, edificios lejanos o pared superior, **sin cortes ni saltos horizontales**.
3. **Zona Inferior / Escenario del Amplificador (`y = 1156 .. 1924`, alto `768 px`):**
   - **Horizonte del piso:** el suelo real debe empezar entre **`y = 1600 .. 1660`** (por detrás de la caja) y avanzar hacia abajo con perspectiva natural hasta **`y = 1924`**.
   - **Líneas de apoyo reales del gabinete (`computeRigStageLayout`):**
     - Con consola abierta: **`y = 1774`** (`1156 + 618`).
     - Con consola cerrada: **`y = 1876`** (`1156 + 720`).
   - **Close-Up (`closeUpView`):** hace zoom **`1.22×`** sobre el recorte `x = 98..1368, y = 1202..1832`. Conviene que entre `y = 1200 .. 1700` el fondo tenga **bokeh / desenfoque suave** y sin objetos muy contrastados justo detrás de los knobs.

---

## 3. Cómo diseñar el fondo del pedal (`1472 × 720 px`)

El motor (`paintFloorScene`) estira `Resources/floor_bg.png` a pantalla completa y coloca encima:
- La **paleta de pedales** a la izquierda (`x = 1% .. 34%`).
- El **pedalboard (`board_surface.png`)** a la derecha (`x = 37.8% .. 97.2%`, `y = 12.5% .. 77.5%`).

**Requisitos:**
1. **Cámara cenital pura (`90°` mirando directo hacia abajo al piso)** — sin perspectiva inclinada ni horizonte.
2. **Mismo material e iluminación** que el piso de la Zona Inferior (`y = 1640 .. 1924`) del fondo vertical.
3. Textura continua y pareja, sin objetos 3D altos ni sombras duras pre-dibujadas (el plugin ya dibuja sus propias sombras de contacto).

---

## 4. Script incluido (`scripts/crear_fondos_mi_amp.py`)

```bash
# 1. Verificar que los fondos actuales cumplen las medidas exactas
python3 scripts/crear_fondos_mi_amp.py --check

# 2. Exportar plantillas visuales con las zonas de seguridad marcadas
python3 scripts/crear_fondos_mi_amp.py --template /tmp/plantillas_fondos

# 3. Convertir una imagen vertical nueva a 1550x1924 JPG lista para Resources/rig/
python3 scripts/crear_fondos_mi_amp.py --build-rig mi_fondo_tall.png Resources/rig/skatepark_tall_capa0.jpg

# 4. (Alternativa) Unir un cielo + un escenario con transición suave (smoothstep)
python3 scripts/crear_fondos_mi_amp.py --stitch cielo.png escenario.png Resources/rig/skatepark_tall_capa0.jpg

# 5. Convertir una textura cenital a 1472x720 PNG para el fondo del pedalboard
python3 scripts/crear_fondos_mi_amp.py --build-floor mi_piso_cenital.png Resources/floor_bg.png
```

---

## 5. Si el día de mañana se agrega un selector de múltiples fondos

1. Agregar los nuevos pares en `CMakeLists.txt` (ejemplo: `skatepark_tall_capa0.jpg` + `floor_bg.png`, `studio_tall_capa0.jpg` + `studio_floor_bg.png`).
2. Agregar un parámetro `bgTheme` en `MiAmpParams` (`PluginProcessor.cpp`) y cargarlos en `MiAmpAudioProcessorEditor::loadImages()`.
3. En `paintHeadScene` y `paintFloorScene`, usar el par activo manteniendo exactamente `1550 × 1924` (rig) y `1472 × 720` (pedales) para que la animación inicial, el close-up `1.22×` y las líneas de apoyo (`618` / `720`) funcionen idénticos sin tocar coordenadas.
4. Sincronizar el nuevo fondo en `miamp_preview/pack_standalone_html.py` y `miamp_preview/public/`.
