# Phase 5.0 — custom Atmosphère boot splash

Status: Generated and verified locally. `SD:/atmosphere/package3` NOT modified. This is
completely independent of the `JheraldBootSound` sysmodule itself — `0.4.0-fase4.0` is
unchanged by this phase (no NPDM, Program ID, SAC, or binary changes).

## Two different splashes — don't confuse them

Atmosphère has two unrelated splash mechanisms:

- (A) The official `package3` splash — a 1280×720 image, officially supported and inserted
  via `insert_splash_screen.py`. This is what this phase covers.
- (B) The `boot` sysmodule's own splash (a smaller, different logo shown for roughly 2
  seconds early in boot) — a separate mechanism, not touched by this phase. If the
  `package3` splash turns out to disappear before `JheraldBootSound`'s own audio starts, that
  second mechanism is a candidate for a future, separate phase — not assumed or promised here.

## What `insert_splash_screen.py` actually does

Atmosphère's own official utility (obtained from its own repository, matching the exact
Atmosphère version this project targets — see the "Tested compatibility" section in the main
[`README.md`](../README.md)). Reading its logic directly (not assumed):

- Takes a PNG and a `package3` file.
- If the PNG is 1280×720 (landscape), it's rotated 90° internally — the console's own splash
  buffer is stored in portrait orientation, 720×1280.
- Converted to a raw BGRA buffer, one row per output line, with padding to match the buffer's
  internal stride. Total: exactly `0x3C0000` bytes.
- Validates the `package3` magic (`PK31`) and total size (`0x800000` = 8 MiB) before writing.
- Writes back: `package3[:0x400000]` (unchanged) + the converted splash + `package3[0x7C0000:]`
  (unchanged). The splash occupies exactly the byte range `0x400000`–`0x7BFFFF` — nothing
  outside that range is ever touched.

## Generic workflow

```
1. Back up your own package3 (see backup-and-rollback.md) — verify its hash first.
2. Obtain insert_splash_screen.py matching YOUR installed Atmosphère version — from
   Atmosphère's own repository, not an arbitrary/unrelated copy.
3. Work on a LOCAL COPY of your own package3, never the one on the SD card directly:
       your_package3  ->  copy  ->  insert_splash_screen.py your_splash.png copy
4. Verify the result before going anywhere near the SD card:
       - size still 0x800000, magic still "PK31"
       - only 0x400000-0x7BFFFF differs from your backup; everything else is byte-identical
       - SHA-256 of the patched copy differs from the original (expected)
5. Only after verification: copy the patched file back to SD:/atmosphere/package3.
```

Never replace your own `package3` with someone else's / an official download. Your
installation may carry environment-specific differences — always patch a copy of *your own*
`package3`, taken from your own SD card.

`package3` itself is never published or distributed by this project — provide your own from
your own installation. Only the splash image and the (from-scratch, original) tooling around it
are shared here.

## The provided splash image

`assets/splash/JheraldBootSound_splash.png` — 1280×720, generated from scratch with
`scripts/generate_splash.py` (Pillow + a font already installed on the machine that generates
it — no font files are bundled in this repository). Minimalist dark design: a near-black
background, a very subtle violet glow, the project name, and a small subtitle. No Nintendo
branding, no Atmosphère branding, no copied assets — an original, replaceable placeholder. Feel
free to regenerate it with your own text/colors, or replace it with your own artwork entirely
(same 1280×720 PNG requirement).

### Trying more than one candidate before choosing

The workflow above (backup → patch a local copy → verify → decide) works exactly the same for
comparing several candidate splashes before settling on one — patch a separate local copy of
your backed-up `package3` per candidate (never chain one patched copy into the next; always
start each one from the same untouched backup), run the binary-range and roundtrip checks on
each, and only then copy your final choice to the SD card. This project's own development used
exactly that process to compare multiple candidates locally.

One thing worth saying explicitly: don't use official game artwork, logos, or other
copyrighted/trademarked material you don't have rights to — even for a splash that only you
will see, and especially never in anything you publish or share. This repository only ever
ships its own original placeholder design for that reason; swap in your own artwork, not
someone else's copyrighted work.

## Verifying your own patched `package3` (roundtrip)

`scripts/verify_splash.py` performs the exact inverse of the insertion — it extracts
`package3[0x400000:0x7C0000]`, reconstructs the internal 720×1280 buffer, and rotates it back
to 1280×720, so you can visually confirm what actually got embedded before touching your SD
card:

```sh
python scripts/verify_splash.py path/to/package3 preview.png
```

`preview.png` should look identical to the image you passed to `insert_splash_screen.py`. This
project's own verification of this exact process produced a pixel-exact roundtrip (no
difference in any channel, any pixel) against the generated splash — confirming the insertion
and extraction logic agree exactly with each other and with the official script's own layout.

## Timing — don't expect exact synchronization

This is Atmosphère/fusée's own early splash mechanism, shown before `JheraldBootSound` (or any
`boot2` content) ever runs. This project makes no claim that the splash stays visible until
`JheraldBootSound`'s audio begins. Based on phase 3.3/4.0 hardware measurements, the audio
service typically becomes available roughly 1.6 seconds after `JheraldBootSound` starts, with
playback beginning almost immediately after — but the actual visual relationship between the
splash and the audio has not yet been confirmed on hardware for this phase, and depends on
factors this project doesn't control (console model, boot method, SD speed). A dedicated
hardware test of phase 5.0 will determine the real timing relationship, not a promise made here.

## Updating Atmosphère

Atmosphère updates normally replace `package3` outright. After updating Atmosphère, the
custom splash is gone and must be re-applied to the new `package3` using the matching version
of `insert_splash_screen.py` — treat this as a one-time customization per Atmosphère version,
not something that survives updates automatically.

## What this phase does NOT do

- Does not modify `JheraldBootSound` in any way (binary, NPDM, config, or audio files).
- Does not install anything on a real SD card yet — everything above happens on local working
  copies only.
- Does not touch the `boot` sysmodule's own splash (mechanism B above).
