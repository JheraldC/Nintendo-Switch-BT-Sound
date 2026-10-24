"""
replace_boot_kip.py - reempaquetado QUIRURGICO de package3: sustituye UNICAMENTE
el KIP "boot" por una version custom, reconstruyendo solo la region de KIPs
[0x100000:0x400000) y los metadatos del header que le corresponden. Todo lo
demas (warmboot, tsec_keygen, mariko_fatal, mtc, exosphere, mesosphere, el
early splash [0x400000:0x7C0000), fusee+rebootstub+padding [0x7C0000:0x800000))
se copia byte a byte sin tocar.

No recompila NADA de Atmosphere -- toma los demas componentes (emummc, Loader,
NCM, ProcessManager, sm, spl, ams_mitm) directamente del package3 base, ya
compilados y ya hardware-tested.

Uso:
    python replace_boot_kip.py package3.base boot_custom.kip package3.boot_custom

No escribe nada fuera de los 3 argumentos dados -- nunca toca una SD.
"""
import hashlib
import sys

import package3_format as fmt


def load(path):
    with open(path, 'rb') as f:
        return f.read()


def main():
    if len(sys.argv) != 4:
        print('Uso: python replace_boot_kip.py package3.base boot_custom.kip package3.boot_custom')
        return 1

    base_path, boot_custom_path, out_path = sys.argv[1:4]

    base = load(base_path)
    boot_custom = load(boot_custom_path)

    # --- validaciones de entrada ---
    assert base[:4] == fmt.MAGIC, "package3.base: magic PK31 incorrecto"
    assert len(base) == fmt.TOTAL_SIZE, f"package3.base: tamano {len(base)}, esperado {fmt.TOTAL_SIZE}"
    assert boot_custom[:4] == b'KIP1', "boot_custom.kip: magic KIP1 incorrecto"

    content_metas = fmt.read_content_metas(base)
    kip_metas = fmt.read_kip_metas(base)

    by_name = {m['name']: m for m in content_metas}
    kip_by_name = {m['name']: m for m in kip_metas}

    boot_meta = by_name['boot']
    boot_original = fmt.extract_component(base, boot_meta)

    # --- Program ID: no debe cambiar nunca ---
    pid_original = fmt.kip_program_id(boot_original)
    pid_custom = fmt.kip_program_id(boot_custom)
    if pid_original != pid_custom:
        print(f"DETENTE: Program ID cambio. original={pid_original.hex()} custom={pid_custom.hex()}")
        return 2
    print(f"Program ID boot: {int.from_bytes(pid_original, 'little'):#x} (sin cambios) -> OK")

    # --- extraer los 8 componentes originales de la region KIP, en el orden real ---
    originals = {}
    for name in fmt.KIP_REGION_ORDER:
        originals[name] = fmt.extract_component(base, by_name[name])

    # --- reconstruir la region KIP con boot sustituido ---
    region = bytearray()
    new_sizes = {}       # name -> tamano real (sin padding)
    new_abs_offsets = {}  # name -> offset absoluto donde empieza dentro del archivo final

    for name in fmt.KIP_REGION_ORDER:
        component = boot_custom if name == 'boot' else originals[name]
        abs_offset = fmt.KIP_REGION_START + len(region)
        new_abs_offsets[name] = abs_offset
        new_sizes[name] = len(component)
        region += component
        pad_len = fmt.align16(len(region)) - len(region)
        if pad_len:
            region += b'\xCC' * pad_len

    used_bytes = len(region)
    if used_bytes > fmt.KIP_REGION_BUDGET:
        print(f"DETENTE: desbordamiento del presupuesto de KIPs: {used_bytes} > {fmt.KIP_REGION_BUDGET}")
        return 3

    remaining = fmt.KIP_REGION_BUDGET - used_bytes
    region += b'\xCC' * remaining
    assert len(region) == fmt.KIP_REGION_BUDGET

    print(f"region KIP reconstruida: {used_bytes} bytes usados, {remaining} bytes de padding/headroom")

    # --- construir el header final: copia exacta del original, parcheando SOLO
    #     las entradas de content meta y kip meta que realmente cambian ---
    header = bytearray(base[:0x800])  # header completo tal cual, se parchea encima

    changed = []
    for name in ('boot', 'spl', 'ams_mitm'):
        m = by_name[name]
        new_offset = new_abs_offsets[name]
        new_size = new_sizes[name]
        if new_offset != m['offset'] or new_size != m['size']:
            changed.append(name)
        off = fmt.CONTENT_HEADER_OFFSET + m['index'] * fmt.CONTENT_META_SIZE
        header[off:off + fmt.CONTENT_META_SIZE] = fmt.pack_content_meta(
            new_offset, new_size, m['type'], m['a'], m['b'], m['c'], m['reserved'], name)

    for name in ('boot', 'spl', 'ams_mitm'):
        km = kip_by_name[name]
        component = boot_custom if name == 'boot' else originals[name]
        new_rel_offset = new_abs_offsets[name] - fmt.KIP_REGION_START
        new_size = new_sizes[name]
        new_sha = hashlib.sha256(component).digest()
        off = fmt.KIP_META_BASE_OFFSET + km['index'] * fmt.KIP_META_SIZE
        header[off:off + fmt.KIP_META_SIZE] = fmt.pack_kip_meta(
            km['program_id'], new_rel_offset, new_size, new_sha)

    print("content/kip metas actualizados para:", changed)

    # --- ensamblar el package3 final ---
    out = bytearray(fmt.TOTAL_SIZE)
    out[0x000:0x800] = header
    out[0x800:0x100000] = base[0x800:0x100000]          # warmboot..mesosphere, sin tocar
    out[0x100000:0x400000] = region                       # region KIP reconstruida
    out[0x400000:0x800000] = base[0x400000:0x800000]     # early splash + fusee + rebootstub, sin tocar

    assert len(out) == fmt.TOTAL_SIZE
    assert out[:4] == fmt.MAGIC

    with open(out_path, 'wb') as f:
        f.write(out)

    print(f"escrito: {out_path} ({len(out)} bytes)")
    return 0


if __name__ == '__main__':
    sys.exit(main())
