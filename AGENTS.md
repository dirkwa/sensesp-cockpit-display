# sensesp-cockpit-display

Shared library used by [sensesp-p4-cockpit](https://github.com/dirkwa/sensesp-p4-cockpit)
— bundles the per-board display + touch HAL, LVGL init, an HTTP OTA
endpoint, and a TCP remote-log forwarder behind one PlatformIO
library so each downstream firmware doesn't have to re-implement them.

## What's in here

- **`hal/`** — `DisplayDriver` + `TouchDriver` interfaces and
  per-board implementations. Boards:
  - **Waveshare ESP32-P4-WIFI6-Touch-LCD-7B** — EK79007 MIPI-DSI,
    1024×600, GT911 touch.
  - **Waveshare ESP32-P4-WIFI6-Touch-LCD-4B** — ST7703 MIPI-DSI,
    720×720, GT911 touch (needs two LDO rails vs the 7B's one).

  Add a board by implementing both interfaces.
- **`lvgl/`** — `lvgl_init(display, touch)` wires the HAL into LVGL's
  display + indev infrastructure, owns the partial-render buffers,
  and runs the 1 ms tick via `esp_timer`.
- **`net/http_ota.{h,cpp}`** — HTTP OTA server. `POST /update` with
  the firmware binary; verify, flash, reboot.
- **`net/remote_log.{h,cpp}`** — TCP log forwarder. `nc <ip> 2323`
  streams the ESP-IDF log to stdout when serial isn't plugged in.
- **`ui/`** — Legacy cockpit UI framework (pages, widgets, theme)
  that predates JLP. Still compiled but new firmware should drive
  LVGL directly.
- **`data/`** — Legacy SK ↔ widget bindings (toggle, button, gauge)
  for the pre-JLP cockpit. Same caveat as `ui/`.

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
3. **Backlight LEDC duty maps directly to brightness on the
   Waveshare 7B**, even though the channel is configured with
   `output_invert=1`. `set_brightness(0)` is dark, `set_brightness(100)`
   is bright. Don't reintroduce the duty-inversion math (it was
   wrong; the buggy form gave "ON looks dark, OFF looks bright" on
   real hardware).

## Where to start reading

| File                                                        | Why                                       |
|-------------------------------------------------------------|-------------------------------------------|
| [src/sensesp_cockpit_display/hal/display_driver.h](src/sensesp_cockpit_display/hal/display_driver.h) | `DisplayDriver` interface |
| [src/sensesp_cockpit_display/hal/touch_driver.h](src/sensesp_cockpit_display/hal/touch_driver.h)   | `TouchDriver` interface  |
| [src/sensesp_cockpit_display/hal/boards/waveshare_7b.cpp](src/sensesp_cockpit_display/hal/boards/waveshare_7b.cpp) | Reference HAL — EK79007 + GT911 |
| [src/sensesp_cockpit_display/lvgl/lv_drivers.cpp](src/sensesp_cockpit_display/lvgl/lv_drivers.cpp) | `lvgl_init`, indev wiring  |
| [src/sensesp_cockpit_display/net/http_ota.cpp](src/sensesp_cockpit_display/net/http_ota.cpp)       | HTTP OTA `POST /update`  |
| [src/sensesp_cockpit_display/net/remote_log.cpp](src/sensesp_cockpit_display/net/remote_log.cpp)   | TCP log forwarder        |

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
