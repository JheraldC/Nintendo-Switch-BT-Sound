"""
gen_fixtures.py - genera los WAV de prueba en tests/fixtures/ + expected.json

Ejecutar:  python tests/gen_fixtures.py
"""
import json
import os
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "fixtures")
os.makedirs(OUT, exist_ok=True)


def chunk(cid: bytes, body: bytes) -> bytes:
    assert len(cid) == 4
    out = cid + struct.pack("<I", len(body)) + body
    if len(body) & 1:            # padding RIFF
        out += b"\x00"
    return out


def fmt_chunk(audio_fmt=1, ch=2, sr=48000, bps=16,
              block_align=None, byte_rate=None, extra=b""):
    if block_align is None:
        block_align = ch * bps // 8
    if byte_rate is None:
        byte_rate = sr * block_align
    body = struct.pack("<HHIIHH", audio_fmt, ch, sr, byte_rate, block_align, bps) + extra
    return chunk(b"fmt ", body)


def data_chunk(nbytes, declared=None):
    """Chunk 'data' con 'nbytes' bytes reales; 'declared' fuerza otro tamano en la cabecera."""
    body = bytes((i * 7) & 0xFF for i in range(nbytes))
    if declared is None:
        return chunk(b"data", body)
    # cabecera mentirosa: declara 'declared' pero solo hay 'nbytes'
    out = b"data" + struct.pack("<I", declared) + body
    return out


def riff(payload: bytes, riff_tag=b"RIFF", wave_tag=b"WAVE") -> bytes:
    return riff_tag + struct.pack("<I", 4 + len(payload)) + wave_tag + payload


def riff_custom(payload: bytes, riff_size_field: int, wave_tag=b"WAVE") -> bytes:
    """Como riff(), pero con el campo riff_size FORZADO a un valor arbitrario,
    independiente de len(payload). Para probar riff_size mentiroso / riff_end
    distinto del tamano fisico del archivo (FASE 2.1)."""
    return b"RIFF" + struct.pack("<I", riff_size_field) + wave_tag + payload


def write(name, blob):
    with open(os.path.join(OUT, name), "wb") as f:
        f.write(blob)
    return name


PCM_1S_STEREO = 48000 * 4          # 192000
SMALL = 8000 * 4                   # 32000 bytes = ~0.167 s
MAX_PCM = 2 * 1024 * 1024          # JBS_MAX_PCM_SIZE (FASE 4.0; era 1 MiB hasta FASE 3.3)

expected = {}

# 01 - correcto
expected[write("01_valid.wav",
    riff(fmt_chunk() + data_chunk(SMALL)))] = "OK"

# 02 - 44100 Hz
expected[write("02_sr44100.wav",
    riff(fmt_chunk(sr=44100) + data_chunk(SMALL)))] = "UNSUPPORTED_FORMAT"

# 03 - mono
expected[write("03_mono.wav",
    riff(fmt_chunk(ch=1) + data_chunk(SMALL)))] = "UNSUPPORTED_FORMAT"

# 04 - 24 bit
expected[write("04_24bit.wav",
    riff(fmt_chunk(bps=24) + data_chunk(SMALL)))] = "UNSUPPORTED_FORMAT"

# 05 - RIFF invalido
expected[write("05_bad_riff.wav",
    riff(fmt_chunk() + data_chunk(SMALL), riff_tag=b"RIFX"))] = "INVALID_RIFF"

# 06 - WAVE invalido
expected[write("06_bad_wave.wav",
    riff(fmt_chunk() + data_chunk(SMALL), wave_tag=b"WAVX"))] = "INVALID_WAVE"

# 07 - sin fmt (solo JUNK + data)
expected[write("07_no_fmt.wav",
    riff(chunk(b"JUNK", b"\x00" * 16) + data_chunk(SMALL)))] = "FMT_NOT_FOUND"

# 08 - sin data (solo fmt valido)
expected[write("08_no_data.wav",
    riff(fmt_chunk()))] = "DATA_NOT_FOUND"

# 09 - chunk desconocido antes de fmt
expected[write("09_unknown_before_fmt.wav",
    riff(chunk(b"LIST", b"INFOxxxx") + fmt_chunk() + data_chunk(SMALL)))] = "OK"

# 10 - chunk de tamano impar con padding, antes de fmt/data
expected[write("10_odd_chunk_padding.wav",
    riff(chunk(b"JUNK", b"\x01\x02\x03") + fmt_chunk() + data_chunk(SMALL)))] = "OK"

# 11 - archivo truncado: data declara mas de lo que hay
expected[write("11_truncated.wav",
    riff(fmt_chunk() + data_chunk(1000, declared=40000)))] = "DATA_NOT_FOUND"

# 12 - data_size falso / fuera de limites (enorme)
expected[write("12_fake_data_size.wav",
    riff(fmt_chunk() + data_chunk(64, declared=0x0FFFFFF0)))] = "DATA_NOT_FOUND"

# 13 - PCM demasiado grande (data_size real > MAX_PCM_SIZE, pero archivo < MAX+64K).
#      Usa MAX_PCM (el limite VIGENTE, 2 MiB desde FASE 4.0) para que este caso de
#      frontera siga significando "justo por encima del limite actual" pase lo que
#      pase con JBS_MAX_PCM_SIZE en el futuro; ver tambien 29_pcm_over_2mib.wav.
expected[write("13_too_large.wav",
    riff(fmt_chunk() + data_chunk(MAX_PCM + 4)))] = "PCM_TOO_LARGE"

