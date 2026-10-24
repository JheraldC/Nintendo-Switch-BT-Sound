"""
check_ipc_dispatch.py - comprobacion ESTATICA (no ejecuta nada, no hay IPC real
en PC) de que source/audio.c envia AtmosphereHasService (comando 65100) por
TIPC y no por CMIF. Esto es exactamente lo que FASE 3.3 corrigio tras el
fallo observado en hardware (FASE 3.1): AUDIO_SERVICE_QUERY_FAILED por usar
serviceDispatchInOut()/smGetServiceSession() (CMIF) en vez de
tipcDispatchInOut()/smGetServiceSessionTipc() (TIPC).

Uso:
    python tests/check_ipc_dispatch.py
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
AUDIO_C = os.path.join(ROOT, "source", "audio.c")

REQUIRED = ["tipcDispatchInOut", "smGetServiceSessionTipc"]
# Subcadenas literales que NO deben aparecer en CODIGO (CMIF). Un simple "in" basta:
# "smGetServiceSession(" NO es subcadena de "smGetServiceSessionTipc(" (hay
# "Tipc" de por medio), asi que no hay falso positivo entre ambas.
FORBIDDEN = ["serviceDispatchInOut", "smGetServiceSession("]


def strip_c_comments(text: str) -> str:
    """Quita comentarios /* ... */ y // para que el chequeo de FORBIDDEN no
    dispare por texto explicativo en los comentarios (p.ej. el propio
    historial del bug de FASE 3.1, que nombra a proposito las funciones CMIF
    que se dejaron de usar)."""
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    return text


def main():
    if not os.path.isfile(AUDIO_C):
        print(f"[FALLO] no existe {AUDIO_C}")
        return 1

    with open(AUDIO_C, "r", encoding="utf-8") as f:
        src = f.read()
    code_only = strip_c_comments(src)

    fails = 0

    # REQUIRED: basta con que esten en el archivo (aunque solo se documenten
    # en un comentario ya seria sospechoso que falten del codigo real, pero
    # se buscan sobre 'src' completo por simplicidad; el chequeo fuerte de
    # "de verdad se usan" es el de 65100 asociado a tipcDispatchInOut, abajo).
    for token in REQUIRED:
        if token in code_only:
            print(f"  [ OK  ] presente en el CODIGO: {token}")
        else:
            print(f"  [FALLO] falta en el codigo (o solo esta en un comentario): {token}")
            fails += 1

    for token in FORBIDDEN:
        if token in code_only:
            print(f"  [FALLO] presente en el CODIGO (no deberia): {token}")
            fails += 1
        else:
            print(f"  [ OK  ] ausente del codigo: {token}")

    # El comando 65100 debe aparecer asociado a tipcDispatchInOut, ya sea como
    # literal o como macro #define'ida a 65100 (que es como lo hace audio.c:
    # JBS_ATMOSPHERE_HAS_SERVICE_CMD), no "suelto" sin mas contexto.
    cmd_names = ["65100"] + re.findall(r"#define\s+(\w+)\s+65100u?\b", code_only)
    pattern = r"tipcDispatchInOut\([^;]*\b(?:%s)\b" % "|".join(re.escape(n) for n in cmd_names)
    m = re.search(pattern, code_only, re.S)
    if m:
        print(f"  [ OK  ] 65100 (directo o via {cmd_names[1:] or 'literal'}) dentro de tipcDispatchInOut(...)")
    else:
        print("  [FALLO] no se encontro 65100 (ni una macro definida a 65100) dentro de tipcDispatchInOut(...)")
        fails += 1

    print()
    if fails == 0:
        print("RESULTADO: OK - AtmosphereHasService (65100) se envia por TIPC, sin CMIF")
    else:
        print(f"RESULTADO: {fails} comprobacion(es) fallida(s)")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
