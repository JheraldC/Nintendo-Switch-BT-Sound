#!/usr/bin/env python3
"""
generate_test_wav.py - genera de forma DETERMINISTA el WAV de prueba usado en la
verificacion en hardware de FASE 2.1 (silencio puro, sin dependencias externas).

RIFF/WAVE, PCM, 48000 Hz, 16-bit, estereo, 0.5 s de silencio.

Bytes exactos (deterministas, siempre los mismos):
    pcm bytes (data)      = 96000
    riff_size (bytes 4-7) = 96036
    tamano total del WAV  = 96044

Uso:
    python scripts/generate_test_wav.py [ruta_salida.wav]
    (por defecto: startup_test.wav en el directorio actual)
"""
import struct
import sys

SAMPLE_RATE = 48000
CHANNELS = 2
BITS_PER_SAMPLE = 16
DURATION_S = 0.5

EXPECTED_PCM_BYTES = 96000
EXPECTED_RIFF_SIZE = 96036
EXPECTED_FILE_BYTES = 96044


def build_wav() -> bytes:
    block_align = CHANNELS * BITS_PER_SAMPLE // 8          # 4
    byte_rate = SAMPLE_RATE * block_align                  # 192000
    n_samples = int(SAMPLE_RATE * DURATION_S)               # 24000
    data = b"\x00" * (n_samples * block_align)               # silencio, 96000 bytes

    fmt_body = struct.pack("<HHIIHH", 1, CHANNELS, SAMPLE_RATE, byte_rate,
                            block_align, BITS_PER_SAMPLE)
    fmt_chunk = b"fmt " + struct.pack("<I", len(fmt_body)) + fmt_body
    data_chunk = b"data" + struct.pack("<I", len(data)) + data

    payload = fmt_chunk + data_chunk
    riff_size = 4 + len(payload)
    blob = b"RIFF" + struct.pack("<I", riff_size) + b"WAVE" + payload

    assert len(data) == EXPECTED_PCM_BYTES, len(data)
    assert riff_size == EXPECTED_RIFF_SIZE, riff_size
    assert len(blob) == EXPECTED_FILE_BYTES, len(blob)
    return blob


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "startup_test.wav"
    blob = build_wav()
    with open(out, "wb") as f:
        f.write(blob)
    print(f"wrote {out}: {len(blob)} bytes total ({EXPECTED_PCM_BYTES} bytes PCM, "
          f"riff_size={EXPECTED_RIFF_SIZE})")


if __name__ == "__main__":
    main()
