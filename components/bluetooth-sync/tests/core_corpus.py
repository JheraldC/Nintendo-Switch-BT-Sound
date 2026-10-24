"""Synthetic SaveData images only. Independent SHA/HMAC via Python stdlib.
No input image, console key, SD/NAND backend, or SystemSettings is used.
The CMAC marker is a TEST DOUBLE, not cryptographic authentication.
"""
from pathlib import Path
import hashlib
import hmac
import struct

ROOT = Path(__file__).resolve().parent / "corpus"
ROOT.mkdir(exist_ok=True)
TAG = b"BSR-TEST-CMAC-v1"
assert len(TAG) == 16
SEED = bytes(range(32))
PAYLOAD = bytes((i * 17 + 3) & 255 for i in range(700))


def p32(buf, offset, value):
    struct.pack_into("<I", buf, offset, value)


def p64(buf, offset, value):
    struct.pack_into("<Q", buf, offset, value)


def digest_block(data, level):
    label = ("HierarchicalIntegrityVerificationStorage::" +
             ("Master" if level == 0 else f"L{level}")).encode()
    salt = hmac.new(label, SEED, hashlib.sha256).digest()
    result = bytearray(hashlib.sha256(salt + data).digest())
    result[31] |= 0x80
    return bytes(result)


def integrity_tree(data, count):
    levels = [None] * count
    levels[-1] = bytes(data)
    for i in range(count - 1, 0, -1):
        hashes = b"".join(digest_block(levels[i][j:j+512].ljust(512, b"\0"), i)
                          for j in range(0, len(levels[i]), 512))
        levels[i-1] = hashes.ljust(512, b"\0")
    master = b"".join(digest_block(levels[0][j:j+512].ljust(512, b"\0"), 0)
                       for j in range(0, len(levels[0]), 512))
    return levels, master


def list_data(name, parent, file=False):
    data = bytearray(512)
    p32(data, 0, 3)
    p32(data, 4, 3)
    p32(data, 0x60 + 0x5C, 2)
    p32(data, 0xC0, parent)
    data[0xC4:0xC4+len(name)] = name
    if file:
        p32(data, 0xC0 + 0x48, 2)
        p64(data, 0xC0 + 0x4C, len(PAYLOAD))
    else:
        p32(data, 0xC0 + 0x4C, 2)
    return data


