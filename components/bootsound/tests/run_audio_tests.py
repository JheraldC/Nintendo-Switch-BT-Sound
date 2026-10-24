"""
run_audio_tests.py - pruebas en PC de la logica PORTABLE de FASE 3 (audio.c):
formula de timeout de reproduccion, metadatos de AudioOutBuffer, y el WAV de
tono generado por scripts/generate_test_tone.py validado con el parser de
FASE 2.1.

NO emula audout ni Horizon: la reproduccion real solo se valida en consola
(ver docs/07-phase-3-audout.md una vez exista, o docs/DESIGN.md).

    python scripts/generate_test_tone.py tests/fixtures/startup_tone.wav
    python tests/run_audio_tests.py
"""
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "scripts"))

import audio_timing_ref as t   # noqa: E402
import audio_wait_ref as w      # noqa: E402
import wav_ref                  # noqa: E402
import generate_test_tone as gen  # noqa: E402

FAIL = 0


def check(label, cond, detail=""):
    global FAIL
    status = "PASS" if cond else "FAIL"
    if not cond:
        FAIL += 1
    print(f"  [{status}] {label}" + (f"  ({detail})" if detail else ""))


def main():
    print("== formula de timeout de reproduccion ==")

    r = t.playback_timeout_ns(0)
    check("pcm_size=0 -> duration=0, timeout=solo margen (2s)",
          r["duration_ns"] == 0 and r["timeout_ns"] == t.MARGIN_NS and not r["clamped"], str(r))

    r = t.playback_timeout_ns(96000)   # el WAV de prueba de FASE 2.1/3: 0.5 s
    check("pcm_size=96000 (0.5s) -> duration=500_000_000ns, timeout=2_500_000_000ns",
          r["duration_ns"] == 500_000_000 and r["timeout_ns"] == 2_500_000_000 and not r["clamped"],
          str(r))

    r = t.playback_timeout_ns(t.MAX_PCM_SIZE)   # 1 MiB, el caso mas largo posible en FASE 2/2.1
    expected_duration = (t.MAX_PCM_SIZE // t.BLOCK_ALIGN) * 1_000_000_000 // t.SAMPLE_RATE
    check("pcm_size=1 MiB -> duration ~5.46s, timeout = duration+2s (sin clamp, <10s)",
          r["duration_ns"] == expected_duration and r["timeout_ns"] == expected_duration + t.MARGIN_NS
          and not r["clamped"] and r["timeout_ns"] < t.MAX_TIMEOUT_NS,
          f"duration_ns={r['duration_ns']} timeout_ns={r['timeout_ns']}")

    # caso hipotetico (nunca deberia llegar asi desde sdfile.c, que limita a 1 MiB;
    # se prueba igualmente para confirmar que el clamp protege si algo cambiara)
    huge_pcm = 100_000_000
    r = t.playback_timeout_ns(huge_pcm)
    check("pcm_size hipotetico grande -> el timeout se recorta al maximo (10s)",
          r["timeout_ns"] == t.MAX_TIMEOUT_NS and r["clamped"], str(r))

    # guarda de overflow: frames * 1e9 nunca debe acercarse a desbordar u64 (2**64-1)
    max_frames = t.MAX_PCM_SIZE // t.BLOCK_ALIGN
    product = max_frames * 1_000_000_000
    check("frames_max * 1e9 muy por debajo de 2**64 (sin riesgo de overflow en u64)",
          product < (2 ** 64), f"product={product}  2**64={2**64}")

    print("\n== espera de audout:u (AtmosphereHasService, sin IPC real) ==")

    # 1) disponible de inmediato (primer poll)
    r = w.wait_and_init(lambda i: (False, True), w.SimClock())
    check("disponible en el primer poll -> OK, n_polls=1, wait_ms=0",
          r["status"] == "OK" and r["n_polls"] == 1 and r["wait_ms"] == 0, str(r))

    # 2) disponible tras varios polls (el 5o)
    r = w.wait_and_init(lambda i: (False, i >= 5), w.SimClock())
    check("disponible en el 5o poll -> OK, n_polls=5, wait_ms=4*20ms=80",
          r["status"] == "OK" and r["n_polls"] == 5 and r["wait_ms"] == 80, str(r))

    # 3) timeout a los 8 s exactos (nunca disponible)
    r = w.wait_and_init(lambda i: (False, False), w.SimClock())
    check("nunca disponible -> AUDIO_SERVICE_TIMEOUT a partir de 8000 ms",
          r["status"] == "AUDIO_SERVICE_TIMEOUT" and r["wait_ms"] >= 8000,
          f"wait_ms={r['wait_ms']} n_polls={r['n_polls']}")
    check("numero de polls hasta el timeout ~= 8000ms/20ms = 400 (+/-1)",
          399 <= r["n_polls"] <= 401, f"n_polls={r['n_polls']}")

    # 4) fallo de la propia consulta (65100 devuelve Result de error)
    r = w.wait_and_init(lambda i: (True, False), w.SimClock())
    check("fallo de consulta en el primer intento -> AUDIO_SERVICE_QUERY_FAILED, n_polls=1",
          r["status"] == "AUDIO_SERVICE_QUERY_FAILED" and r["n_polls"] == 1, str(r))

    # 4b) fallo de consulta tras varios polls correctos (no disponible aun)
    r = w.wait_and_init(lambda i: (i == 3, False), w.SimClock())
    check("fallo de consulta en el 3er intento -> AUDIO_SERVICE_QUERY_FAILED, n_polls=3",
          r["status"] == "AUDIO_SERVICE_QUERY_FAILED" and r["n_polls"] == 3, str(r))

    # 5) borde exacto del deadline. La disponibilidad SIEMPRE se consulta antes que el
    #    deadline dentro de la misma iteracion (igual que en audio.c: "if (available) break;"
    #    va antes de comprobar elapsed_ns >= timeout) -> el ultimo poll tiene siempre su
    #    oportunidad, no se descarta un "disponible" que llega justo a tiempo.
    #    Con sleep de 20ms, elapsed alcanza exactamente 8000ms al EMPEZAR el poll 401.
    r = w.wait_and_init(lambda i: (False, i >= 401), w.SimClock())
    check("borde: disponible justo en el poll donde elapsed llega a 8000ms -> gana OK (no se descarta)",
          r["status"] == "OK" and r["n_polls"] == 401 and r["wait_ms"] == 8000, str(r))

    r = w.wait_and_init(lambda i: (False, i >= 402), w.SimClock())
    check("borde: disponible un poll DESPUES del deadline -> nunca se llega a consultarlo, TIMEOUT",
          r["status"] == "AUDIO_SERVICE_TIMEOUT" and r["n_polls"] == 401 and r["wait_ms"] == 8000, str(r))

    # 6) audoutInitialize() disponible pero fallando -> AUDIO_INIT_FAILED (simulado con init_ok=False)
    r = w.wait_and_init(lambda i: (False, True), w.SimClock(), init_ok=False)
    check("servicio disponible pero init simulado en fallo -> AUDIO_INIT_FAILED",
          r["status"] == "AUDIO_INIT_FAILED", str(r))

    print("\n== metadatos de AudioOutBuffer ==")
    m = t.audio_out_buffer_meta(96000)
    check("data_size NO se redondea (96000 exacto)", m["data_size"] == 96000, str(m))
    check("buffer_size alineado a 0x1000 (98304)", m["buffer_size"] == 98304 and m["aligned_ok"], str(m))
    check("cola en 0 = buffer_size - data_size (2304 bytes)", m["tail_zero_bytes"] == 2304, str(m))
    check("data_offset == 0", m["data_offset"] == 0)

    m0 = t.audio_out_buffer_meta(0)
    check("data_size=0 -> buffer_size=0 (ALIGN_UP(0,0x1000)=0)", m0["buffer_size"] == 0, str(m0))

    print("\n== WAV de tono (scripts/generate_test_tone.py) ==")
    fixtures = os.path.join(HERE, "fixtures")
    os.makedirs(fixtures, exist_ok=True)
    tone_path = os.path.join(fixtures, "startup_tone.wav")
    blob = gen.build_wav()
    with open(tone_path, "wb") as f:
        f.write(blob)

    check("tamano total = 96044 bytes", len(blob) == gen.EXPECTED_FILE_BYTES, f"{len(blob)} bytes")

    parsed = wav_ref.load_startup_pcm(tone_path)
    check("parser FASE 2.1: status=OK", parsed.get("status") == "OK", str(parsed.get("status")))
    check("parser FASE 2.1: pcm_bytes=96000", parsed.get("pcm_bytes") == 96000)
    check("parser FASE 2.1: alloc_bytes=98304", parsed.get("alloc_bytes") == 98304)

    # comprobacion basica de fade-in/fade-out y amplitud moderada, sobre el PCM crudo
    pcm = blob[-gen.EXPECTED_PCM_BYTES:]
    samples = struct.unpack("<%dh" % (len(pcm) // 2), pcm)   # entrelazado L,R,L,R,...
    left = samples[0::2]

    first_sample_abs = abs(left[0])
    mid = len(left) // 2
    mid_peak = max(abs(s) for s in left[mid - 50: mid + 50])
    last_sample_abs = abs(left[-1])

    check("fade-in: primera muestra ~silenciosa frente al pico central",
          first_sample_abs < mid_peak * 0.05,
          f"first={first_sample_abs} mid_peak={mid_peak}")
    check("fade-out: ultima muestra ~silenciosa frente al pico central",
          last_sample_abs < mid_peak * 0.05,
          f"last={last_sample_abs} mid_peak={mid_peak}")
    check("amplitud moderada: pico central entre 15% y 30% de 32767 (nunca 100%)",
          0.15 * 32767 <= mid_peak <= 0.30 * 32767,
          f"mid_peak={mid_peak} ({mid_peak/32767*100:.1f}% de rango)")

    print(f"\ntotal fails: {FAIL}")
    return 1 if FAIL else 0


if __name__ == "__main__":
    sys.exit(main())
