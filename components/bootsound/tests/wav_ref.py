"""
wav_ref.py - port 1:1 en Python de source/wav.c + source/sdfile.c (parte de parseo
y reserva). Sirve para probar la LOGICA del parser en PC sin Switch ni compilador C.

Mantener SINCRONIZADO con source/wav.c / source/sdfile.c: mismos limites, mismas
comprobaciones, mismo orden, mismos codigos de estado.
"""
import os
import struct

# ---- constantes (== wav.h) ----
FORMAT_PCM      = 1
SAMPLE_RATE     = 48000
CHANNELS        = 2
BITS_PER_SAMPLE = 16
BLOCK_ALIGN     = 4
BYTE_RATE       = 192000
MAX_PCM_SIZE    = 2 * 1024 * 1024      # 2 MiB (FASE 4.0; era 1 MiB hasta FASE 3.3)
PCM_ALIGN       = 0x1000
SDFILE_MAX_FILE = MAX_PCM_SIZE + 0x10000

# ---- estados (== jbs_status_t) ----
STATUS = [
    "OK", "SD_MOUNT_FAILED", "FILE_NOT_FOUND", "READ_FAILED",
    "INVALID_RIFF", "INVALID_WAVE", "FMT_NOT_FOUND", "DATA_NOT_FOUND",
    "UNSUPPORTED_FORMAT", "INVALID_BLOCK_ALIGN", "INVALID_BYTE_RATE",
    "PCM_TOO_LARGE", "MEMORY_ALLOCATION_FAILED",
]


def align_up(n, a):
    return (n + (a - 1)) & ~(a - 1)


def u16le(b):
    return struct.unpack_from("<H", b, 0)[0]


def u32le(b):
    return struct.unpack_from("<I", b, 0)[0]


class Fmt:
    __slots__ = ("audio_format", "num_channels", "sample_rate",
                 "byte_rate", "block_align", "bits_per_sample")

    def __init__(self):
        self.audio_format = self.num_channels = self.sample_rate = 0
        self.byte_rate = self.block_align = self.bits_per_sample = 0

    def as_dict(self):
        return {k: getattr(self, k) for k in self.__slots__}


def wav_probe(data: bytes, file_size: int):
    """Devuelve (status_str, fmt, data_offset, data_size). Espeja jbs_wav_probe()
    (FASE 2.1: recorrido limitado a [12, riff_end), no a [12, file_size))."""
    fmt = Fmt()

    if file_size < 12:
        return "INVALID_RIFF", fmt, 0, 0
    if len(data) < 12:
        return "READ_FAILED", fmt, 0, 0
    if data[0:4] != b"RIFF":
        return "INVALID_RIFF", fmt, 0, 0
    if data[8:12] != b"WAVE":
        return "INVALID_WAVE", fmt, 0, 0

    # riff_size / riff_end: limite real y declarado del contenedor RIFF.
    riff_size = u32le(data[4:8])
    if riff_size < 4:                    # debe cubrir al menos "WAVE"
        return "INVALID_RIFF", fmt, 0, 0
    riff_end = 8 + riff_size             # Python: enteros arbitrarios, sin overflow posible
    if riff_end > file_size:
        return "INVALID_RIFF", fmt, 0, 0
    # file_size > riff_end permitido: trailing data, NO se recorre como chunks.

    pos = 12
    have_fmt = have_data = False
    data_off = data_size = 0

    while pos + 8 <= riff_end:
        ch = data[pos:pos + 8]
        if len(ch) < 8:
            return "READ_FAILED", fmt, 0, 0
        csize = u32le(ch[4:8])
        body = pos + 8

        if body > riff_end or csize > riff_end - body:
            if ch[0:4] == b"data":
                return "DATA_NOT_FOUND", fmt, 0, 0
            break

        if ch[0:4] == b"fmt ":
            if csize < 16:
                return "UNSUPPORTED_FORMAT", fmt, 0, 0
            fb = data[body:body + 16]
            if len(fb) < 16:
                return "READ_FAILED", fmt, 0, 0
            fmt.audio_format    = u16le(fb[0:2])
            fmt.num_channels    = u16le(fb[2:4])
            fmt.sample_rate     = u32le(fb[4:8])
            fmt.byte_rate       = u32le(fb[8:12])
            fmt.block_align     = u16le(fb[12:14])
            fmt.bits_per_sample = u16le(fb[14:16])
            have_fmt = True
        elif ch[0:4] == b"data":
            data_off = body
            data_size = csize
            have_data = True
        # else: chunk desconocido -> ignorar

        if have_fmt and have_data:
            break

        adv = csize + (csize & 1)
        if adv > riff_end - body:
            break
        pos = body + adv

    if not have_fmt:
        return "FMT_NOT_FOUND", fmt, 0, 0
    if fmt.audio_format != FORMAT_PCM:
        return "UNSUPPORTED_FORMAT", fmt, 0, 0
    if (fmt.num_channels != CHANNELS or fmt.sample_rate != SAMPLE_RATE
            or fmt.bits_per_sample != BITS_PER_SAMPLE):
        return "UNSUPPORTED_FORMAT", fmt, 0, 0
    if fmt.block_align != BLOCK_ALIGN:
        return "INVALID_BLOCK_ALIGN", fmt, 0, 0
    if fmt.byte_rate != BYTE_RATE:
        return "INVALID_BYTE_RATE", fmt, 0, 0
    if not have_data or data_size == 0:
        return "DATA_NOT_FOUND", fmt, 0, 0
    if data_size % BLOCK_ALIGN != 0:
        return "UNSUPPORTED_FORMAT", fmt, 0, 0
    if data_size > MAX_PCM_SIZE:
        return "PCM_TOO_LARGE", fmt, 0, 0

    return "OK", fmt, data_off, data_size


def load_startup_pcm(path: str):
    """Espeja jbs_load_startup_pcm(). Devuelve dict con status y, si OK, tamanos."""
    if not os.path.exists(path) or not os.path.isfile(path):
        return {"status": "FILE_NOT_FOUND"}
    size = os.path.getsize(path)
    if size < 12:
        return {"status": "INVALID_RIFF"}
    if size > SDFILE_MAX_FILE:
        return {"status": "PCM_TOO_LARGE"}

    with open(path, "rb") as fh:
        data = fh.read()

    st, fmt, off, dsize = wav_probe(data, size)
    if st != "OK":
        return {"status": st, "fmt": fmt.as_dict()}

    if dsize == 0 or dsize > MAX_PCM_SIZE:
        return {"status": "PCM_TOO_LARGE", "fmt": fmt.as_dict()}
    if off > size or dsize > size - off:
        return {"status": "DATA_NOT_FOUND", "fmt": fmt.as_dict()}

    alloc = align_up(dsize, PCM_ALIGN)
    chunk = data[off:off + dsize]
    if len(chunk) != dsize:
        return {"status": "READ_FAILED", "fmt": fmt.as_dict()}

    return {
        "status": "OK",
        "fmt": fmt.as_dict(),
        "pcm_bytes": dsize,
        "alloc_bytes": alloc,
        "tail_zeros": alloc - dsize,
    }
