"""
config_ref.py - port 1:1 en Python de source/config.c (parser de config.ini
y validacion de nombre de archivo). Sirve para probar la LOGICA en PC sin
Switch ni compilador C.

Mantener SINCRONIZADO con source/config.c: mismas reglas, mismo orden.
"""

AUDIO_FILENAME_MAX = 128   # == JBS_AUDIO_FILENAME_MAX (incluye el terminador nulo en C)

STATUS_NOT_FOUND = "NOT_FOUND"
STATUS_OK = "OK"
STATUS_INVALID_AUDIO_FILE = "INVALID_AUDIO_FILE"


def filename_is_safe(filename) -> bool:
    """Espeja jbs_config_filename_is_safe(). Valida el ORIGINAL sin truncar."""
    if filename is None:
        return False
    n = len(filename)
    if n == 0 or n >= AUDIO_FILENAME_MAX:
        return False
    for c in filename:
        if c in ("/", "\\", ":"):
            return False
    if ".." in filename:
        return False
    return True


def build_path(filename: str, dir_="sdmc:/config/JheraldBootSound", limit=192):
    """Espeja jbs_config_build_path(). Devuelve (path_or_None, truncated: bool)."""
    path = dir_ + "/" + filename
    if len(path) + 1 > limit:   # +1: terminador nulo, igual que snprintf/out_size
        return None, True
    return path, False


def _trim(s: str) -> str:
    return s.strip(" \t")


def load_config_text(text: str) -> dict:
    """Espeja jbs_config_load() para un config.ini que SI existe, dado ya su
    contenido completo como texto (equivalente a haberlo leido linea a linea
    con fgets())."""
    resolved = "startup.wav"
    configured = ""
    status = None
    has_audio_file = False

    for raw_line in text.splitlines():
        line = raw_line.rstrip("\r\n")
        p = line.lstrip(" \t")
        if p == "" or p[0] in ("#", ";", "["):
            continue
        if "=" not in p:
            continue

        key, _, val = p.partition("=")
        key = _trim(key)
        val = _trim(val)

        if key == "audio_file":
            has_audio_file = True
            configured = val[:AUDIO_FILENAME_MAX - 1]   # como strncpy(...,N-1)

            if filename_is_safe(val):                    # valida 'val' COMPLETO, no el truncado
                status = STATUS_OK
                resolved = val[:AUDIO_FILENAME_MAX - 1]
            else:
                status = STATUS_INVALID_AUDIO_FILE
                # 'resolved' se queda en el default "startup.wav"

    if not has_audio_file:
        status = STATUS_OK

    return {
        "status": status,
        "has_audio_file": has_audio_file,
        "configured": configured,
        "resolved": resolved,
    }


def load_config_missing() -> dict:
    """Espeja jbs_config_load() cuando config_path no existe (fopen falla)."""
    return {
        "status": STATUS_NOT_FOUND,
        "has_audio_file": False,
        "configured": "",
        "resolved": "startup.wav",
    }


def resolve_with_fallback(cfg_resolved: str, try_load_fn):
    """
    Espeja la logica de fallback de main.c (paso 3b), SIN E/S real:
      cfg_resolved : lo que devolvio jbs_config_load().resolved
      try_load_fn(filename) -> True si ese archivo cargaria con exito

    Devuelve (selected_filename, fallback_used, final_ok).
    """
    selected = cfg_resolved
    ok = try_load_fn(selected)

    fallback_used = False
    if not ok and selected != "startup.wav":
        selected = "startup.wav"
        fallback_used = True
        ok = try_load_fn(selected)

    return selected, fallback_used, ok
