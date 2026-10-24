# Supported selective settings profile

The authenticated reader opens only SYSTEM/save/8000000000000050 and its
logical /file. Accepted: size 0x80000, version 0x220000, flags 7, zero reserved
header bytes. Identified in firmware 23.0.1 program analysis and validated
by selective extraction on the development console. Firmware, decrypted
code, keys, settings savedata and controller records are not distributed.

Allowed logical reads: 16-byte profile header at 0; four-byte count at 0x8060,
capped at 24; 512-byte records in two tables, first 10 at 0x8070, remaining
14 at 0x2AAA0. Maximum logical bytes 12308. These are NOT raw container
offsets. Lower authentication reads backing blocks transiently, potentially
including other settings, but never exports complete savedata. CMAC/IVFC/SHA
checks remain enabled. Unknown profiles stop instead of scanning or guessing.

Record layout reference:
https://github.com/switchbrew/libnx/blob/master/nx/include/switch/services/set.h

Atmosphere's external DB is a little-endian u64 count followed by 512-byte
records. Selection requires explicit Pro Controller / Joy-Con names, HID
class and a present nonzero link key. Generic devices matching that shape
can be selected; names do not prove hardware authenticity. Audio and unknown
names are excluded. Duplicates and malformed lengths/flags are rejected.

Original NAND writes are denied. SD writes intentionally operate only through
the guarded transaction after Original eMMC is disconnected. Source NAND
read-only status must not be confused with the SD destination. Physical
manual connectivity was confirmed on one console; automatic new-device
import and additional models remain separate physical acceptance tests.
Host tests use synthetic media and mock AES/CMAC, not new hardware evidence.
