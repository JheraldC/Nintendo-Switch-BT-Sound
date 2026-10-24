"""
audio_wait_ref.py - port 1:1 de la LOGICA de audio_wait_and_init() en source/audio.c
(bucle de polling con AtmosphereHasService + deadline monotonico), SIN emular IPC
real de Atmosphere ni de Horizon: se inyecta una funcion que simula la respuesta de
"servicio disponible" y un reloj simulado, para poder probar deadline/timeout/fallo
de consulta de forma determinista y sin dormir de verdad.

Mantener sincronizado con las constantes JBS_AUDIO_SERVICE_TIMEOUT_NS /
JBS_AUDIO_POLL_SLEEP_NS de source/audio.c.
"""

SERVICE_TIMEOUT_NS = 8_000_000_000   # JBS_AUDIO_SERVICE_TIMEOUT_NS (8 s)
POLL_SLEEP_NS = 20_000_000            # JBS_AUDIO_POLL_SLEEP_NS     (20 ms)


class SimClock:
    """Reloj monotonico simulado: avanza solo cuando se le pide dormir."""
    def __init__(self):
        self._t = 0

    def now_ns(self):
        return self._t

    def sleep(self, ns):
        self._t += ns


def wait_and_init(has_service_fn, clock, init_ok=True):
    """
    Espeja audio_wait_and_init() SIN la llamada real a audoutInitialize():
    'init_ok' simula si esa unica llamada final tendria exito.

    has_service_fn(poll_index) -> (failed: bool, available: bool)
        poll_index empieza en 1 en cada llamada.

    Devuelve dict: status, wait_ms, n_polls.
    """
    t0 = clock.now_ns()
    n_polls = 0

    while True:
        n_polls += 1
        failed, available = has_service_fn(n_polls)

        if failed:
            wait_ms = (clock.now_ns() - t0) // 1_000_000
            return {"status": "AUDIO_SERVICE_QUERY_FAILED", "wait_ms": wait_ms, "n_polls": n_polls}

        if available:
            wait_ms = (clock.now_ns() - t0) // 1_000_000
            if not init_ok:
                return {"status": "AUDIO_INIT_FAILED", "wait_ms": wait_ms, "n_polls": n_polls}
            return {"status": "OK", "wait_ms": wait_ms, "n_polls": n_polls}

        elapsed = clock.now_ns() - t0
        if elapsed >= SERVICE_TIMEOUT_NS:
            return {"status": "AUDIO_SERVICE_TIMEOUT", "wait_ms": elapsed // 1_000_000, "n_polls": n_polls}

        clock.sleep(POLL_SLEEP_NS)
