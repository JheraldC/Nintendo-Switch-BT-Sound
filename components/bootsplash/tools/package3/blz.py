"""
blz.py - descompresor BLZ (Bottom/Backward LZ) para segmentos de KIP1.

La funcion kip1_blz_decompress() de este archivo es una traduccion literal
(misma estructura, mismos nombres de variable, misma aritmetica de indices,
solo portada de Python 2 a Python 3) de la funcion del mismo nombre en
utilities/nxo64.py, distribuido dentro del repositorio de Atmosphere
(Atmosphere-NX/Atmosphere, commit 5388824be146a89619e8d641acd64599cf1c5f62)
pero con su PROPIA licencia declarada en su propia cabecera, distinta del
resto del repositorio (que es GPLv2):

    Copyright 2017 Reswitched Team

    Permission to use, copy, modify, and/or distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

(licencia ISC, compatible con la licencia MIT del resto de este repositorio;
el aviso de copyright y esta nota de permiso se conservan intactos arriba,
tal como exige esa licencia, en vez de reetiquetar el codigo como propio).

Se eligio esta fuente en vez del codigo de referencia de switch-tools
(src/blz.c, funcion BLZ_Decode) porque ese ultimo esta completamente
COMENTADO y nunca se invoca en produccion (verificado con grep: cero
llamadas reales en todo el repositorio) -- su footer resulto inconsistente
al probarlo empiricamente. kip1_blz_decompress(), en cambio, es la funcion
que SI usa esta utilidad de Atmosphere para leer KIPs reales, y se valido
descomprimiendo los 3 segmentos de un KIP real a exactamente sus tamanos
declarados en la cabecera antes de confiar en ella.
"""
import struct


def kip1_blz_decompress(compressed: bytes) -> bytes:
    compressed_size, init_index, uncompressed_addl_size = struct.unpack('<III', compressed[-0xC:])

    decompressed = bytearray(compressed) + bytearray(uncompressed_addl_size)
    decompressed_size = len(decompressed)

    if not (compressed_size + uncompressed_addl_size):
        return b''

    cmp_start = len(compressed) - compressed_size
    cmp_ofs = compressed_size - init_index
    out_ofs = compressed_size + uncompressed_addl_size

    while out_ofs > 0:
        cmp_ofs -= 1
        control = decompressed[cmp_start + cmp_ofs]
        for _ in range(8):
            if control & 0x80:
                if cmp_ofs < 2 - cmp_start:
                    raise ValueError('Compression out of bounds!')
                cmp_ofs -= 2
                segmentoffset = decompressed[cmp_start + cmp_ofs] | (decompressed[cmp_start + cmp_ofs + 1] << 8)
                segmentsize = ((segmentoffset >> 12) & 0xF) + 3
                segmentoffset &= 0x0FFF
                segmentoffset += 2
                if out_ofs < segmentsize - cmp_start:
                    raise ValueError('Compression out of bounds!')
                for _ in range(segmentsize):
                    if out_ofs + segmentoffset >= decompressed_size:
                        raise ValueError('Compression out of bounds!')
                    data = decompressed[cmp_start + out_ofs + segmentoffset]
                    out_ofs -= 1
                    decompressed[cmp_start + out_ofs] = data
            else:
                if out_ofs < 1 - cmp_start:
                    raise ValueError('Compression out of bounds!')
                out_ofs -= 1
                cmp_ofs -= 1
                decompressed[cmp_start + out_ofs] = decompressed[cmp_start + cmp_ofs]
            control = (control << 1) & 0xFF
            if not out_ofs:
                break

    return bytes(decompressed)
