#pragma once

#include "lvgl.h"
#include "../hal/display_driver.h"
#include "../hal/touch_driver.h"

namespace sensesp_cockpit_display {

/// Initialize LVGL, wire display driver and touch driver as LVGL devices.
/// Must be called once from setup(), before any LVGL widgets are created.
void lvgl_init(DisplayDriver* display, TouchDriver* touch);

/// Call from the main event loop to drive LVGL rendering.
/// Typical usage: event_loop()->onRepeat(16, lvgl_tick);
void lvgl_tick();

}  // namespace sensesp_cockpit_display
