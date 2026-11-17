#!/usr/bin/env python3
"""
crear_fondos_mi_amp.py — Herramienta y referencia técnica para crear fondos de MI AMP.

Documenta y automatiza las medidas exactas que usa `Source/PluginEditor.cpp`
(y `miamp_preview/`) para:
  1) El fondo panorámico vertical continuo (`Resources/rig/skatepark_tall_capa0.jpg`, 1550x1924):
     - Zona A (y =    0 ..  768): Animación de entrada (intro, 2.9s) + logo `intro_logo.png`.
     - Zona B (y =  600 .. 1156): Barrido vertical de cámara al descender hacia el amplificador.
     - Zona C (y = 1156 .. 1924): Escenario final del amp (`fullView` y `closeUpView` con zoom 1.22x).
  2) El fondo cenital de la pedalera (`Resources/floor_bg.png`, 1472x720):
     - Vista cenital (90° desde arriba) del mismo suelo donde está apoyada la caja en Zona C.

Uso:
  python3 scripts/crear_fondos_mi_amp.py --check
  python3 scripts/crear_fondos_mi_amp.py --template /tmp/plantillas
  python3 scripts/crear_fondos_mi_amp.py --build-rig mi_fondo_tall.png Mi-Amp/Resources/rig/skatepark_tall_capa0.jpg
  python3 scripts/crear_fondos_mi_amp.py --stitch cielo.png escenario.png Mi-Amp/Resources/rig/skatepark_tall_capa0.jpg
  python3 scripts/crear_fondos_mi_amp.py --build-floor mi_piso_cenital.png Mi-Amp/Resources/floor_bg.png
"""

from __future__ import annotations
import argparse
import pathlib
import sys
from PIL import Image, ImageDraw, ImageOps

# ==============================================================================
# ESPECIFICACIÓN EXACTA DEL MOTOR GRÁFICO (Source/PluginEditor.cpp)
# ==============================================================================
RIG_BG_W = 1550
RIG_BG_H = 1924
VIEWPORT_H = 768
MAX_CAM_Y = RIG_BG_H - VIEWPORT_H  # 1156 px (headSceneYOffsetStage inicial)

# Coordenadas clave dentro del canvas 1550 x 1924:
INTRO_LOGO_BOX = (325, 195, 1225, 445)       # Donde aparece intro_logo.png al t=0 (camY=0)
CLOSEUP_CROP   = (98, 1202, 1368, 1832)      # Zoom 1.22x en closeUpView (centrado en x~733 con drawer)
FLOOR_Y_CONSOLE_OPEN   = 1156 + 618          # 1774: apoyo de ruedas/patas con consola abierta
FLOOR_Y_CONSOLE_CLOSED = 1156 + 720          # 1876: apoyo de ruedas/patas con consola cerrada
GROUND_HORIZON_MIN_Y   = 1600                # Recomendado: el piso real empieza entre y=1600..1670

# Fondo de pedalera (Resources/floor_bg.png):
FLOOR_BG_W = 1472
FLOOR_BG_H = 720
# Posición relativa del pedalboard (board_surface.png) en coordenadas normalizadas [0..1]:
BOARD_NORM_RECT = (585.79 / 1550.0, 95.73 / 768.0, 921.48 / 1550.0, 499.54 / 768.0)


def fit_cover(im: Image.Image, target_w: int, target_h: int) -> Image.Image:
    """Escala manteniendo proporción (cover/crop centrado) exactamente a (target_w, target_h)."""
    return ImageOps.fit(im.convert("RGB"), (target_w, target_h), method=Image.LANCZOS, centering=(0.5, 0.5))


def build_rig_bg(src_path: pathlib.Path, out_path: pathlib.Path, quality: int = 88) -> None:
    im = Image.open(src_path)
    out = fit_cover(im, RIG_BG_W, RIG_BG_H)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out.save(out_path, format="JPEG", quality=quality, optimize=True, progressive=True)
    print(f"[OK] Fondo vertical generado: {out_path} ({out.size[0]}x{out.size[1]}, {out_path.stat().st_size:,} B)")


