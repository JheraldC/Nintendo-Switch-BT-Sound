#!/usr/bin/env python3
"""
verify_json.py - comprobaciones estaticas del NPDM de BootSound.

NO compila nada. Solo valida, sin tener la toolchain instalada:
  - que BootSound.json es JSON valido
  - que el Program ID es coherente (title_id == range_min == range_max)
  - que el Program ID no colisiona con una lista conocida de sysmodules homebrew
  - que el Program ID no aparece en un arbol de SD que se le pase (--sd RUTA)
  - imprime un resumen legible del SAC y de las kernel capabilities

Uso:
  python scripts/verify_json.py
  python scripts/verify_json.py --sd /path/to/SD  (comprueba colision en esa SD; en Windows
                                                    seria por ejemplo E:/ o F:/, la letra que
                                                    le asigne tu sistema a la tarjeta)
"""
import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
JSON_PATH = os.path.join(ROOT, "BootSound.json")

# Sysmodules homebrew conocidos (para deteccion de colision "best effort").
# NO es un registro mundial; solo lo mas habitual. Ver docs/DESIGN.md correccion 7.
KNOWN = {
    "0100000000000000": "Nintendo (rango oficial de sistema)",
    "00FF0000636C6BFF": "sys-clk",
    "420000000000000B": "sys-patch",
    "420000000007E51A": "nx-ovlloader",
    "420000000007E51B": "nx-ovlloader (companion)",
    "0000000000534C56": "SaltyNX",
    "0100000000000352": "emuiibo",
    "010000000000BD00": "MissionControl",
    "4200000000000000": "sys-ftpd / varios (rango homebrew 0x42)",
    "420000000000000E": "sys-ftpd-light",
    "4200000000000010": "sys-tune (aprox.)",
    "010000000000D609": "sys-con (aprox.)",
    "0100000000000D60": "sys-httpd (aprox.)",
}


def fail(msg):
    print(f"  [FALLO] {msg}")
    return 1


def ok(msg):
    print(f"  [ OK  ] {msg}")
    return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--sd", help="ruta a una SD para comprobar colision de carpeta contents/<id>")
    args = ap.parse_args()

    rc = 0
    print(f"NPDM: {JSON_PATH}")

    try:
        with open(JSON_PATH, "r", encoding="utf-8") as f:
            raw = f.read()
        meta = json.loads(raw)
    except FileNotFoundError:
        return fail("no existe BootSound.json")
    except json.JSONDecodeError as e:
        return fail(f"JSON invalido: {e}")
    rc |= ok("JSON valido")

    # --- Program ID -----------------------------------------------------
    # npdmtool 1.13.x: nombre moderno "program_id*"; "title_id*" es alias deprecado.
    def norm(*keys):
        for k in keys:
            if k in meta:
                return str(meta[k]).lower().replace("0x", "").rjust(16, "0")
        return ""

    if "title_id" in meta or "title_id_range_min" in meta:
        print("  [AVISO] JSON usa el nombre deprecado 'title_id*'. Preferir 'program_id*'.")
    tid = norm("program_id", "title_id")
    rmin = norm("program_id_range_min", "title_id_range_min")
    rmax = norm("program_id_range_max", "title_id_range_max")

    if not tid or len(tid) != 16:
        rc |= fail(f"title_id con longitud inesperada: {tid!r}")
    else:
        rc |= ok(f"Program ID = {tid.upper()}  (ASCII cola: {bytes.fromhex(tid)!r})")

    if tid == rmin == rmax:
        rc |= ok("title_id == range_min == range_max")
    else:
        rc |= fail(f"rango incoherente: tid={tid} min={rmin} max={rmax}")

    if not tid.startswith("42"):
        print("  [AVISO] el Program ID no esta en el rango homebrew 0x42xxxxxxxxxxxxxx")

    hit = KNOWN.get(tid.upper())
    if hit:
        rc |= fail(f"colision con lista conocida: {hit}")
    else:
        rc |= ok("sin colision en la lista conocida (no es garantia global)")

    # --- Colision contra una SD real ------------------------------------
    if args.sd:
        contents = os.path.join(args.sd, "atmosphere", "contents")
        if os.path.isdir(contents):
            existing = {d.lower() for d in os.listdir(contents)}
            if tid in existing:
                rc |= fail(f"ya existe {contents}/{tid}")
            else:
                rc |= ok(f"no existe {contents}/{tid} (libre en esa SD)")
                print("         contenidos actuales: " + ", ".join(sorted(existing)))
        else:
            print(f"  [AVISO] no se encontro {contents}")

    # --- Resumen SAC / caps -------------------------------------------------
    print("\nService Access Control (SAC):")
    for s in meta.get("service_access", []):
        print(f"  - {s}")
    if not meta.get("service_access"):
        print("  (ninguno)")

    print("\nfilesystem_access.permissions:",
          meta.get("filesystem_access", {}).get("permissions"))

    caps = meta.get("kernel_capabilities", [])
    sysc = next((c["value"] for c in caps if c.get("type") == "syscalls"), {})
    print(f"\nkernel_capabilities: {len(caps)} bloques, {len(sysc)} syscalls permitidas")
    kf = next((c["value"] for c in caps if c.get("type") == "kernel_flags"), None)
    if kf:
        print(f"  kernel_flags: prio {kf.get('lowest_thread_priority')}-{kf.get('highest_thread_priority')}, "
              f"cpu {kf.get('lowest_cpu_id')}-{kf.get('highest_cpu_id')}")
    mkv = next((c["value"] for c in caps if c.get("type") == "min_kernel_version"), None)
    if mkv:
        print(f"  min_kernel_version: {mkv}")

    # syscalls que NO deberian estar (paranoia): MMIO / secure monitor
    forbidden = {"svcReadWriteRegister", "svcCallSecureMonitor",
                 "svcCreateDeviceAddressSpace", "svcMapDeviceAddressSpaceAligned"}
    bad = forbidden & set(sysc.keys())
    if bad:
        rc |= fail(f"syscalls no deseadas presentes: {sorted(bad)}")
    else:
        rc |= ok("sin syscalls de hardware/secure-monitor")

    print("\nRESULTADO:", "TODO OK" if rc == 0 else "HAY FALLOS")
    return rc


if __name__ == "__main__":
    sys.exit(main())
