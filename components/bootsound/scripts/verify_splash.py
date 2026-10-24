"""
verify_splash.py - extrae el splash insertado en un package3 (rango
0x400000:0x7C0000) y reconstruye un PNG 1280x720 para verificacion visual.
Es la operacion INVERSA de vendor/atmosphere-1.11.2/utilities/insert_splash_screen.py
(mismo layout: filas de 1280 en el eje "row", 720 en el eje "col", stride de
768 columnas, BGRA por pixel, imagen interna en orientacion 720x1280 que se
gira 90 grados hacia la orientacion final 1280x720).

No modifica package3: solo lee.

Uso:
    python scripts/verify_splash.py package3 salida_preview.png
"""
import sys
from struct import unpack as up
from PIL import Image

SPLASH_SCREEN_WIDTH  = 1280
SPLASH_SCREEN_HEIGHT = 720
SPLASH_SCREEN_STRIDE = 768

SPLASH_OFFSET = 0x400000
SPLASH_SIZE   = 0x3C0000  # SPLASH_SCREEN_WIDTH * SPLASH_SCREEN_STRIDE * 4


def extract_preview(package3_path):
    with open(package3_path, 'rb') as f:
        data = f.read()
    assert len(data) == 0x800000, "tamano de package3 inesperado"
    blob = data[SPLASH_OFFSET:SPLASH_OFFSET + SPLASH_SIZE]
    assert len(blob) == SPLASH_SIZE, "rango del splash incompleto"

    # reconstruir la imagen interna (720 ancho x 1280 alto), BGRA por pixel,
    # cada fila de "row" tiene SPLASH_SCREEN_STRIDE columnas pero solo las
    # primeras SPLASH_SCREEN_HEIGHT (720) son pixeles reales; el resto es
    # padding de ceros que se descarta.
    rotated = Image.new('RGBA', (SPLASH_SCREEN_HEIGHT, SPLASH_SCREEN_WIDTH))
    px = rotated.load()
    row_bytes = SPLASH_SCREEN_STRIDE * 4
    for row in range(SPLASH_SCREEN_WIDTH):
        base = row * row_bytes
        for col in range(SPLASH_SCREEN_HEIGHT):
            off = base + col * 4
            b, g, r, a = up('<BBBB', blob[off:off + 4])
            px[col, row] = (r, g, b, a)

    # deshacer el transpose(ROTATE_90) del script oficial: ROTATE_90 es
    # 90 grados antihorario, así que el inverso es ROTATE_270.
    preview = rotated.transpose(Image.ROTATE_270)
    assert preview.size == (SPLASH_SCREEN_WIDTH, SPLASH_SCREEN_HEIGHT)
    return preview


def main():
    if len(sys.argv) != 3:
        print('Uso: python scripts/verify_splash.py package3 salida_preview.png')
        return 1
    preview = extract_preview(sys.argv[1])
    preview.convert('RGB').save(sys.argv[2], 'PNG')
    print('preview extraido:', sys.argv[2], preview.size, preview.mode)
    return 0


if __name__ == '__main__':
    sys.exit(main())
