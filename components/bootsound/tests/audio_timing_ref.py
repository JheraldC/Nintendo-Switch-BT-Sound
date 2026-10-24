"""
audio_timing_ref.py - port 1:1 de la aritmetica PORTABLE de source/audio.c
(formula de timeout de reproduccion + metadatos del AudioOutBuffer). NO
intenta emular audout ni Horizon: eso solo se valida en consola real.

Mantener sincronizado con las constantes de source/audio.c.
"""

BLOCK_ALIGN = 4                        # JBS_WAV_BLOCK_ALIGN
SAMPLE_RATE = 48000                    # JBS_WAV_SAMPLE_RATE
MARGIN_NS = 2_000_000_000              # JBS_PLAYBACK_TIMEOUT_MARGIN_NS (2 s)
MAX_TIMEOUT_NS = 10_000_000_000        # JBS_PLAYBACK_TIMEOUT_MAX_NS   (10 s)
PCM_ALIGN = 0x1000
MAX_PCM_SIZE = 1 * 1024 * 1024         # JBS_MAX_PCM_SIZE (1 MiB) - limite real impuesto por sdfile.c


def align_up(n, a):
    return (n + (a - 1)) & ~(a - 1)


def playback_timeout_ns(pcm_size: int) -> dict:
    """Espeja EXACTAMENTE la formula de jbs_audio_play() en audio.c."""
    frames = pcm_size // BLOCK_ALIGN
    duration_ns = (frames * 1_000_000_000) // SAMPLE_RATE
    timeout_ns = duration_ns + MARGIN_NS
    clamped = False
    if timeout_ns > MAX_TIMEOUT_NS:
        timeout_ns = MAX_TIMEOUT_NS
        clamped = True
    return {"frames": frames, "duration_ns": duration_ns,
            "timeout_ns": timeout_ns, "clamped": clamped}


def audio_out_buffer_meta(pcm_size: int) -> dict:
    """Espeja los metadatos que main.c/audio.c construyen para AudioOutBuffer
    (sin tocar memoria real; solo los numeros: buffer_size, data_size)."""
    alloc = align_up(pcm_size, PCM_ALIGN)
    return {
        "data_size": pcm_size,              # NUNCA se redondea
        "buffer_size": alloc,               # SIEMPRE alineado a 0x1000
        "data_offset": 0,
        "aligned_ok": (alloc % PCM_ALIGN) == 0,
        "tail_zero_bytes": alloc - pcm_size,
    }