def build(case="valid-v5"):
    legacy = case == "valid-v4"
    empty = case in ("valid-empty","empty-invalid-marker")
    header = bytearray(0x4000)
    data = bytearray(0x800)
    data[:512] = list_data(b"", 0)
    data[512:1024] = list_data(b"file", 2, True)
    data[1024:1024+len(PAYLOAD)] = PAYLOAD
    fat = bytearray(512)
    for index in (1, 2, 3):
        p32(fat, index*8, 0x80000000)
    p32(fat, 3*8+4, 0x80000000)
    p32(fat, 4*8, 0x80000003)
    p32(fat, 4*8+4, 4)
    if empty:
        p32(data, 512+0xC0+0x48, 0xFFFFFFFF if case == "valid-empty" else 0x80000000)
        p64(data, 512+0xC0+0x4C, 0)
        p32(fat, 4, 3)  # blocks 2..3 become a valid free range
    # Malformed filesystem/FAT cases are rehashed to reach the appropriate layer.
    if case == "fat-next-oob":
        p32(fat, 1*8+4, 99)
    if case == "fat-cycle":
        p32(fat, 3*8+4, 0x80000003)
    if case == "fat-backlink":
        p32(fat, 1*8, 1)
    if case == "fat-range-bad":
        p32(fat, 4*8+4, 0xFFFFFFFF)
    if case == "file-start-oob":
        p32(data, 512+0xC0+0x48, 77)
    if case == "file-too-long":
        p64(data, 512+0xC0+0x4C, 0xFFFFFFFFFFFFFFFF)
    if case == "file-crosslink":
        p32(data, 512+0xC0+0x48, 0)
    if case == "name-no-nul":
        data[512+0xC4:512+0xC4+64] = b"X" * 64
    if case == "name-slash":
        data[512+0xC4:512+0xC4+5] = b"f/ile"
    if case == "file-missing":
        data[512+0xC4:512+0xC4+4] = b"else"
    if case == "list-cycle":
        p32(data, 512+0xC0+0x5C, 2)
    if case == "list-oob":
        p32(data, 512+0x60+0x5C, 99)
    if case == "list-huge":
        p32(data, 512+4, 0xFFFFFFFF)
    if case == "list-zero":
        p32(data, 512+4, 0)
    if case == "parent-oob":
        p32(data, 512+0xC0, 99)
    if case == "parent-cycle":
        p32(data, 0xC0, 2)
    if case == "child-cycle":
        p32(data, 512+0xC0+0x44, 2)
    if case == "child-oob":
        p32(data, 0xC0+0x4C, 99)
    if case == "duplicate-file":
        p32(data, 512, 4)
        p32(data, 512+4, 4)
        data[512+0x120:512+0x180] = data[512+0xC0:512+0x120]
        p32(data, 512+0xC0+0x5C, 3)
        p32(data, 512+0xC0+0x44, 3)
    # IVFC data and FAT have separate authenticated trees.
    data_levels, data_master = integrity_tree(data, 4)
    fat_levels, fat_master = integrity_tree(fat, 3)
    metadata = bytearray(0x2000)
    for offset, layer in zip((0, 0x200, 0x400), data_levels[:3]):
        metadata[offset:offset+len(layer)] = layer
    if legacy:
        metadata[0xA00:0xC00] = fat
    else:
        for offset, layer in zip((0x600, 0x800, 0xA00), fat_levels):
            metadata[offset:offset+len(layer)] = layer
    if case == "ivfc-data-corrupt":
        data[1030] ^= 1
    if case == "ivfc-top-corrupt":
        metadata[0] ^= 1
    if case == "ivfc-fat-corrupt":
        metadata[0xA00+10] ^= 1
    def layout64(offset, value):
        p64(header, 0x100+offset, value)
    header[:16] = TAG
    header[0x100:0x104] = b"DISF"
    p32(header, 0x104, 0x40000 if legacy else 0x50000)
    for offset, value in {
        0x28:0x8000,0x30:0x200,0x38:0x8200,0x40:0x200,0x48:0x8400,0x50:0x5000,
        0x58:0,0x60:0x200,0x68:4,0x70:0x400,0x78:0x2400,0x80:0x2000,
        0x88:0x4400,0x90:0x800,0x98:0x800,0xA0:0x200,
        0xA8:0xCC0,0xB0:0xCC4,0xB8:4,0xC0:0xC00,0xC8:0xC60,0xD0:32,
        0xD8:0xC00,0xE0:32,0x118:0,0x120:512,0x128:0x200,0x130:512,
        0x138:0x400,0x140:512,0x148:0xA00,0x150:512,
        0x160:0xC20,0x168:0xC80,0x170:0x600,0x178:512,0x180:0x800,0x188:512
    }.items():
        layout64(offset, value)
    header[0x258] = 1
    header[0x300:0x304] = b"DPFS"
    p32(header, 0x304, 0x10000)
    for pos, offset, size, power in ((0x308,0xCC0,4,2),(0x31C,0,4,2),(0x330,0x400,0x2000,9)):
        p64(header,pos,offset)
        p64(header,pos+8,size)
        p32(header,pos+16,power)
    def ivfc(pos, offsets, sizes, root):
        header[pos:pos+4] = b"IVFC"
        p32(header,pos+4,0x20000)
        p32(header,pos+8,len(root))
        p32(header,pos+12,len(offsets)+1)
        for i,(offset,size) in enumerate(zip(offsets,sizes)):
            p64(header,pos+0x10+i*24,offset)
            p64(header,pos+0x18+i*24,size)
            p32(header,pos+0x20+i*24,9)
        header[pos+0xA0:pos+0xC0] = SEED
    ivfc(0x344,(0,0x200,0x400,0),(512,512,512,0x800),data_master)
    if not legacy:
        ivfc(0xAD8,(0x600,0x800,0xA00),(512,512,512),fat_master)
    header[0xC00:0xC20] = data_master
    header[0xC60:0xC80] = data_master
    header[0xC20:0xC40] = fat_master
    header[0xC80:0xCA0] = fat_master
    p32(header,0xCC4,0x80000000)  # active master B selects L1 B
    header[0x408:0x40C] = b"JNGL"
    p32(header,0x40C,0x10000)
    p64(header,0x410,0xA00)
    p64(header,0x418,0x200)
    p64(header,0x420,0x200)
    p32(header,0x428,1)
    p32(header,0x42C,4)
    p32(header,0x430,1)
    # Nonidentity journal mapping.
    journal_map = (2,0,4,1)
    journal = bytearray(0xA00)
    for i,physical in enumerate(journal_map):
        p32(metadata,0xC00+i*8,physical)
        journal[physical*512:(physical+1)*512] = data[i*512:(i+1)*512]
    if case == "journal-oob":
        p32(metadata,0xC00,99)
    if case == "journal-duplicate":
        p32(metadata,0xC08,journal_map[0])
    header[0x608:0x60C] = b"SAVE"
    p32(header,0x60C,0x60000)
    p64(header,0x610,4)
    p64(header,0x618,512)
    p64(header,0x620,512)
    p32(header,0x630,63 if case == "valid-fat-padding" else 5 if empty else 4)
    if case == "fs-data-range":p32(header,0x630,5)
    if case == "valid-save-shrunk":p64(header,0x610,8)
    p32(header,0x640,4)
    p32(header,0x648,0)
    p32(header,0x64C,1)
    for pos in (0x650,0x690):
        header[pos:pos+4] = b"RMAP"
        p32(header,pos+4,0x10000)
        p32(header,pos+8,2)
        p32(header,pos+12,1)
        p32(header,pos+16,4)
    # Meta-remap swaps 0x1000-byte halves.
    physical_metadata = metadata[0x1000:] + metadata[:0x1000]
    main = bytearray(0x5000)
    p32(main,0x200,0xA5A50000)  # L1 bitmap B selects mixed data copies
    for block in range(16):
        use_b = bool(0xA5A50000 & (1 << (31-block)))
        for copy,offset in ((0,0x400),(1,0x2400)):
            main[offset+block*512:offset+(block+1)*512] = (
                physical_metadata[block*512:(block+1)*512] if copy==use_b else b"\xD3"*512)
    main[0x4400:0x4E00] = journal
    image = bytearray(0xD400)
    def remap_entry(pos, virtual, physical, size):
        struct.pack_into("<QQQII", image, pos, virtual,physical,size,0x200,0)
    remap_entry(0x8000,0,0x2800,0x2800)
    remap_entry(0x8020,0x2800,0,0x2800)
    remap_entry(0x8200,0,0x1000,0x1000)
    remap_entry(0x8220,0x1000,0,0x1000)
    image[0x8400:0xAC00] = main[0x2800:]
    image[0xAC00:0xD400] = main[:0x2800]
    mutations = {
        "remap-zero":(0x658,0),"remap-extreme":(0x658,0xFFFFFFFF),
        "remap-segments":(0x65C,3),"remap-missing-segment":(0x65C,2),
        "segment-bits":(0x660,64),"segment-bits-zero-invalid":(0x660,0xFFFFFFFF),
        "ivfc-levels":(0x350,6),"ivfc-levels-zero-invalid":(0x350,0xFFFFFFFF),
        "ivfc-power":(0x364,63),"fat-ivfc-levels":(0xAE4,5),
        "duplex-power":(0x340,64),"duplex-index":(0x258,2),
        "journal-count":(0x42C,0xFFFFFFFF),"fat-count":(0x630,0xFFFFFFFF),
        "meta-remap-zero":(0x698,0),"meta-remap-segments":(0x69C,3),
        "meta-segment-bits":(0x6A0,64),"fat-ivfc-power":(0xAF8,64),
        "fat-capacity-small":(0x630,3),"fat-capacity-zero":(0x630,0),
        "fs-block-zero":(0x618,0),"fs-block-not-power":(0x618,513),
        "fs-block-large":(0x618,131072),"fs-block-mismatch":(0x620,1024),
        "fs-data-zero":(0x640,0),"fs-data-large":(0x640,65537),
        "fs-data-range":(0x640,5),"fat-capacity-storage":(0x630,64)
    }
    if case in mutations:
        pos,value = mutations[case]
        p32(header,pos,value)
    if case == "duplex-too-short":
        layout64(0x68,1)
        p64(header,0x324,1)
    if case == "master-oob":
        layout64(0xA8,0xFFFFFFFFFFFFFFFF)
    if case == "table-overflow":
        layout64(0x28,0xFFFFFFFFFFFFFFF0)
        layout64(0x30,0x40)
    if case == "table-truncated":
        layout64(0x30,0x20)
    if case == "regression-one-entry-two-segments":
        p32(header,0x658,1)
        p32(header,0x65C,2)
    if case == "journal-zero-block":
        p64(header,0x420,0)
    if case == "journal-size-underflow":
        p64(header,0x410,0x100)
    if case == "journal-data-truncated":
        layout64(0x88,0x4C00)
    if case == "fat-offset-unsupported":
        p64(header,0x628,8)
    if case == "top-overlap":
        layout64(0x38,0x8000)
    if case == "remap-virtual-overflow":
        p64(image,0x8000,0xFFFFFFFFFFFFFF00)
    if case == "remap-physical-overflow":
        p64(image,0x8008,0xFFFFFFFFFFFFFF00)
    if case == "remap-size-overflow":
        p64(image,0x8010,0xFFFFFFFFFFFFFFFF)
    if case == "remap-gap":
        p64(image,0x8020,0x3000)
    if case == "remap-align":
        p32(image,0x8018,7)
    if case == "valid-no-segment-bits":
        p32(header,0x660,0)
        p32(header,0x6A0,0)
    if case == "valid-unused-levels":
        p32(header,0x350,0)
        p32(header,0xAE4,0)
    header[0x108:0x128] = hashlib.sha256(header[0x300:]).digest()
    if case == "cmac-invalid":
        header[0] ^= 1
    if case == "header-hash-invalid":
        header[0x108] ^= 1
    image[:0x4000] = header
    image[0x4000:0x8000] = header
    if case == "header-ab-disagree":
        p32(image,0x4104,0x40000)  # authenticated version 4 vs 5, hashes still match
    if case == "header-b-cmac":
        image[0x4000] ^= 1
    return image


