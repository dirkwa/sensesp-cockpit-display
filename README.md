# sensesp-cockpit-display

Shared library used by [sensesp-p4-cockpit](https://github.com/dirkwa/sensesp-p4-cockpit) — the ESP32-P4 cockpit firmware that runs the JSON Layout Player (JLP).

Wraps the per-board display + touch HAL, LVGL initialisation, an HTTP OTA endpoint, and a TCP remote-log forwarder behind a single PlatformIO library so each downstream firmware doesn't have to re-implement them.

## What's in here

| Path                                                | Purpose                                                                 |
|-----------------------------------------------------|-------------------------------------------------------------------------|
| `src/sensesp_cockpit_display/hal/`                  | `DisplayDriver` + `TouchDriver` interfaces and per-board implementations. Today: Waveshare ESP32-P4-WIFI6-Touch-LCD-7B (EK79007 MIPI-DSI + GT911 capacitive touch). Add a board by implementing both interfaces. |
| `src/sensesp_cockpit_display/lvgl/`                 | `lvgl_init(display, touch)` wires the HAL into LVGL's display + indev infrastructure, owns the partial-render buffers, and runs the 1 ms tick via `esp_timer`. |
| `src/sensesp_cockpit_display/net/http_ota.{h,cpp}`  | HTTP OTA server. `POST /update` with the firmware binary; the device verifies, flashes, and reboots into the new image. |
| `src/sensesp_cockpit_display/net/remote_log.{h,cpp}`| TCP log forwarder. `nc <device-ip> 2323` streams the ESP-IDF log to stdout — replaces serial when the cable's unplugged. |
| `src/sensesp_cockpit_display/ui/`                   | Legacy cockpit UI (pages, widgets, theme) that predates JLP. Still compiled but new firmware should drive LVGL directly. |
| `src/sensesp_cockpit_display/data/`                 | Legacy SK ↔ widget bindings (toggle, button, gauge) for the pre-JLP cockpit. Same caveat as `ui/`. |

## Architecture invariants

1. **`DisplayDriver` is the only LVGL-aware HAL surface.** Boards implement `init`, `flush(x, y, w, h, buf)`, `set_brightness(pct)`, `set_display_on(bool)`. No board-specific code escapes the driver.
2. **`TouchDriver::read()` returns the last press coordinates on release.** LVGL distinguishes a click from a drag by comparing press vs. release coords; returning `(0, 0)` on release breaks `LV_EVENT_CLICKED` for any widget that isn't centred on the origin. Every board's `read()` must keep the last point alive between presses.
3. **Backlight LEDC duty maps directly to brightness on the Waveshare 7B**, even though the channel is configured with `output_invert = 1`. `set_brightness(0)` is dark, `set_brightness(100)` is bright. Don't reintroduce the inversion math (it was wrong; pre-fix produced "ON looks dark, OFF looks bright" on real hardware).

## Using it

In a downstream PlatformIO project:

```ini
lib_deps =
    symlink://../sensesp-cockpit-display
    lvgl/lvgl@^9.2
```

```cpp
#include "sensesp_cockpit_display/hal/boards/waveshare_7b.h"
#include "sensesp_cockpit_display/lvgl/lv_drivers.h"

using namespace sensesp_cockpit_display;

void setup() {
  auto* display = new Waveshare7BDisplay();
  auto* touch   = new Waveshare7BTouch();
  lvgl_init(display, touch);

  // From here on the standard LVGL API works.
  lv_obj_t* lbl = lv_label_create(lv_screen_active());
  lv_label_set_text(lbl, "Hello cockpit");
  lv_obj_center(lbl);
}

void loop() {
  lv_timer_handler();
  delay(5);
}
```

A minimal reproducer that also exercises the backlight + touch is the standalone [backlight-touch-test](https://github.com/dirkwa/backlight-touch-test) project (when published).

## Build / test

This library doesn't ship its own build target — it's compiled by whatever PIO project pulls it in. The reference downstream is [sensesp-p4-cockpit](https://github.com/dirkwa/sensesp-p4-cockpit).

## License

MIT.
