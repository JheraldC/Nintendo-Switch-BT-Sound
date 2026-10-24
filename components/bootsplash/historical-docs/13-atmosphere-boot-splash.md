# Phase 5.6 — the second splash (Atmosphère's `boot` sysmodule)

Status: Analysis and local build only. Nothing written to any SD card, nothing installed.
This is a third, independent splash mechanism — distinct from both the `package3` early splash
(phase 5.0/5.1) and Hekate's own `logopath=` bootlogo (phase 5.3). No changes to
`JheraldBootSound` itself come from this phase.

## What this splash is

Atmosphère's `boot` sysmodule (title ID `0x0100000000000005`) shows a small splash image for
about 2 seconds early in the Horizon boot sequence — after `boot2` content (including this
project's own sysmodule) hasn't run yet, but well before the console reaches HOME. Reading its
source directly (`stratosphere/boot/source/boot_splash_screen.cpp`, the exact version matching
the Atmosphère release this project targets):

```cpp
void ShowSplashScreen() {
    const auto boot_reason = GetBootReason();
    if (boot_reason == spl::BootReason_AcOk || boot_reason == spl::BootReason_RtcAlarm2) {
        return;
    }
    InitializeDisplay();
    ShowDisplay(SplashScreenX, SplashScreenY, SplashScreenW, SplashScreenH, SplashScreen);
    os::SleepThread(TimeSpan::FromSeconds(2));
    FinalizeDisplay();
}
```

It's skipped entirely when the boot reason is a charger plug-in or an RTC alarm wake, and also
when running on a specific hardware variant — those conditions were left untouched. Changing
*what* is shown means replacing the compiled-in image data and rebuilding this one sysmodule
against the exact same Atmosphère source; it means recompiling a component of Atmosphère
itself, not something a config file or an external asset swap can do.

## Deriving the image format from source, not assuming it

Two completely different pieces of code render this splash versus the `package3` early splash,
and they use different pixel formats and different orientation conventions — confirmed by
reading `stratosphere/boot/source/boot_display.cpp` directly rather than reusing the earlier
splash's format:

- `ShowDisplay(x, y, width, height, img)` treats `img` as a perfectly normal row-major array —
  `width` columns per row, `height` rows, top-left origin, no rotation needed. The function
  itself handles mapping that onto the console's physical portrait framebuffer internally.
  This is different from both the `package3` splash and the Hekate bootlogo, which both require
  rotating the source image 90° before packing it.
- The pixel format register (`DC_WIN_COLOR_DEPTH`, set to `WIN_COLOR_DEPTH_B8G8R8A8`) confirms
  each pixel is a 32-bit word with bytes ordered B, G, R, A in memory — the same byte order as
  a classic 32-bit Windows BMP and as `insert_splash_screen.py`'s output, even though the
  orientation handling differs.
- `X`/`Y`/`W`/`H` are plain screen coordinates on the normal 1280×720 logical screen — verified
  against the upstream default values, which satisfy exactly `X=(1280-W)/2, Y=(720-H)/2`
  (centered).

Getting either of these wrong (rotating when you shouldn't, or swapping the byte order) would
produce a corrupted or misoriented image rather than a build error — this project's own tooling
verifies its own output against the *original* upstream `.inc` before trusting it on new data
(see below).

## Tooling

```sh
python tools/boot_splash/png_to_boot_inc.py my_boot_splash.png my_boot_splash.inc
python tools/boot_splash/boot_inc_to_png.py my_boot_splash.inc preview.png
```

`png_to_boot_inc.py` takes any PNG and produces a `.inc` with exactly the four `constexpr`
declarations and the pixel array the real source file expects, centered on the 1280×720 screen.
`boot_inc_to_png.py` is the literal inverse — it parses an existing `.inc` (upstream or custom)
back into a PNG, with no need to compile anything, so you can check what's actually in a `.inc`
before it goes anywhere near a build.

This project's own testing ran the inverse tool against the unmodified upstream `.inc`
first — reconstructing Atmosphère's own splash image from its shipped source data — as the
first sanity check that the format was understood correctly, before trusting the tool on any
new image. Every generated `.inc` was then round-tripped back through the inverse tool and
compared pixel-by-pixel against its source PNG.

## Package3's fixed KIP budget — why size matters here

`boot.kip` doesn't get its own arbitrary amount of space. Reading Atmosphère's own
`fusee/build_package3.py` directly: seven system KIPs plus the emuMMC KIP all share one
fixed 3 MiB (0x300000-byte) region inside `package3` — however much any one of them uses is
space taken away from the shared budget, not from some separate per-file allowance. Growing the
splash embedded in `boot.kip` grows `boot.kip` itself, which eats into that same shared 3 MiB
alongside `Loader`, `NCM`, `ProcessManager`, `sm`, `spl`, `ams_mitm`, and `emummc` — a
1280×720 splash (raw, ~3.6 MB) does not come close to fitting; a real safe maximum is
meaningfully smaller than the full screen. This project measured its own real, current KIP
sizes directly from its own installed `package3`'s own header (the exact same file format
`insert_splash_screen.py` and `build_package3.py` both target) to get an actual, not assumed,
headroom figure before choosing a size — "measure, don't guess" applied literally.

## What this phase does NOT do

- Does not change `JheraldBootSound`, its `config.ini`, its audio files, or its NPDM.
- Does not touch the `package3` early splash or Hekate's `logopath=` bootlogo — both separate,
  unaffected mechanisms.
- Does not install anything on any SD card — everything here is local build and verification
  only.
- Does not silently pick a resolution beyond what was actually measured to fit; a resolution
  believed unsafe based on the real headroom calculation was not built or proposed for
  installation.