def stitch_rig_bg(sky_path: pathlib.Path, stage_path: pathlib.Path, out_path: pathlib.Path,
                  blend_y0: int = 920, blend_y1: int = 1220, quality: int = 88) -> None:
    """
    Une un cielo/entorno superior y un escenario inferior en un único fondo continuo 1550x1924.
    - sky_path:   cubre y = 0 .. blend_y1 (recomendado 1550x1240 o más alto)
    - stage_path: cubre y = blend_y0 .. 1924 (recomendado 1550x1050 o más alto)
    """
    import numpy as np
    sky = fit_cover(Image.open(sky_path), RIG_BG_W, blend_y1)
    stage_h = RIG_BG_H - blend_y0
    stage = fit_cover(Image.open(stage_path), RIG_BG_W, stage_h)

    canvas = np.zeros((RIG_BG_H, RIG_BG_W, 3), dtype=np.float32)
    sky_arr = np.array(sky, dtype=np.float32)
    stage_arr = np.array(stage, dtype=np.float32)

    canvas[:blend_y0] = sky_arr[:blend_y0]
    canvas[blend_y1:] = stage_arr[blend_y1 - blend_y0:]
    span = max(1, blend_y1 - blend_y0)
    for y in range(blend_y0, blend_y1):
        t = (y - blend_y0) / float(span)
        # curva suave S-curve (smoothstep) para transición sin costura
        w = t * t * (3.0 - 2.0 * t)
        canvas[y] = sky_arr[y] * (1.0 - w) + stage_arr[y - blend_y0] * w

    out = Image.fromarray(np.clip(canvas, 0, 255).astype(np.uint8), "RGB")
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out.save(out_path, format="JPEG", quality=quality, optimize=True, progressive=True)
    print(f"[OK] Fondo vertical unido con smoothstep ({blend_y0}..{blend_y1}): {out_path} ({out_path.stat().st_size:,} B)")


def build_floor_bg(src_path: pathlib.Path, out_path: pathlib.Path) -> None:
    im = Image.open(src_path)
    out = fit_cover(im, FLOOR_BG_W, FLOOR_BG_H)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out.save(out_path, format="PNG", optimize=True)
    print(f"[OK] Fondo de pedalera generado: {out_path} ({out.size[0]}x{out.size[1]}, {out_path.stat().st_size:,} B)")


def export_templates(out_dir: pathlib.Path) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)

    # 1) Plantilla vertical 1550x1924
    tall = Image.new("RGB", (RIG_BG_W, RIG_BG_H), (28, 32, 38))
    dr = ImageDraw.Draw(tall)
    dr.rectangle([0, 0, RIG_BG_W, 768], fill=(38, 62, 92))
    dr.rectangle([0, 768, RIG_BG_W, 1156], fill=(48, 56, 66))
    dr.rectangle([0, 1156, RIG_BG_W, 1640], fill=(56, 52, 48))
    dr.rectangle([0, 1640, RIG_BG_W, 1924], fill=(74, 70, 66))

    dr.rectangle(INTRO_LOGO_BOX, outline=(120, 220, 255), width=4)
    dr.text((340, 205), "ZONA INTRO LOGO (intro_logo.png) - mantener cielo limpio y buen contraste", fill=(200, 245, 255))
    dr.line([(0, 768), (RIG_BG_W, 768)], fill=(255, 210, 80), width=3)
    dr.text((24, 738), "y=768 Fin viewport inicial (t=0)", fill=(255, 210, 80))
    dr.line([(0, 1156), (RIG_BG_W, 1156)], fill=(100, 240, 160), width=4)
    dr.text((24, 1126), "y=1156 Tope superior del escenario final (camY=1156 -> 1550x768)", fill=(100, 240, 160))
    dr.rectangle(CLOSEUP_CROP, outline=(255, 160, 90), width=3)
    dr.text((110, 1212), "CLOSE-UP VIEW (zoom 1.22x): evitar texto/detalles duros detras del amp", fill=(255, 190, 130))
    dr.line([(0, 1640), (RIG_BG_W, 1640)], fill=(220, 220, 220), width=2)
    dr.text((24, 1612), "y~1640 Horizonte del piso recomendado (profundidad detras del gabinete)", fill=(230, 230, 230))
    dr.line([(0, FLOOR_Y_CONSOLE_OPEN), (RIG_BG_W, FLOOR_Y_CONSOLE_OPEN)], fill=(255, 95, 95), width=3)
    dr.text((24, FLOOR_Y_CONSOLE_OPEN - 26), f"y={FLOOR_Y_CONSOLE_OPEN} Apoyo caja con consola abierta", fill=(255, 130, 130))
    dr.line([(0, FLOOR_Y_CONSOLE_CLOSED), (RIG_BG_W, FLOOR_Y_CONSOLE_CLOSED)], fill=(255, 60, 180), width=3)
    dr.text((24, FLOOR_Y_CONSOLE_CLOSED - 26), f"y={FLOOR_Y_CONSOLE_CLOSED} Apoyo caja con consola cerrada", fill=(255, 140, 210))
    tall_p = out_dir / "PLANTILLA_FONDO_RIG_1550x1924.png"
    tall.save(tall_p)

    # 2) Plantilla cenital de pedalera 1472x720
    fl = Image.new("RGB", (FLOOR_BG_W, FLOOR_BG_H), (52, 54, 58))
    df = ImageDraw.Draw(fl)
    bx = int(BOARD_NORM_RECT[0] * FLOOR_BG_W)
    by = int(BOARD_NORM_RECT[1] * FLOOR_BG_H)
    bw = int(BOARD_NORM_RECT[2] * FLOOR_BG_W)
    bh = int(BOARD_NORM_RECT[3] * FLOOR_BG_H)
    df.rectangle([bx, by, bx + bw, by + bh], outline=(110, 240, 160), width=4)
    df.text((bx + 16, by + 16), "ZONA PEDALBOARD (board_surface.png) - vista cenital 90°", fill=(160, 255, 190))
    df.rectangle([18, 24, int(FLOOR_BG_W * 0.34), FLOOR_BG_H - 90], outline=(140, 190, 255), width=3)
    df.text((30, 36), "PALETA IZQUIERDA DE PEDALES", fill=(170, 215, 255))
    fl_p = out_dir / "PLANTILLA_FONDO_PEDAL_1472x720.png"
    fl.save(fl_p)
    print(f"[OK] Plantillas exportadas en {out_dir}")


