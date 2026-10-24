#!/usr/bin/env python3
"""
generate_example_startup.py - genera un sonido de arranque de EJEMPLO,
100% sintetizado por codigo (ninguna fuente externa, ninguna grabacion,
sin contenido de terceros), listo para publicar en el arbol publico como
"startup.wav" de muestra para quien no quiera convertir su propio audio.

Arpegio ascendente corto tipo "campanita de bienvenida": 4 notas de un
acorde de Do mayor (C5-E5-G5-C6), cada una con ataque rapido y decaimiento
exponencial suave (no una sinusoide pura sostenida, para que no suene a
"pitido"), ligero solape entre notas, amplitud moderada (~25% del rango
Int16, nunca al 100%) y fade-out final.

RIFF/WAVE, PCM, 48000 Hz, 16-bit, estereo, exactamente el formato que
exige el parser del proyecto (ver wav.c / package/README_AUDIO.txt).

Uso:
    python scripts/generate_example_startup.py [ruta_salida.wav]
    (por defecto: startup_example.wav en el directorio actual)
"""
import math
import struct
import sys

SAMPLE_RATE = 48000
CHANNELS = 2
BITS_PER_SAMPLE = 16
INT16_MAX = 32767

# Arpegio de Do mayor, ascendente: C5, E5, G5, C6
NOTES_HZ = [523.25, 659.25, 783.99, 1046.50]
NOTE_DURATION_S = 0.30      # duracion de cada nota antes de que decaiga a ~0
NOTE_STRIDE_S = 0.20        # cada nota empieza 0.20 s despues de la anterior (se solapan)
DECAY_TAU_S = 0.22          # constante de tiempo del decaimiento exponencial
ATTACK_MS = 6.0             # ataque rapido tipo "campanita", evita click inicial
AMPLITUDE_FRACTION = 0.25   # pico maximo ~25% del rango Int16, nunca 100%
FINAL_FADE_MS = 25.0        # fade-out final del ultimo remanente, evita click de corte

TOTAL_DURATION_S = (len(NOTES_HZ) - 1) * NOTE_STRIDE_S + NOTE_DURATION_S + 0.15


def _note_envelope(t: float) -> float:
    """Ataque lineal rapido + decaimiento exponencial. t en segundos desde el inicio de la nota."""
    if t < 0:
        return 0.0
    attack_s = ATTACK_MS / 1000.0
    if t < attack_s:
        return t / attack_s
    return math.exp(-(t - attack_s) / DECAY_TAU_S)


def build_pcm() -> bytes:
    n_samples = int(round(SAMPLE_RATE * TOTAL_DURATION_S))
    final_fade_samples = max(1, int(SAMPLE_RATE * FINAL_FADE_MS / 1000.0))

    samples = [0.0] * n_samples
    for note_idx, freq in enumerate(NOTES_HZ):
        start_sample = int(round(note_idx * NOTE_STRIDE_S * SAMPLE_RATE))
        # un poco de segundo armonico (timbre mas "campana", menos "pitido puro")
        for i in range(start_sample, n_samples):
            t = (i - start_sample) / SAMPLE_RATE
            env = _note_envelope(t)
            if env <= 0.0005:
                continue
            phase = 2.0 * math.pi * freq * t
            wave = math.sin(phase) + 0.18 * math.sin(2.0 * phase)
            samples[i] += env * wave

    # normalizar al pico objetivo (AMPLITUDE_FRACTION del rango Int16)
    peak = max(1e-9, max(abs(s) for s in samples))
    scale = (AMPLITUDE_FRACTION * INT16_MAX) / peak
    for i in range(n_samples):
        samples[i] *= scale

    # fade-out final para que el remanente del decaimiento no corte en seco
    for k in range(final_fade_samples):
        idx = n_samples - final_fade_samples + k
        if 0 <= idx < n_samples:
            samples[idx] *= (final_fade_samples - k) / final_fade_samples

    frames = bytearray()
    for s in samples:
        v = int(round(s))
        v = max(-32768, min(32767, v))
        frames += struct.pack("<hh", v, v)   # mismo valor en ambos canales

    assert len(frames) == n_samples * CHANNELS * (BITS_PER_SAMPLE // 8)
    return bytes(frames)


def build_wav() -> bytes:
    block_align = CHANNELS * BITS_PER_SAMPLE // 8
    byte_rate = SAMPLE_RATE * block_align
    data = build_pcm()

    fmt_body = struct.pack("<HHIIHH", 1, CHANNELS, SAMPLE_RATE, byte_rate,
                            block_align, BITS_PER_SAMPLE)
    fmt_chunk = b"fmt " + struct.pack("<I", len(fmt_body)) + fmt_body
    data_chunk = b"data" + struct.pack("<I", len(data)) + data

    payload = fmt_chunk + data_chunk
    riff_size = 4 + len(payload)
    blob = b"RIFF" + struct.pack("<I", riff_size) + b"WAVE" + payload
    return blob


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "startup_example.wav"
    blob = build_wav()
    with open(out, "wb") as f:
        f.write(blob)
    print(f"wrote {out}: {len(blob)} bytes total, duration~{TOTAL_DURATION_S:.2f}s, "
          f"arpeggio={NOTES_HZ} Hz, amplitude~{AMPLITUDE_FRACTION*100:.0f}%")


if __name__ == "__main__":
    main()
