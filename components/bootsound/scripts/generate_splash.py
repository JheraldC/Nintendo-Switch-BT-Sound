"""
generate_splash.py - genera el splash provisional de FASE 5.0 (1280x720 PNG),
para insertar en una copia local de package3 con insert_splash_screen.py.

Diseno minimalista propio: sin logos de Nintendo ni de Atmosphere, sin contenido
protegido. Usa una fuente YA INSTALADA en el sistema (no se incrusta ninguna
fuente en el repositorio); si esa fuente no existe en otra maquina, el script
cae a la fuente por defecto de PIL (mismo layout, tipografia distinta).

Uso:
    python scripts/generate_splash.py [ruta_salida.png]
"""
import os
import sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

WIDTH, HEIGHT = 1280, 720

BG_TOP    = (8, 8, 12)        # casi negro, ligerisimo tinte azulado
BG_BOTTOM = (14, 11, 20)      # casi negro, ligerisimo tinte violeta hacia abajo
ACCENT    = (138, 92, 246)    # violeta discreto (acento)
TITLE_COLOR    = (235, 235, 240)
SUBTITLE_COLOR = (150, 140, 170)

FONT_CANDIDATES_TITLE = [
    r"C:\Windows\Fonts\seguisb.ttf",   # Segoe UI Semibold
    r"C:\Windows\Fonts\segoeuib.ttf",  # Segoe UI Bold
    r"C:\Windows\Fonts\arialbd.ttf",
]
FONT_CANDIDATES_SUB = [
    r"C:\Windows\Fonts\segoeuil.ttf",  # Segoe UI Light
    r"C:\Windows\Fonts\segoeui.ttf",
    r"C:\Windows\Fonts\arial.ttf",
]


def _load_font(candidates, size):
    for path in candidates:
        if os.path.isfile(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def _vertical_gradient(w, h, top, bottom):
    img = Image.new("RGB", (w, h), top)
    px = img.load()
    for y in range(h):
        t = y / (h - 1)
        r = round(top[0] + (bottom[0] - top[0]) * t)
        g = round(top[1] + (bottom[1] - top[1]) * t)
        b = round(top[2] + (bottom[2] - top[2]) * t)
        for x in range(w):
            px[x, y] = (r, g, b)
    return img


def _letter_spaced(text, spaces=1):
    sep = " " * spaces
    return sep.join(list(text))


def build_splash():
    img = _vertical_gradient(WIDTH, HEIGHT, BG_TOP, BG_BOTTOM).convert("RGBA")
    draw = ImageDraw.Draw(img, "RGBA")

    # acento: resplandor violeta MUY discreto detras del texto (radial simple)
    glow = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
    gdraw = ImageDraw.Draw(glow)
    cx, cy = WIDTH // 2, HEIGHT // 2 - 10
    max_r = 340
    for r in range(max_r, 0, -4):
        alpha = int(10 * (1 - r / max_r))  # muy tenue, pico ~10/255
        if alpha <= 0:
            continue
        gdraw.ellipse(
            (cx - r, cy - r, cx + r, cy + r),
            fill=(ACCENT[0], ACCENT[1], ACCENT[2], alpha),
        )
    img = Image.alpha_composite(img, glow)
    draw = ImageDraw.Draw(img, "RGBA")

    title_font = _load_font(FONT_CANDIDATES_TITLE, 92)
    sub_font = _load_font(FONT_CANDIDATES_SUB, 30)

    title = "BootSound"
    subtitle = _letter_spaced("CUSTOM BOOT AUDIO", spaces=1)

    tb = draw.textbbox((0, 0), title, font=title_font)
    tw, th = tb[2] - tb[0], tb[3] - tb[1]
    title_x = (WIDTH - tw) / 2 - tb[0]
    title_y = HEIGHT / 2 - th / 2 - tb[1] - 26

    draw.text((title_x, title_y), title, font=title_font, fill=TITLE_COLOR)

    # linea acento violeta, fina y corta, centrada bajo el titulo
    line_y = title_y + th + 34
    line_half_w = 46
    draw.line(
        (WIDTH / 2 - line_half_w, line_y, WIDTH / 2 + line_half_w, line_y),
        fill=(ACCENT[0], ACCENT[1], ACCENT[2], 220),
        width=3,
    )

    sb = draw.textbbox((0, 0), subtitle, font=sub_font)
    sw, sh = sb[2] - sb[0], sb[3] - sb[1]
    sub_x = (WIDTH - sw) / 2 - sb[0]
    sub_y = line_y + 22

    draw.text((sub_x, sub_y), subtitle, font=sub_font, fill=SUBTITLE_COLOR)

    return img.convert("RGB")


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else str(
        Path(__file__).resolve().parents[2] / 'bootsplash/images/BootSound_splash.png'
    )
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    img = build_splash()
    assert img.size == (WIDTH, HEIGHT)
    img.save(out_path, "PNG")
    print("splash generado:", out_path, img.size, img.mode)


if __name__ == "__main__":
    main()
