"""
make_hekate_bootlogo.py - convierte una imagen 1280x720 (16:9) en un bootlogo
32-bit BMP de 720x1280 compatible con el logopath= de Hekate (CTCaer mod).

Formato verificado directamente contra el codigo fuente de Hekate
(bootloader/main.c, funcion _auto_launch; bootloader/gfx/gfx.c,
gfx_render_bmp_argb), no asumido:

  - cabecera BMP clasica: BITMAPFILEHEADER (14 bytes) + BITMAPINFOHEADER de
    40 bytes (offset 0='B', offset 1='M', offset 10=bfOffBits, offset 18=
    biWidth, offset 22=biHeight, offset 28=biBitCount).
  - Hekate EXIGE biBitCount == 32 (offset 28) -- un BMP de 24 bits es
    rechazado. size_x debe ser <= 720 y size_y <= 1280 (formato retrato).
  - biHeight debe ser POSITIVO (bottom-up, el orden estandar de BMP): Hekate
    lee ese campo como si fuera positivo para la comprobacion <=1280; un
    biHeight negativo (top-down) produciria un numero absurdo y fallaria la
    validacion.
  - gfx_render_bmp_argb() indexa las filas como
    buf[(size_y - 1 - fila_relativa) * size_x + col] -- eso es exactamente
    la semantica de un BMP bottom-up estandar, asÃ­ que NO hace falta invertir
    filas manualmente: un encoder de BMP normal (bottom-up) ya queda
    orientado correctamente en pantalla.
  - Cada pixel se copia tal cual (u32) al framebuffer sin blending visible en
    el codigo -- se escribe alpha=0xFF (opaco) en cada pixel por seguridad.
  - Orden de bytes por pixel: B, G, R, A (poco a mucho, igual que el BMP de
    32 bits clasico de Windows y que utilities/insert_splash_screen.py de
    Atmosphere para su propio formato).

La consola muestra el panel en orientacion retrato internamente (igual que
el splash de package3 de FASE 5.0): una fuente horizontal 1280x720 hay que
rotarla 90 grados antihorario (Image.ROTATE_90, la MISMA convencion que usa
insert_splash_screen.py) para obtener el resultado final de 720x1280.

Uso:
    python make_hekate_bootlogo.py input.png output.bmp [--resize]

    --resize   si input.png es 16:9 pero no mide exactamente 1280x720, la reescala antes
               de rotar (por defecto: sin --resize, un tamano distinto de
               1280x720 hace que el script se detenga con error, para no
               deformar una imagen sin que el usuario lo pida).

No escribe nada en ninguna SD -- solo genera el archivo BMP local indicado.
"""
import argparse
import os
import struct
import sys

from PIL import Image

SRC_W, SRC_H = 1280, 720
OUT_W, OUT_H = 720, 1280   # tras rotar 90 grados antihorario

BMP_FILE_HEADER_SIZE = 14
BMP_INFO_HEADER_SIZE = 40
BMP_HEADER_SIZE = BMP_FILE_HEADER_SIZE + BMP_INFO_HEADER_SIZE  # 54, == bfOffBits


def _is_16_9(w, h, tol=0.01):
    return abs((w / h) - (16 / 9)) <= tol


def load_and_orient(input_png, allow_resize):
    img = Image.open(input_png)
    if img.format != 'PNG':
        raise ValueError(f"se esperaba PNG, formato real: {img.format}")

    w, h = img.size
    if (w, h) != (SRC_W, SRC_H):
        if not _is_16_9(w, h):
            raise ValueError(f"la imagen no es 16:9 ({w}x{h}); no se deforma ni con --resize")
        if not allow_resize:
            raise ValueError(
                f"la imagen mide {w}x{h}, se esperaba exactamente {SRC_W}x{SRC_H}; "
                f"pasa --resize si quieres que se reescale (LANCZOS) antes de rotar"
            )
        img = img.resize((SRC_W, SRC_H), Image.LANCZOS)

    rotated = img.transpose(Image.ROTATE_90)  # antihorario, igual que Atmosphere
    if rotated.size != (OUT_W, OUT_H):
        raise AssertionError(f"rotacion produjo {rotated.size}, se esperaba {(OUT_W, OUT_H)}")

    return rotated.convert('RGBA')


