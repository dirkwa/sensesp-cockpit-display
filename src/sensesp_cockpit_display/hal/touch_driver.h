#pragma once

#include <cstdint>

namespace sensesp_cockpit_display {

class TouchDriver {
 public:
  virtual ~TouchDriver() = default;

  virtual void init() = 0;

  struct TouchPoint {
    uint16_t x;
    uint16_t y;
    bool pressed;
  };

  /// Read the current touch state. Called from the LVGL indev callback.
  virtual TouchPoint read() = 0;
};

}  // namespace sensesp_cockpit_display
