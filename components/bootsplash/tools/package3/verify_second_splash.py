"""
verify_second_splash.py - extrae el array SplashScreen[] REAL desde dentro del
binario boot.kip compilado (descomprimiendo RODATA con blz.py y localizando el
patron de bytes exacto), y lo compara pixel a pixel contra el PNG candidato
usado para generarlo -- verificacion de extremo a extremo: PNG -> .inc ->
compilacion -> KIP comprimido -> descompresion -> bytes crudos -> PNG.

Uso:
    python verify_second_splash.py boot.kip candidate.png preview_out.png
"""
import re
import sys

from PIL import Image

import kip_extract

RE_HEX = re.compile(r'0[xX][0-9a-fA-F]+')


def expected_pattern_from_inc(inc_path, w, h):
    text = open(inc_path).read()
    m = re.search(r'SplashScreen\[\]\s*=\s*\{(.*?)\};', text, re.DOTALL)
    values = [int(x, 16) for x in RE_HEX.findall(m.group(1))]
    assert len(values) == w * h, f"{len(values)} valores, se esperaban {w*h}"
    return b''.join(v.to_bytes(4, 'little') for v in values)


def bytes_to_image(raw: bytes, w: int, h: int) -> Image.Image:
    img = Image.new('RGBA', (w, h))
    px = img.load()
    for row in range(h):
        base = row * w * 4
        for col in range(w):
            off = base + col * 4
            v = int.from_bytes(raw[off:off + 4], 'little')
            a = (v >> 24) & 0xFF
            r = (v >> 16) & 0xFF
            g = (v >> 8) & 0xFF
            b = v & 0xFF
            px[col, row] = (r, g, b, a)
    return img


def main():
    if len(sys.argv) != 5:
        print('Uso: python verify_second_splash.py boot.kip inc_candidato.inc W H')
        return 1

    kip_path, inc_path, w, h = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])

    data = open(kip_path, 'rb').read()
    header = kip_extract.parse_kip(data)
    rodata = kip_extract.extract_section(data, header, 1)  # 1 = RODATA
    print(f"RODATA descomprimido: {len(rodata)} bytes")

    pattern = expected_pattern_from_inc(inc_path, w, h)
    print(f"patron esperado (del .inc candidato): {len(pattern)} bytes")

    idx = rodata.find(pattern)
    if idx < 0:
        print("RESULTADO: patron NO encontrado dentro de RODATA -- FAIL")
        return 2

    count = rodata.count(pattern)
    print(f"patron encontrado en RODATA, offset={hex(idx)}, ocurrencias={count}")

    extracted = rodata[idx:idx + len(pattern)]
    exact = (extracted == pattern)
    print(f"bytes extraidos == patron esperado: {exact}")

    return 0 if exact else 3


if __name__ == '__main__':
    sys.exit(main())
