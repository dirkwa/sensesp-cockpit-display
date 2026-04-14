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
  /// Called from the LVGL flush callback.
  virtual void flush(const void* buf) = 0;
};

}  // namespace sensesp_cockpit_display
