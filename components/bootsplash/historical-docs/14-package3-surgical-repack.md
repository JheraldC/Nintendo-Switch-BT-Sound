# Phase 5.7 — surgical `package3` repacking (replace one KIP, not the whole build)

Status: Tooling built and verified locally against a real, hardware-tested `package3`.
Nothing installed on any SD card. This is a third approach to customizing the second
splash (see [`13-atmosphere-boot-splash.md`](13-atmosphere-boot-splash.md)) — instead of
rebuilding all of Atmosphère from source just to change one KIP, patch the one component that
actually changed directly inside an existing, working `package3`.

## Why this instead of a full rebuild

Phase 5.6 already proved a custom `boot.kip` compiles and fits the KIP-region budget with real
measured numbers. But assembling a complete `package3` from scratch means also building
`exosphere`, `warmboot`, `mesosphere`, `mariko_fatal`, `rebootstub`, and every other KIP — none
of which changed. Every one of those components is already sitting, compiled and
hardware-tested, inside your current `package3`. Extracting them and reusing their exact bytes
is both faster and safer than re-deriving them from source and hoping the rebuild matches.

## The format — derived from Atmosphère's own builder, not guessed

`tools/package3/package3_format.py` documents the `package3` (`PK31`) layout, reverse-derived
by reading Atmosphère's own `fusee/build_package3.py` (not copied — this is an independent
implementation of a documented data layout): a header up to `0x800` containing content
metadata (offset/size/type per component) and per-KIP metadata (program ID/offset/size/SHA-256
hash), a fixed 0x300000-byte KIP region starting at `0x100000` holding `emummc` plus seven
system KIPs in a known order, and everything else (early splash, `fusee`, `rebootstub`) as
fixed regions the repacker never touches.

`tools/package3/replace_boot_kip.py`:

```sh
python replace_boot_kip.py package3.base my_custom.kip package3.custom
```

- Confirms the KIP's embedded Program ID is unchanged before doing anything else — refuses to
  proceed otherwise.
- Extracts every component from the region unchanged, substitutes only the one KIP you're
  replacing, and reconstructs the region with the exact same `0x10`-byte alignment (`0xCC`
  padding) the official builder uses.
- Patches only the header entries that actually changed (the replaced KIP's size, and the
  offset of anything after it that got pushed forward) — every other byte of the header, and
  every byte outside the KIP region, is copied verbatim from the source `package3`.

## Verifying the result, not just trusting it

Every non-replaced component is extracted from both the original and the patched `package3`
and compared byte-for-byte — they must be identical. The replaced KIP's own hash is checked
against the binary you meant to insert. The full header is re-parsed from the patched file to
confirm no overlaps, everything stays in bounds, and the KIP count/names are unchanged. A full
byte-level diff of the entire 8 MiB file confirms differences only ever land inside the header
metadata or the KIP region — never in the early splash or the `fusee`/`rebootstub` tail.

## Extracting a splash back out of a *compiled* KIP — not just the source

The deepest verification this phase did: don't just trust that the `.inc` you compiled is what
ended up in the binary — extract it back out of the actual compressed KIP and compare.

KIP segments (`.text`/`.rodata`/`.data`) are optionally BLZ-compressed (a Nintendo-specific
"backward LZ" scheme). `tools/package3/blz.py` implements the decompressor — not invented, and
specifically not based on the reference decoder in `switch-tools`' own `blz.c`, because that
function is entirely commented out and never actually called anywhere in that codebase (checked
directly — zero real call sites). Instead it's a faithful Python 3 port of
`kip1_blz_decompress()` from Atmosphère's own `utilities/nxo64.py` — a function that *is*
actively used, under its own ISC-style license distinct from the rest of that GPLv2 repository
(reproduced in full at the top of `blz.py`, exactly as that license requires). Before trusting
it, it was validated by decompressing all three segments of a real compiled KIP and confirming
each came out to *exactly* the size declared in the KIP's own header — and that decompressed
`.text` begins with a valid instruction and ends in a `RET` followed by padding, not garbage.

`tools/package3/kip_extract.py` + `verify_second_splash.py` put this together: decompress
`.rodata`, search it for the exact pixel-array byte pattern generated from your source `.inc`,
and — if found — rebuild it as a PNG and compare pixel-for-pixel against your original image.
This project's own test of this tooling found the pattern at a single, unambiguous offset
inside the real compiled binary, byte-for-byte identical, reconstructing to a pixel-exact match
of the source PNG.

## What this phase does NOT publish

`package3` files (original or patched), extracted KIPs, and anything derived from a
copyrighted splash image are never published — same reasoning as
[`11-custom-boot-splash.md`](11-custom-boot-splash.md). This page documents the *technique* and
ships the *tooling*; bring your own `package3` and your own splash content.

## What this phase does NOT do

- Does not recompile `exosphere`, `mesosphere`, `emummc`, or any KIP other than the one you're
  replacing — their exact existing bytes are reused as-is.
- Does not touch `JheraldBootSound` itself, Hekate's configuration, or anything on an SD card.
- Does not skip verification — every non-replaced component is checked for byte-for-byte
  identity, not assumed to be unaffected.
