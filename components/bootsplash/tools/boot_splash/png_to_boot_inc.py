"""
png_to_boot_inc.py - convierte un PNG en el .inc que espera el sysmodule "boot"
de Atmosphere (stratosphere/boot/source/boot_splash_screen_notext.inc), el
SEGUNDO splash (~2 s, ShowSplashScreen()/TimeSpan::FromSeconds(2)) -- DISTINTO
del splash temprano de package3 (FASE 5.0/5.1) y del bootlogo de Hekate
(FASE 5.3), cada uno con su propio formato.

Formato derivado LEYENDO el codigo fuente real de Atmosphere 1.11.2
(commit 5388824be146a89619e8d641acd64599cf1c5f62), no asumido de terceros:

  - stratosphere/boot/source/boot_display.cpp, ShowDisplay(x, y, width, height,
    const u32 *img): recorre el framebuffer fisico (768x1280, retrato) con
        g_frame_buffer[(FrameBufferHeight - (x + cur_x)) * FrameBufferWidth + y + cur_y]
            = img[cur_y * width + cur_x]
    Esto significa que 'img' es un array NORMAL, fila por fila, de 'width'
    columnas x 'height' filas -- SIN rotar -- la propia ShowDisplay() hace la
    transformacion a la orientacion fisica del panel. A diferencia del splash
    de package3 (insert_splash_screen.py) y del bootlogo de Hekate, que SI
    exigen rotar la imagen 90 grados antes de empaquetarla, este formato NO se
    rota.
  - stratosphere/boot/source/boot_display_config.inc, linea DC_WIN_COLOR_DEPTH:
        WIN_COLOR_DEPTH_B8G8R8A8  (comentario propio: "T_A8R8G8B8")
    Confirma el orden de bytes en memoria: B, G, R, A (poco a mucho) -- el
    mismo que un literal hexadecimal 0xAARRGGBB para un u32 little-endian.
    Es la MISMA convencion de bytes que insert_splash_screen.py y el bootlogo
    de Hekate (aunque la orientacion sea distinta).
  - stratosphere/boot/source/boot_splash_screen.cpp: la posicion se pasa en
    coordenadas de pantalla LOGICA 1280x720 (X, Y son las mismas que en la
    formula de centrado X=(1280-W)/2, Y=(720-H)/2 -- verificado con los
    valores originales SplashScreenX=535/Y=274/W=210/H=172, que cumplen
    exactamente esa formula).

Uso:
    python png_to_boot_inc.py input.png output.inc [--alpha 255]

Genera:
    constexpr size_t SplashScreenX = ...;
    constexpr size_t SplashScreenY = ...;
    constexpr size_t SplashScreenW = ...;
    constexpr size_t SplashScreenH = ...;
    constexpr u32 SplashScreen[] = {0xAARRGGBB, ...};

No escribe nada en ninguna SD -- solo genera el .inc local indicado. No es un
build tool: no compila nada, solo prepara el archivo fuente a sustituir.
"""
import argparse
import os
import sys

from PIL import Image

SCREEN_W, SCREEN_H = 1280, 720


def load_image(input_png):
    img = Image.open(input_png)
    if img.format != 'PNG':
        raise ValueError(f"se esperaba PNG, formato real: {img.format}")
    return img.convert('RGBA')


def generate_inc(img, alpha_override=None):
    w, h = img.size
    if not (1 <= w <= SCREEN_W and 1 <= h <= SCREEN_H):
        raise ValueError("La imagen debe medir entre 1x1 y 1280x720")
    if alpha_override is not None and not (0 <= alpha_override <= 255):
        raise ValueError("El alfa debe estar entre 0 y 255")
    x = (SCREEN_W - w) // 2
    y = (SCREEN_H - h) // 2
    px = img.load()

    values = []
    for row in range(h):
        for col in range(w):
            r, g, b, a = px[col, row]
            if alpha_override is not None:
                a = alpha_override
            values.append(f"0x{a:02X}{r:02X}{g:02X}{b:02X}")

    lines = []
    lines.append("/* Generado por tools/boot_splash/png_to_boot_inc.py -- NO es upstream. */")
    lines.append(f"constexpr size_t SplashScreenX = {x};")
    lines.append(f"constexpr size_t SplashScreenY = {y};")
    lines.append(f"constexpr size_t SplashScreenW = {w};")
    lines.append(f"constexpr size_t SplashScreenH = {h};")
    lines.append("")
    # una fila por linea, para que el diff/lectura sea manejable
    lines.append("constexpr u32 SplashScreen[] = {")
    for row in range(h):
        row_vals = values[row * w:(row + 1) * w]
        lines.append("    " + ", ".join(row_vals) + ",")
    lines.append("};")
    lines.append("")

    return "\n".join(lines), (x, y, w, h)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('input_png')
    ap.add_argument('output_inc')
    ap.add_argument('--alpha', type=int, default=255,
                     help='valor de alfa fijo a escribir para cada pixel (por defecto 255, opaco)')
    args = ap.parse_args()

    img = load_image(args.input_png)
    text, (x, y, w, h) = generate_inc(img, alpha_override=args.alpha)

    os.makedirs(os.path.dirname(os.path.abspath(args.output_inc)) or '.', exist_ok=True)
    with open(args.output_inc, 'w', newline='\n') as f:
        f.write(text)

    raw_bytes = w * h * 4
    print('boot .inc generado:', args.output_inc)
    print(f'  X={x} Y={y} W={w} H={h}  (centrado sobre {SCREEN_W}x{SCREEN_H})')
    print(f'  sizeof(SplashScreen) = {raw_bytes} bytes ({raw_bytes/1024:.1f} KiB)')


if __name__ == '__main__':
    sys.exit(main())
