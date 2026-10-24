"""
package3_format.py - lectura/escritura del formato PK31 (package3) de Atmosphere,
derivado directamente de fusee/build_package3.py (Atmosphere 1.11.2, commit
5388824be146a89619e8d641acd64599cf1c5f62) -- NO es un formato inventado.

Layout verificado leyendo build_package3.py:

  0x000 magic 'PK31' (4)
  0x004 metadata_offset u32 (=0x20)
  0x008 flags u32
  0x00C meso_size u32
  0x010 num_kips u32 (=7)
  0x014 reserved1 (12 bytes 0xCC)
  0x020 legacy magic 'FSS0' (4)
  0x024 total_size u32 (=0x800000)
  0x028 reserved2 u32
  0x02C content_header_offset u32 (=0x40)
  0x030 num_content_headers u32 (=15 = 8 fijos + 7 kips)
  0x034 supported_hos_version (4 bytes)
  0x038 release_version (4 bytes)
  0x03C git_revision u32
  0x040 content metas: num_content_headers x 0x20 bytes
        cada uno: <IIBBBBI16s> = offset, size, type, _, _, _, reserved(0xCCCCCCCC), name
  0x400 kip metas: (1 + 7) x 0x30 bytes (emummc primero, luego los 7 KIP_NAMES en orden)
        cada uno: <8sII32s> = program_id(8, del propio kip[0x10:0x18]),
                  relative_offset(=offset-0x100000), size, sha256(32)
  0x800 inicio de los datos reales (warmboot, tsec_keygen, mariko_fatal, mtc, exosphere,
        mesosphere, region KIP [0x100000:0x400000), early splash [0x400000:0x7C0000),
        fusee+rebootstub+padding [0x7C0000:0x800000))

KIP_NAMES, en el orden exacto que usa build_package3.py (orden de escritura Y de las
kip metas):
"""
import hashlib
import struct

MAGIC = b'PK31'
TOTAL_SIZE = 0x800000

CONTENT_HEADER_OFFSET = 0x40
CONTENT_META_SIZE = 0x20
KIP_META_BASE_OFFSET = 0x400
KIP_META_SIZE = 0x30

KIP_REGION_START = 0x100000
KIP_REGION_END = 0x400000
KIP_REGION_BUDGET = KIP_REGION_END - KIP_REGION_START  # 0x300000

# Orden EXACTO de fusee/build_package3.py: KIP_NAMES = [...]; emummc siempre primero.
KIP_NAMES = ['Loader', 'NCM', 'ProcessManager', 'sm', 'boot', 'spl', 'ams_mitm']
KIP_REGION_ORDER = ['emummc'] + KIP_NAMES   # orden real de escritura en el archivo

CNT_TYPE_KIP = 6  # confirmado leyendo bootloader/hos/pkg3.c (Hekate) en FASE 5.6


def align16(n: int) -> int:
    return (n + 0xF) & ~0xF


def read_content_metas(data: bytes):
    """Devuelve lista de dicts: {name, offset, size, type, index} en orden de aparicion."""
    num = struct.unpack_from('<I', data, 0x30)[0]
    metas = []
    for i in range(num):
        off = CONTENT_HEADER_OFFSET + i * CONTENT_META_SIZE
        offset, size, ctype, a, b, c, resv, name = struct.unpack_from('<IIBBBBI16s', data, off)
        name = name.rstrip(b'\x00').decode('ascii', errors='replace')
        metas.append({'index': i, 'name': name, 'offset': offset, 'size': size,
                      'type': ctype, 'a': a, 'b': b, 'c': c, 'reserved': resv})
    return metas


def read_kip_metas(data: bytes):
    """Devuelve lista de dicts: {name(inferido por orden), program_id, rel_offset, size, sha256}."""
    metas = []
    for i, name in enumerate(KIP_REGION_ORDER):
        off = KIP_META_BASE_OFFSET + i * KIP_META_SIZE
        program_id, rel_offset, size, sha = struct.unpack_from('<8sII32s', data, off)
        metas.append({'index': i, 'name': name, 'program_id': program_id,
                      'rel_offset': rel_offset, 'size': size, 'sha256': sha})
    return metas


def pack_content_meta(offset, size, ctype, a, b, c, reserved, name: str) -> bytes:
    return struct.pack('<IIBBBBI16s', offset, size, ctype, a, b, c, reserved,
                        name.encode('ascii'))


def pack_kip_meta(program_id: bytes, rel_offset: int, size: int, sha256: bytes) -> bytes:
    assert len(program_id) == 8
    assert len(sha256) == 32
    return struct.pack('<8sII32s', program_id, rel_offset, size, sha256)


def extract_component(data: bytes, meta: dict) -> bytes:
    return data[meta['offset']:meta['offset'] + meta['size']]


def kip_program_id(kip_bytes: bytes) -> bytes:
    assert kip_bytes[:4] == b'KIP1', f"magic KIP1 esperado, encontrado {kip_bytes[:4]!r}"
    return kip_bytes[0x10:0x18]