PASS = ["valid-v5","valid-v4","valid-empty","valid-unused-levels","valid-no-segment-bits","valid-fat-padding","valid-save-shrunk"]
FAIL = ["fat-next-oob","fat-cycle","fat-backlink","fat-range-bad","file-start-oob",
        "file-too-long","file-crosslink","name-no-nul","name-slash","file-missing",
        "list-cycle","list-oob","list-huge","list-zero","parent-oob","parent-cycle",
        "child-cycle","child-oob","duplicate-file","ivfc-data-corrupt","ivfc-top-corrupt",
        "ivfc-fat-corrupt","journal-oob","journal-duplicate","remap-zero","remap-extreme",
        "remap-segments","remap-missing-segment","segment-bits","segment-bits-zero-invalid",
        "ivfc-levels","ivfc-levels-zero-invalid","ivfc-power","fat-ivfc-levels","duplex-power",
        "duplex-index","journal-count","fat-count","duplex-too-short","master-oob",
        "table-overflow","table-truncated","top-overlap","remap-virtual-overflow",
        "remap-physical-overflow","remap-size-overflow","remap-gap","remap-align",
        "cmac-invalid","header-hash-invalid","header-ab-disagree","header-b-cmac"]
FAIL += ["meta-remap-zero","meta-remap-segments","meta-segment-bits","fat-ivfc-power",
         "regression-one-entry-two-segments","journal-zero-block","journal-size-underflow",
         "journal-data-truncated","fat-offset-unsupported"]
FAIL += ["empty-invalid-marker","fat-capacity-small","fat-capacity-zero","fs-block-zero",
         "fs-block-not-power","fs-block-large","fs-block-mismatch","fs-data-zero",
         "fs-data-large","fs-data-range","fat-capacity-storage"]
for case in PASS + FAIL:
    (ROOT / (case + ".bin")).write_bytes(build(case))
(ROOT / "cases.txt").write_text("".join(f"{'PASS' if case in PASS else 'FAIL'} {case}\n"
                                      for case in PASS + FAIL), encoding="ascii")
(ROOT / "expected-file.bin").write_bytes(PAYLOAD)
print(f"Created {len(PASS)} valid + {len(FAIL)} malformed SYNTHETIC fixtures on PC.")
