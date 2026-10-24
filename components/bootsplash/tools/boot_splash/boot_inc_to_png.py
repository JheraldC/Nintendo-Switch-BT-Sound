"""
boot_inc_to_png.py - operacion INVERSA de png_to_boot_inc.py: lee un .inc con
el formato de stratosphere/boot (SplashScreenX/Y/W/H/SplashScreen[]) y
reconstruye un PNG, para verificar visualmente y por pixel que la conversion
de ida fue correcta (pixel-exacta) antes de sustituir el .inc real.

No necesita compilar nada: parsea el texto C++ directamente (son constantes
literales simples, no macros ni expresiones).

Uso:
    python boot_inc_to_png.py input.inc output_preview.png
"""
import re
import sys

from PIL import Image

RE_SIZE = re.compile(r'constexpr\s+size_t\s+SplashScreen([XYWH])\s*=\s*(\d+)\s*;')
RE_ARRAY = re.compile(r'constexpr\s+u32\s+SplashScreen\[\]\s*=\s*\{(.*?)\};', re.DOTALL)
RE_HEX = re.compile(r'0[xX][0-9a-fA-F]+')


def parse_inc(path):
    text = open(path, 'r').read()

    dims = {}
    for m in RE_SIZE.finditer(text):
        dims[m.group(1)] = int(m.group(2))
    for key in ('X', 'Y', 'W', 'H'):
        if key not in dims:
            raise ValueError(f"no se encontro SplashScreen{key} en {path}")

    m = RE_ARRAY.search(text)
    if not m:
        raise ValueError(f"no se encontro el array SplashScreen[] en {path}")
    values = [int(h, 16) for h in RE_HEX.findall(m.group(1))]

    w, h = dims['W'], dims['H']
    if len(values) != w * h:
        raise ValueError(f"el array tiene {len(values)} elementos, se esperaban {w*h} ({w}x{h})")

    return dims, values


def build_image(dims, values):
    w, h = dims['W'], dims['H']
    img = Image.new('RGBA', (w, h))
    px = img.load()
    for row in range(h):
        for col in range(w):
            v = values[row * w + col]
            a = (v >> 24) & 0xFF
            r = (v >> 16) & 0xFF
            g = (v >> 8) & 0xFF
            b = v & 0xFF
            px[col, row] = (r, g, b, a)
    return img


def main():
    if len(sys.argv) != 3:
        print('Uso: python boot_inc_to_png.py input.inc output_preview.png')
        return 1

    dims, values = parse_inc(sys.argv[1])
    img = build_image(dims, values)
    img.convert('RGB').save(sys.argv[2], 'PNG')

    print('preview generado:', sys.argv[2])
    print(f"  X={dims['X']} Y={dims['Y']} W={dims['W']} H={dims['H']}")
    return 0


if __name__ == '__main__':
    sys.exit(main())
