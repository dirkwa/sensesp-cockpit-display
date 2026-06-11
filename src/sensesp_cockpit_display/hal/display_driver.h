#pragma once

#include <cstddef>
#include <cstdint>

namespace sensesp_cockpit_display {

class DisplayDriver {
 public:
  virtual ~DisplayDriver() = default;

  virtual void init() = 0;
  virtual uint16_t width() const = 0;
  virtual uint16_t height() const = 0;

  /// Get a DMA-capable framebuffer for LVGL direct rendering.
  virtual void* get_draw_buffer(int index) = 0;
  virtual size_t get_draw_buffer_size() = 0;

  /// Push the framebuffer content to the display hardware.
  /// Called from the LVGL flush callback. For partial rendering,
  /// (x, y, w, h) define the rectangle and `buf` is the RGB565
  /// pixel data for just that rectangle.
  virtual void flush(int x, int y, int w, int h, const void* buf) = 0;

  /// Set the backlight brightness as a percentage (0-100). 0 = off.
  /// Default no-op; boards with a controllable backlight override.
  virtual void set_brightness(uint8_t /*pct*/) {}

  /// Put the panel into sleep / out of sleep. When off, the panel
  /// stops driving sync signals — typically combined with backlight
  /// off for full power save. Restoring requires a redraw. Default
  /// no-op; boards that support it override.
  virtual void set_display_on(bool /*on*/) {}
};

}  // namespace sensesp_cockpit_display
