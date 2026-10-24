"""
run_tests.py - ejecuta wav_ref (port de wav.c/sdfile.c) contra tests/fixtures/
y compara con expected.json.

    python tests/gen_fixtures.py     # 1) generar fixtures
    python tests/run_tests.py        # 2) ejecutar

Sale con codigo != 0 si algun caso no coincide.
"""
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import wav_ref  # noqa: E402

FIX = os.path.join(HERE, "fixtures")


def main():
    exp_path = os.path.join(FIX, "expected.json")
    if not os.path.exists(exp_path):
        print("Falta expected.json. Ejecuta primero:  python tests/gen_fixtures.py")
        return 2
    with open(exp_path) as f:
        expected = json.load(f)

    rows = []
    fails = 0
    for name in sorted(expected):
        want = expected[name]
        if name == "30_missing_file":
            got = wav_ref.load_startup_pcm(os.path.join(FIX, "__no_such_file__.wav"))
        else:
            got = wav_ref.load_startup_pcm(os.path.join(FIX, name))
        status = got["status"]
        ok = (status == want)
        if not ok:
            fails += 1
        extra = ""
        if status == "OK":
            extra = "pcm=%d alloc=%d tail0=%d" % (
                got["pcm_bytes"], got["alloc_bytes"], got["tail_zeros"])
        rows.append((name, want, status, "PASS" if ok else "FAIL", extra))

    w = max(len(r[0]) for r in rows)
    print("%-*s  %-20s  %-20s  %-4s  %s" % (w, "fixture", "esperado", "obtenido", "res", "detalle"))
    print("-" * (w + 60))
    for n, want, got, res, extra in rows:
        print("%-*s  %-20s  %-20s  %-4s  %s" % (w, n, want, got, res, extra))
    print("-" * (w + 60))
    print("total=%d  pass=%d  fail=%d" % (len(rows), len(rows) - fails, fails))

    # comprobacion adicional del caso OK principal
    ok1 = wav_ref.load_startup_pcm(os.path.join(FIX, "01_valid.wav"))
    assert ok1["status"] == "OK"
    assert ok1["pcm_bytes"] == 32000
    assert ok1["alloc_bytes"] == 32768          # ALIGN_UP(32000, 0x1000)
    assert ok1["alloc_bytes"] % 0x1000 == 0
    assert ok1["tail_zeros"] == 768
    print("check 01_valid: pcm=32000 alloc=32768 (0x1000-aligned) tail_zeros=768  OK")

    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