def write_bmp32(img_rgba, out_path):
    w, h = img_rgba.size
    assert (w, h) == (OUT_W, OUT_H)

    row_bytes = w * 4                    # 32bpp -> ya alineado a 4 bytes, sin padding
    pixel_data_size = row_bytes * h
    file_size = BMP_HEADER_SIZE + pixel_data_size

    file_header = struct.pack(
        '<2sIHHI',
        b'BM',            # bfType
        file_size,        # bfSize
        0, 0,             # bfReserved1, bfReserved2
        BMP_HEADER_SIZE,  # bfOffBits
    )
    info_header = struct.pack(
        '<IiiHHIIiiII',
        BMP_INFO_HEADER_SIZE,  # biSize
        w,                      # biWidth
        h,                      # biHeight (POSITIVO = bottom-up, exigido por Hekate)
        1,                       # biPlanes
        32,                      # biBitCount -- Hekate exige exactamente 32
        0,                       # biCompression = BI_RGB
        pixel_data_size,         # biSizeImage
        0, 0,                    # biXPelsPerMeter, biYPelsPerMeter
        0, 0,                    # biClrUsed, biClrImportant
    )

    px = img_rgba.load()
    rows = bytearray(pixel_data_size)
    # bottom-up: la PRIMERA fila del archivo es la fila INFERIOR de la imagen
    for out_row, y in enumerate(range(h - 1, -1, -1)):
        base = out_row * row_bytes
        for x in range(w):
            r, g, b, _a = px[x, y]
            o = base + x * 4
            rows[o + 0] = b
            rows[o + 1] = g
            rows[o + 2] = r
            rows[o + 3] = 0xFF   # opaco siempre

    with open(out_path, 'wb') as f:
        f.write(file_header)
        f.write(info_header)
        f.write(rows)


def validate_bmp(path):
    with open(path, 'rb') as f:
        data = f.read()
    assert data[0:2] == b'BM', "firma BM ausente"
    bf_off_bits = struct.unpack_from('<I', data, 10)[0]
    size_x = struct.unpack_from('<i', data, 18)[0]
    size_y = struct.unpack_from('<i', data, 22)[0]
    bpp = struct.unpack_from('<H', data, 28)[0]
    assert bpp == 32, f"biBitCount={bpp}, Hekate exige 32"
    assert size_x == OUT_W and size_y == OUT_H, f"dimensiones {size_x}x{size_y}"
    assert size_y > 0, "biHeight debe ser positivo (bottom-up)"
    assert size_x <= 720 and size_y <= 1280, "excede el maximo permitido por Hekate"
    assert bf_off_bits == BMP_HEADER_SIZE
    assert len(data) == bf_off_bits + size_x * size_y * 4, "tamano de datos de pixel inesperado"
    return {
        'signature': data[0:2].decode('ascii'),
        'width': size_x,
        'height': size_y,
        'bits_per_pixel': bpp,
        'file_size': len(data),
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('input_png')
    ap.add_argument('output_bmp')
    ap.add_argument('--resize', action='store_true',
                     help='reescalar a 1280x720 si la entrada no mide exactamente eso')
    args = ap.parse_args()

    img = load_and_orient(args.input_png, args.resize)
    os.makedirs(os.path.dirname(os.path.abspath(args.output_bmp)) or '.', exist_ok=True)
    write_bmp32(img, args.output_bmp)
    info = validate_bmp(args.output_bmp)
    print('bootlogo BMP generado:', args.output_bmp)
    print(' ', info)


if __name__ == '__main__':
    sys.exit(main())
