"""
run_config_tests.py - pruebas en PC de config_ref.py (puerto de source/config.c):
parser de config.ini, validacion de nombre de archivo, y la logica de
fallback en tiempo de ejecucion de main.c. Sin IPC/E-S real de Switch.

    python tests/run_config_tests.py
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import config_ref as c  # noqa: E402

FAIL = 0


def check(label, cond, detail=""):
    global FAIL
    status = "PASS" if cond else "FAIL"
    if not cond:
        FAIL += 1
    print(f"  [{status}] {label}" + (f"  ({detail})" if detail else ""))


def main():
    print("== 1) config inexistente ==")
    r = c.load_config_missing()
    check("status=NOT_FOUND, resolved=startup.wav",
          r["status"] == c.STATUS_NOT_FOUND and r["resolved"] == "startup.wav", str(r))

    print("\n== 2) config valido ==")
    r = c.load_config_text("[BootSound]\naudio_file=my_boot_sound.wav\n")
    check("status=OK, resolved=my_boot_sound.wav",
          r["status"] == c.STATUS_OK and r["resolved"] == "my_boot_sound.wav", str(r))

    print("\n== 3) espacios alrededor de '=' y en los bordes ==")
    r = c.load_config_text("  audio_file   =    my_sound.wav   \n")
    check("recortado correctamente -> resolved=my_sound.wav",
          r["status"] == c.STATUS_OK and r["resolved"] == "my_sound.wav", str(r))

    print("\n== 4) comentarios # y ; (lineas completas) ==")
    r = c.load_config_text(
        "# comentario con almohadilla\n"
        "; comentario con punto y coma\n"
        "\n"
        "   \n"
        "audio_file=boot.wav\n"
        "# otro comentario despues\n"
    )
    check("comentarios y lineas vacias ignorados -> resolved=boot.wav",
          r["status"] == c.STATUS_OK and r["resolved"] == "boot.wav", str(r))

    print("\n== 5) clave desconocida ==")
    r = c.load_config_text("enabled=1\nvolume=0.5\naudio_file=custom.wav\nfoo=bar\n")
    check("claves desconocidas ignoradas -> resolved=custom.wav",
          r["status"] == c.STATUS_OK and r["resolved"] == "custom.wav", str(r))

    print("\n== 6) audio_file vacio ==")
    r = c.load_config_text("audio_file=\n")
    check("status=INVALID_AUDIO_FILE, resolved=startup.wav (fallback)",
          r["status"] == c.STATUS_INVALID_AUDIO_FILE and r["resolved"] == "startup.wav", str(r))

    print("\n== 7-11) intentos de path traversal / rutas absolutas ==")
    malicious = [
        "../evil.wav",
        "..\\evil.wav",
        "/evil.wav",
        "C:\\evil.wav",
        "sdmc:/evil.wav",
    ]
    for val in malicious:
        r = c.load_config_text(f"audio_file={val}\n")
        check(f"'{val}' -> INVALID_AUDIO_FILE, resolved=startup.wav",
              r["status"] == c.STATUS_INVALID_AUDIO_FILE and r["resolved"] == "startup.wav",
              str(r))

    print("\n== 12) filename demasiado largo ==")
    long_name = ("a" * 130) + ".wav"   # 134 chars > JBS_AUDIO_FILENAME_MAX-1 (127)
    r = c.load_config_text(f"audio_file={long_name}\n")
    check(f"nombre de {len(long_name)} caracteres -> INVALID_AUDIO_FILE",
          r["status"] == c.STATUS_INVALID_AUDIO_FILE and r["resolved"] == "startup.wav", str(r))

    # limite exacto: 127 caracteres (JBS_AUDIO_FILENAME_MAX-1) debe ser el maximo ACEPTADO
    edge_ok = "a" * 123 + ".wav"     # 127 caracteres exactos
    r = c.load_config_text(f"audio_file={edge_ok}\n")
    check(f"nombre de exactamente {len(edge_ok)} caracteres (limite) -> OK",
          r["status"] == c.STATUS_OK and r["resolved"] == edge_ok,
          f"len={len(edge_ok)} status={r['status']}")

    edge_fail = "a" * 124 + ".wav"   # 128 caracteres: uno mas que el limite -> invalido
    r = c.load_config_text(f"audio_file={edge_fail}\n")
    check(f"nombre de exactamente {len(edge_fail)} caracteres (limite+1) -> INVALID_AUDIO_FILE",
          r["status"] == c.STATUS_INVALID_AUDIO_FILE,
          f"len={len(edge_fail)} status={r['status']}")

    print("\n== jbs_config_build_path: sin truncar ==")
    path, truncated = c.build_path("my_boot_sound.wav")
    check("ruta construida correctamente, sin truncar",
          not truncated and path == "sdmc:/config/JheraldBootSound/my_boot_sound.wav",
          str(path))

    print("\n== 13/14) fallback en tiempo de ejecucion (main.c, sin E/S real) ==")

    # 13: el WAV configurado no existe (o falla al cargar) -> cae a startup.wav
    def only_startup_exists(name):
        return name == "startup.wav"

    selected, fb, ok = c.resolve_with_fallback("my_boot_sound.wav", only_startup_exists)
    check("configurado inexistente -> fallback a startup.wav, exito",
          selected == "startup.wav" and fb is True and ok is True,
          f"selected={selected} fallback={fb} ok={ok}")

    # 14: ya era el default (nada de lo que caer hacia atras) y tambien falla
    def nothing_exists(name):
        return False

    selected, fb, ok = c.resolve_with_fallback("startup.wav", nothing_exists)
    check("ya era el default y tambien falla -> SIN reintento adicional, ok=False",
          selected == "startup.wav" and fb is False and ok is False,
          f"selected={selected} fallback={fb} ok={ok}")

    # caso feliz: el configurado existe, no hace falta fallback
    def everything_exists(name):
        return True

    selected, fb, ok = c.resolve_with_fallback("my_boot_sound.wav", everything_exists)
    check("configurado existe -> se usa tal cual, sin fallback",
          selected == "my_boot_sound.wav" and fb is False and ok is True,
          f"selected={selected} fallback={fb} ok={ok}")

    # caso "double failure": configurado falla Y startup.wav TAMBIEN falla
    def all_fail(name):
        return False

    selected, fb, ok = c.resolve_with_fallback("my_boot_sound.wav", all_fail)
    check("configurado falla Y startup.wav tambien falla -> selected=startup.wav (ultimo intento), ok=False",
          selected == "startup.wav" and fb is True and ok is False,
          f"selected={selected} fallback={fb} ok={ok}")

    print(f"\ntotal fails: {FAIL}")
    return 1 if FAIL else 0


if __name__ == "__main__":
    sys.exit(main())