# 14 - fmt con extra size > 16 (cbSize=0)
expected[write("14_fmt_extra_size.wav",
    riff(fmt_chunk(extra=struct.pack("<H", 0)) + data_chunk(SMALL)))] = "OK"

# 15 - chunks en orden no habitual: data antes de fmt
expected[write("15_data_before_fmt.wav",
    riff(data_chunk(SMALL) + fmt_chunk()))] = "OK"

# --- extra (mas alla de los 15 pedidos), para cubrir los estados restantes ---

# 16 - block_align incoherente
expected[write("16_bad_block_align.wav",
    riff(fmt_chunk(block_align=2, byte_rate=192000) + data_chunk(SMALL)))] = "INVALID_BLOCK_ALIGN"

# 17 - byte_rate incoherente
expected[write("17_bad_byte_rate.wav",
    riff(fmt_chunk(block_align=4, byte_rate=100000) + data_chunk(SMALL)))] = "INVALID_BYTE_RATE"

# 18 - data_size no multiplo de block_align (frames incompletos)
expected[write("18_data_not_frame_aligned.wav",
    riff(fmt_chunk() + data_chunk(30)))] = "UNSUPPORTED_FORMAT"

# 19 - fichero mas pequeno que la cabecera RIFF (12 bytes)
expected[write("19_too_small.wav", b"RIFF\x00\x00")] = "INVALID_RIFF"

# ---------------------------------------------------------------------------
# FASE 2.1 - endurecimiento riff_size / riff_end (5 casos pedidos explicitamente)
# ---------------------------------------------------------------------------

# 21 - riff_size CORRECTO (coincide exactamente con el contenido real)
_core21 = fmt_chunk() + data_chunk(SMALL)
expected[write("21_riff_size_correct.wav",
    riff_custom(_core21, 4 + len(_core21)))] = "OK"

# 22 - riff_size declara MAS bytes de los que el archivo tiene fisicamente
_core22 = fmt_chunk() + data_chunk(SMALL)
expected[write("22_riff_size_too_large.wav",
    riff_custom(_core22, 4 + len(_core22) + 1000))] = "INVALID_RIFF"

# 23 - riff_size MENOR que file_size: hay basura fisica despues de riff_end,
#      pero el WAV (fmt+data, ambos dentro de riff_end) debe aceptarse igual
#      y la basura debe quedar completamente ignorada (ni siquiera se recorre).
_core23 = fmt_chunk() + data_chunk(SMALL)
_trailing_garbage = b"JUNK" + struct.pack("<I", 0xFFFFFFF0) + b"\xAA" * 32
expected[write("23_trailing_data_ignored.wav",
    riff_custom(_core23 + _trailing_garbage, 4 + len(_core23)))] = "OK"

# 24 - el chunk "data" declara mas bytes de los que caben dentro de riff_end,
#      AUNQUE esos bytes existan fisicamente en el archivo (estan fuera del RIFF
#      declarado) -> debe rechazarse igualmente.
_fmt24 = fmt_chunk()
_data24_full = data_chunk(SMALL)                 # cabecera 'data' + SMALL bytes reales
_riff_end24 = len(_fmt24) + 8 + (SMALL // 2)     # riff_end solo cubre la mitad del data
expected[write("24_data_crosses_riff_end.wav",
    riff_custom(_fmt24 + _data24_full, 4 + _riff_end24))] = "DATA_NOT_FOUND"

# 25 - riff_size = 0xFFFFFFFF (maximo u32): riff_end debe calcularse en aritmetica
#      de 64 bits sin desbordar/envolver, y como riff_end >> file_size -> INVALID_RIFF.
_core25 = fmt_chunk() + data_chunk(SMALL)
expected[write("25_riff_size_overflow_guard.wav",
    riff_custom(_core25, 0xFFFFFFFF))] = "INVALID_RIFF"

# ---------------------------------------------------------------------------
# FASE 4.0 - limite PCM subido de 1 MiB a 2 MiB (4 casos de frontera)
# ---------------------------------------------------------------------------

# 26 - PCM por debajo del antiguo limite de 1 MiB (ya funcionaba, sigue OK)
expected[write("26_pcm_under_1mib.wav",
    riff(fmt_chunk() + data_chunk(900000)))] = "OK"

# 27 - PCM entre el antiguo limite (1 MiB) y el nuevo (2 MiB): ANTES de FASE 4.0
#      esto habria dado PCM_TOO_LARGE; ahora debe aceptarse.
expected[write("27_pcm_between_1_and_2mib.wav",
    riff(fmt_chunk() + data_chunk(1500000)))] = "OK"

# 28 - PCM exactamente en el nuevo limite (2 MiB): caso de frontera, debe aceptarse
expected[write("28_pcm_exactly_2mib.wav",
    riff(fmt_chunk() + data_chunk(MAX_PCM)))] = "OK"

# 29 - PCM un frame (4 bytes) por encima del nuevo limite: debe rechazarse
expected[write("29_pcm_over_2mib.wav",
    riff(fmt_chunk() + data_chunk(MAX_PCM + 4)))] = "PCM_TOO_LARGE"

# 30 - (no se crea archivo) -> FILE_NOT_FOUND lo prueba run_tests.py aparte
expected["30_missing_file"] = "FILE_NOT_FOUND"

with open(os.path.join(OUT, "expected.json"), "w", newline="\n") as f:
    json.dump(expected, f, indent=2)

print("generados %d fixtures en %s" % (len([k for k in expected if k != "20_missing_file"]), OUT))
for k, v in expected.items():
    print("  %-30s -> %s" % (k, v))
