"""
kip_extract.py - extrae los segmentos (TEXT/RODATA/DATA) descomprimidos de un
KIP1, usando blz.py para descomprimir cuando el flag correspondiente esta
activo. Formato de cabecera KIP1 verificado empiricamente contra el propio
boot.kip compilado y contra emummc/tools/kip1converter.py de Atmosphere
(mismos offsets: header 0x20, 6 section headers de 0x10 c/u hasta 0x80,
32 capabilities de 4 bytes hasta 0x100, datos desde 0x100).
"""
import struct

from blz import kip1_blz_decompress

SECTION_NAMES = ['TEXT', 'RODATA', 'DATA', 'BSS', 'UNUSED4', 'UNUSED5']
DATA_START = 0x100


def parse_kip(data: bytes):
    assert data[:4] == b'KIP1', f"magic KIP1 esperado, encontrado {data[:4]!r}"
    flags = data[0x1F]
    sections = []
    for i in range(6):
        off = 0x20 + i * 16
        out_offset, out_size, compressed_size, attribute = struct.unpack_from('<IIII', data, off)
        sections.append({
            'name': SECTION_NAMES[i],
            'out_offset': out_offset,
            'out_size': out_size,
            'compressed_size': compressed_size,
            'attribute': attribute,
            'is_compressed': bool(flags & (1 << i)) if i < 3 else False,
        })
    return {'flags': flags, 'sections': sections}


def extract_section(data: bytes, header: dict, index: int) -> bytes:
    """Devuelve los bytes DESCOMPRIMIDOS de la seccion 'index' (0=TEXT,1=RODATA,2=DATA)."""
    sec = header['sections'][index]
    if sec['name'] == 'BSS':
        raise ValueError("BSS no tiene datos en el archivo")

    # las 3 primeras secciones se almacenan CONSECUTIVAMENTE desde DATA_START,
    # cada una ocupando su compressed_size (si esta comprimida) o out_size (si no).
    pos = DATA_START
    for i in range(3):
        s = header['sections'][i]
        stored_size = s['compressed_size'] if s['is_compressed'] else s['out_size']
        if i == index:
            raw = data[pos:pos + stored_size]
            if s['is_compressed']:
                decompressed = kip1_blz_decompress(raw)
                assert len(decompressed) == s['out_size'], (
                    f"{s['name']}: descomprimido {len(decompressed)} bytes, "
                    f"esperado {s['out_size']}")
                return decompressed
            else:
                assert len(raw) == s['out_size']
                return raw
        pos += stored_size
    raise IndexError(index)