def check_existing(repo_root: pathlib.Path) -> int:
    rig_p = repo_root / "Resources" / "rig" / "skatepark_tall_capa0.jpg"
    fl_p  = repo_root / "Resources" / "floor_bg.png"
    ok = True
    for p, ew, eh in [(rig_p, RIG_BG_W, RIG_BG_H), (fl_p, FLOOR_BG_W, FLOOR_BG_H)]:
        if not p.exists():
            print(f"[FALTA] {p}")
            ok = False
            continue
        im = Image.open(p)
        w, h = im.size
        status = "OK" if (w == ew and h == eh) else f"REVISAR (esperado {ew}x{eh})"
        if w != ew or h != eh:
            ok = False
        print(f"[{status}] {p.relative_to(repo_root)} -> {w}x{h} {im.mode} ({p.stat().st_size:,} B)")
    return 0 if ok else 1


def main() -> int:
    parser = argparse.ArgumentParser(description="Referencia y procesador de fondos para MI AMP.")
    parser.add_argument("--check", action="store_true", help="Verifica los fondos actuales del repo.")
    parser.add_argument("--template", type=pathlib.Path, help="Genera PNGs plantilla con zonas seguras.")
    parser.add_argument("--build-rig", nargs=2, metavar=("INPUT", "OUTPUT"), help="Convierte imagen a 1550x1924 JPG.")
    parser.add_argument("--stitch", nargs=3, metavar=("SKY", "STAGE", "OUTPUT"), help="Une cielo + escenario en 1550x1924 JPG.")
    parser.add_argument("--build-floor", nargs=2, metavar=("INPUT", "OUTPUT"), help="Convierte piso cenital a 1472x720 PNG.")
    args = parser.parse_args()

    repo_root = pathlib.Path(__file__).resolve().parent.parent
    if args.check:
        return check_existing(repo_root)
    if args.template:
        export_templates(args.template)
        return 0
    if args.build_rig:
        build_rig_bg(pathlib.Path(args.build_rig[0]), pathlib.Path(args.build_rig[1]))
        return 0
    if args.stitch:
        stitch_rig_bg(pathlib.Path(args.stitch[0]), pathlib.Path(args.stitch[1]), pathlib.Path(args.stitch[2]))
        return 0
    if args.build_floor:
        build_floor_bg(pathlib.Path(args.build_floor[0]), pathlib.Path(args.build_floor[1]))
        return 0

    parser.print_help()
    return 0


if __name__ == "__main__":
    sys.exit(main())
