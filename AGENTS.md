# sensesp-cockpit-display

Shared HAL + LVGL plumbing library used by the cockpit firmware
([sensesp-p4-cockpit](https://github.com/dirkwa/sensesp-p4-cockpit)).

Two halves:

- **HAL** (`src/sensesp_cockpit_display/hal/`): per-board display +
  touch drivers. Today: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B (EK79007
  MIPI-DSI + GT911 capacitive touch). Add a board by implementing
  `DisplayDriver` + `TouchDriver`.
- **LVGL glue** (`src/sensesp_cockpit_display/lvgl/`): `lvgl_init`
  wires the HAL into LVGL's display + indev infrastructure, owns the
  partial-render buffers, and runs the 1 ms tick via `esp_timer`.

Consumers wire it in via PlatformIO `lib_deps` symlink; the firmware
calls `lvgl_init(new Waveshare7BDisplay(), new Waveshare7BTouch())`
once at boot and from then on touches and renders flow through LVGL
normally.

## Architecture invariants

1. **DisplayDriver is the only LVGL-aware HAL surface.** Boards
   implement `init`, `flush(x,y,w,h,buf)`, `set_brightness(pct)`,
   `set_display_on(bool)`. No board-specific code escapes the driver.
2. **TouchDriver returns the last press coordinates on release.**
   LVGL distinguishes click from drag by comparing press and release
   coords; returning `(0,0)` on release breaks `LV_EVENT_CLICKED` for
   any widget that isn't centred on the origin. Every board's
   `read()` must keep the last point alive between presses.
3. **Backlight inversion lives in software.** Waveshare's LEDC config
   uses `output_invert=1` electrically, but the perceived brightness
   on this board maps directly to LEDC duty — `set_brightness(0)`
   really is dark and `set_brightness(100)` is bright. Don't reintroduce
   the duty-inversion math that earlier (buggy) firmware did.

## Where to start reading

| File                                                        | Why                                       |
|-------------------------------------------------------------|-------------------------------------------|
| [src/sensesp_cockpit_display/hal/display_driver.h](src/sensesp_cockpit_display/hal/display_driver.h) | `DisplayDriver` interface |
| [src/sensesp_cockpit_display/hal/touch_driver.h](src/sensesp_cockpit_display/hal/touch_driver.h)   | `TouchDriver` interface  |
| [src/sensesp_cockpit_display/hal/boards/waveshare_7b.cpp](src/sensesp_cockpit_display/hal/boards/waveshare_7b.cpp) | Reference HAL — EK79007 + GT911 |
| [src/sensesp_cockpit_display/lvgl/lv_drivers.cpp](src/sensesp_cockpit_display/lvgl/lv_drivers.cpp) | `lvgl_init`, indev wiring  |

## Build / test

This library doesn't ship its own build target. Consumers exercise it
via their own PlatformIO project; the reproducer at
[backlight-touch-test](https://github.com/dirkwa/backlight-touch-test)
(if published) is the minimal smoke test.

## Repo conventions

- **Commits**: focused, atomic. Subject ≤ 50 chars, imperative.
- **Never auto-commit, never auto-push.** Only when the user asks.
- **No release-flow work** (version bumps, tags) unless the user says
  release.
- **No AI attribution anywhere.** No "Co-Authored-By", no CLAUDE.md
  content in the body, no AI-tool mentions in commits / PRs / code.
- **Comments**: WHY only. No echo comments, no rot bait.
