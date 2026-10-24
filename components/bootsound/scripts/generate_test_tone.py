#!/usr/bin/env python3
"""
generate_test_tone.py - genera de forma DETERMINISTA el WAV de prueba AUDIBLE
usado para verificar la reproduccion real en FASE 3 (audout).

RIFF/WAVE, PCM, 48000 Hz, 16-bit, estereo, 0.5 s, tono de 880 Hz, amplitud
moderada (~22% del rango Int16) con fade-in/fade-out de ~20 ms para evitar
clicks. Mismo tamano exacto de archivo que el WAV de silencio de FASE 2.1
(el contenido cambia, la duracion y por tanto el numero de bytes no).

Bytes exactos (deterministas, siempre los mismos):
    pcm bytes (data)      = 96000
    riff_size (bytes 4-7) = 96036
    tamano total del WAV  = 96044

Uso:
    python scripts/generate_test_tone.py [ruta_salida.wav]
    (por defecto: startup_tone.wav en el directorio actual)
"""
import math
import struct
import sys

SAMPLE_RATE = 48000
CHANNELS = 2
BITS_PER_SAMPLE = 16
DURATION_S = 0.5

TONE_HZ = 880.0
AMPLITUDE_FRACTION = 0.22          # ~20-25% del rango Int16, nunca 100%
FADE_MS = 20.0                     # fade-in / fade-out

EXPECTED_PCM_BYTES = 96000
EXPECTED_RIFF_SIZE = 96036
EXPECTED_FILE_BYTES = 96044

INT16_MAX = 32767


def build_tone_pcm() -> bytes:
    n_samples = int(SAMPLE_RATE * DURATION_S)          # 24000 frames
    fade_samples = max(1, int(SAMPLE_RATE * FADE_MS / 1000.0))  # 960
    amplitude = AMPLITUDE_FRACTION * INT16_MAX

    frames = bytearray()
    for i in range(n_samples):
        # envolvente: fade-in lineal, meseta, fade-out lineal
        if i < fade_samples:
            env = i / fade_samples
        elif i >= n_samples - fade_samples:
            env = (n_samples - 1 - i) / fade_samples
        else:
            env = 1.0
        env = max(0.0, min(1.0, env))

        sample = int(round(amplitude * env * math.sin(2.0 * math.pi * TONE_HZ * i / SAMPLE_RATE)))
        sample = max(-32768, min(32767, sample))
        # estereo: mismo valor en ambos canales
        frames += struct.pack("<hh", sample, sample)

    assert len(frames) == n_samples * CHANNELS * (BITS_PER_SAMPLE // 8)
    return bytes(frames)


def build_wav() -> bytes:
    block_align = CHANNELS * BITS_PER_SAMPLE // 8          # 4
    byte_rate = SAMPLE_RATE * block_align                  # 192000
    data = build_tone_pcm()                                 # 96000 bytes

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
    out = sys.argv[1] if len(sys.argv) > 1 else "startup_tone.wav"
    blob = build_wav()
    with open(out, "wb") as f:
        f.write(blob)
    print(f"wrote {out}: {len(blob)} bytes total ({EXPECTED_PCM_BYTES} bytes PCM, "
          f"riff_size={EXPECTED_RIFF_SIZE}, tone={TONE_HZ:g} Hz, "
          f"amplitude~{AMPLITUDE_FRACTION*100:.0f}%, fade={FADE_MS:g} ms)")


if __name__ == "__main__":
    main()
