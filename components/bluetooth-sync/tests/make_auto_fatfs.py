"""Synthetic FAT32 with LFN directories. No SD or real console data read."""
from pathlib import Path
import struct

image = bytearray(70000 * 512)
struct.pack_into("<H", image, 11, 512)
image[:3] = b"\xeb\x58\x90"
image[3:11] = b"SYNTHETI"
image[13] = 1
struct.pack_into("<H", image, 14, 32)
image[16] = 2
image[21] = 0xF8
struct.pack_into("<I", image, 32, 70000)
struct.pack_into("<I", image, 36, 600)
struct.pack_into("<I", image, 44, 2)
struct.pack_into("<H", image, 48, 1)
struct.pack_into("<H", image, 50, 6)
image[66] = 0x29
image[82:90] = b"FAT32   "
image[510:512] = b"\x55\xaa"
next_cluster = 3
fat = {0: 0x0FFFFFF8, 1: 0x0FFFFFFF, 2: 0x0FFFFFFF}
directories = {"": (2, [])}


def allocate(data):
    global next_cluster
    clusters = list(range(next_cluster, next_cluster + max(1, (len(data) + 511) // 512)))
    next_cluster += len(clusters)
    for i, cluster in enumerate(clusters):
        fat[cluster] = clusters[i + 1] if i + 1 < len(clusters) else 0x0FFFFFFF
        start = (1232 + cluster - 2) * 512
        image[start:start + min(512, max(0, len(data) - i * 512))] = data[i * 512:(i + 1) * 512]
    return clusters[0]


def entries(name, cluster, size, directory, ordinal):
    short = (f"F{ordinal:07d}" + ("DIR" if directory else "BIN")).encode()
    checksum = 0
    for c in short:
        checksum = (((checksum & 1) << 7) + (checksum >> 1) + c) & 255
    text = [ord(c) for c in name] + [0]
    text += [0xFFFF] * ((13 - len(text) % 13) % 13)
    result = []
    for i in reversed(range(len(text) // 13)):
        entry = bytearray(32)
        entry[0] = i + 1 | (0x40 if i + 1 == len(text) // 13 else 0)
        entry[11] = 0x0F
        entry[13] = checksum
        for pos, codepoint in zip((1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30), text[i * 13:(i + 1) * 13]):
            struct.pack_into("<H", entry, pos, codepoint)
        result.append(entry)
    entry = bytearray(32)
    entry[:11] = short
    entry[11] = 0x10 if directory else 0x20
    struct.pack_into("<H", entry, 20, cluster >> 16)
    struct.pack_into("<H", entry, 26, cluster & 0xFFFF)
    struct.pack_into("<I", entry, 28, size)
    result.append(entry)
    return result


def add(path, data=None):
    parent, _, name = path.rpartition("/")
    if parent not in directories:
        add(parent)
    cluster = allocate(data or b"")
    listing = directories[parent][1]
    listing.extend(entries(name, cluster, len(data or b""), data is None, len(listing) + 1))
    if data is None:
        directories[path] = (cluster, [])


add("atmosphere")
add("switch")
add("switch/bt-autosync-v1")
add("switch/bt-autosync-v1/hekate-continue.bin", Path("output/hekate-continue.bin").read_bytes())
add("switch/bt-autosync-v1/LEEME.txt", b"SYNTHETIC GUIDE PRESERVED")
add("keep.txt", b"UNRELATED FILE MUST REMAIN BYTE EXACT" * 8)
for path, (cluster, listing) in directories.items():
    data = b"".join(listing)
    assert len(data) < 512, (path, len(data))
    start = (1232 + cluster - 2) * 512
    image[start:start + len(data)] = data
for copy in range(2):
    base = (32 + copy * 600) * 512
    for cluster, value in fat.items():
        struct.pack_into("<I", image, base + cluster * 4, value)
Path("output/auto-fatfs.bin").write_bytes(image)
print("Synthetic FAT32/LFN fixture generated")
