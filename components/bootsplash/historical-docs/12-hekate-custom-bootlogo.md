# Phase 5.3 — custom Hekate bootlogo (`logopath=`)

Status: Tooling generated and verified locally. Nothing written to any SD card. This is a
third, independent way to customize what you see during boot — separate from both the
`package3` splash (phase 5.0/5.1) and `JheraldBootSound` itself. No changes to the sysmodule,
its NPDM, or its Program ID/SAC come from this phase.

## What this is, and how it differs from the `package3` splash

Hekate (the CTCaer fork used by this project) has its own, independent way to show a custom
image during boot: a `logopath=` key on a boot entry in `hekate_ipl.ini`, pointing at a 32-bit
BMP file. This is a different mechanism from the official Atmosphère `package3` splash
covered in [`11-custom-boot-splash.md`](11-custom-boot-splash.md) — different file, different
format, different code path, and (importantly, see below) a different point in the boot
sequence where it can actually be seen.

## Reading Hekate's own source, not assuming

Directly from `bootloader/main.c` (`_auto_launch()`) and `bootloader/gfx/gfx.c`
(`gfx_render_bmp_argb()`) in the CTCaer/hekate repository:

- The bitmap is loaded via the boot entry's `logopath=` key, falling back to
  `bootloader/bootlogo.bmp` if that key is absent or the file can't be read.
- The BMP header is validated by hand, at fixed byte offsets: magic `b'BM'` at offset 0–1,
  `biBitCount` (bit depth) must equal exactly 32 at offset 28 — a 24-bit BMP is rejected
  outright — and `biWidth ≤ 720`, `biHeight ≤ 1280` (the screen's native portrait resolution).
- `biHeight` must be positive (standard bottom-up row storage) — the code reads it as an
  unsigned value for the `≤ 1280` check, so a negative (top-down) height would fail validation.
- `gfx_render_bmp_argb()` indexes rows as `buf[(size_y - 1 - row) * size_x + col]`, which is
  exactly the semantics of a standard bottom-up BMP — a normally-encoded 32-bit BMP already
  displays right-side up, no manual row-flipping needed.
- Pixels are copied as raw 32-bit words with no visible blending — byte order is B, G, R, A
  (little-endian), the same convention as a classic 32-bit Windows BMP and the same one
  Atmosphère's own `insert_splash_screen.py` uses for its splash format.
- This whole logo-loading block is only reached when `!(boot_cfg & BOOT_CFG_FROM_LAUNCH) &&
  boot_wait` — i.e., only during the autoboot countdown. Manually selecting an entry from
  the Nyx menu (`Launch`) sets `BOOT_CFG_FROM_LAUNCH`, which skips this block entirely.

This last point matters a lot in practice: if your boot entry doesn't autoboot — if you
always pick it manually from the Nyx menu — a `logopath=` bootlogo you set on it will likely
never be shown, no matter how correctly it's formatted. Autoboot with a nonzero `bootwait`
is what actually puts Hekate into the code path that displays it.

## The screen is a portrait panel internally

Same underlying reason as the `package3` splash: the console's native framebuffer is
720×1280 (portrait), even though what you normally see is a 1280×720 landscape image. A
landscape 1280×720 source has to be rotated 90° counter-clockwise to become the 720×1280 buffer
Hekate expects — this project's tooling follows the exact same rotation convention Atmosphère's
own splash script uses, for consistency.

## Generating your own bootlogo

```sh
python tools/hekate_splash/make_hekate_bootlogo.py my_bootlogo.png my_bootlogo.bmp
```

- Input: a 1280×720 PNG (16:9). Pass `--resize` if your source isn't exactly that size and you
  want it resampled (LANCZOS) rather than have the script refuse to continue.
- Output: a genuine 32-bit BMP, 720×1280, bottom-up, BGRA per pixel, alpha always opaque
  (`0xFF`) — validated against exactly the header fields Hekate itself checks (magic, bit
  depth, dimensions, positive height) before the script reports success.
- No SD card is touched by this tool — it only ever writes the local output file you name.

This project's own testing of this tool produced a bootlogo that, when read back and rotated
90° clockwise (undoing the conversion), compared pixel-exact against the original PNG in
every RGB channel — the source image had no alpha channel to begin with, and the BMP always
writes a constant, fully-opaque alpha, so there's no meaningful alpha difference to report
either.

## Where it goes (once you're ready to install)

```
SD:/bootloader/res/my_bootlogo.bmp
```

with a `logopath=` line added to the relevant boot entry in `hekate_ipl.ini`:

```ini
[Your Atmosphere entry]
pkg3=atmosphere/package3
...
logopath=bootloader/res/my_bootlogo.bmp
```

Back up `hekate_ipl.ini` before editing it — same discipline as everywhere else in this
project: verified, hash-checked backup first, see
[`backup-and-rollback.md`](backup-and-rollback.md). If `bootloader/res/my_bootlogo.bmp` doesn't
already exist, there's nothing to back up at that path — it's simply a new file.

## Autoboot vs. manual `Launch` — decide this before installing anything

Check your own `bootloader/hekate_ipl.ini` `[config]` section for `autoboot=` and
`autoboot_list=`. Two paths, and this project recommends being deliberate about which one you
want rather than assuming:

- Keep manual `Launch` (`autoboot=0`, or the entry isn't the default) — simplest, safest,
  but per the source-code behavior above, the custom bootlogo will most likely never be
  shown, since manual `Launch` skips the code path that reads `logopath=` entirely.
- Enable autoboot for that entry, with a short `bootwait` (Hekate will boot automatically
  once the countdown reaches zero, showing the bootlogo during that wait) — this is what
  actually makes the custom bootlogo visible, at the cost of the console booting on its own
  after a short delay instead of always stopping at the menu.

Changing `autoboot`/`bootwait` is a separate, deliberate decision from adding the `logopath=`
line — don't flip it silently as a side effect of wanting a bootlogo.

## Changing the image later

If you keep the same path (`bootloader/res/my_bootlogo.bmp`) and the same `logopath=` line,
switching to a different image later is just: regenerate the BMP from a new source PNG with
this same tool, and replace the one file on the SD card. `hekate_ipl.ini` doesn't need to be
touched again.

## What this phase does NOT do

- Does not modify `JheraldBootSound`, its config, or its audio files.
- Does not modify the `package3` splash from phase 5.0/5.1 — this is a separate mechanism.
- Does not write anything to any SD card, or edit any `.ini` file.
- Does not silently enable autoboot — that's a decision for you to make deliberately, once you
  understand the trade-off above.
