#pragma once

#include "../display_driver.h"
#include "../touch_driver.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"

namespace sensesp_cockpit_display {

/// Display driver for the Waveshare ESP32-P4-WIFI6-Touch-LCD-7B.
/// EK79007 panel controller, 1024x600, 2-lane MIPI-DSI, RGB565.
class Waveshare7BDisplay : public DisplayDriver {
 public:
  static constexpr uint16_t kWidth = 1024;
  static constexpr uint16_t kHeight = 600;
  static constexpr int kBytesPerPixel = 2;
  static constexpr int kNumBuffers = 2;
  static constexpr size_t kBufferSize = kWidth * kHeight * kBytesPerPixel;

  void init() override;
  uint16_t width() const override { return kWidth; }
  uint16_t height() const override { return kHeight; }
  void* get_draw_buffer(int index) override;
  size_t get_draw_buffer_size() override { return kBufferSize; }
  void flush(int x, int y, int w, int h, const void* buf) override;

 private:
  void init_ldo();
  void init_backlight();

  esp_lcd_dsi_bus_handle_t dsi_bus_ = nullptr;
  esp_lcd_panel_io_handle_t dbi_io_ = nullptr;
  esp_lcd_panel_handle_t panel_ = nullptr;
  void* framebuffers_[kNumBuffers] = {};
};

/// Touch driver for the GT911 on the Waveshare 7B board.
/// Uses Arduino Wire library on SDA=7, SCL=8.
class Waveshare7BTouch : public TouchDriver {
 public:
  void init() override;
  TouchPoint read() override;

 private:
  bool initialized_ = false;
};

}  // namespace sensesp_cockpit_display
