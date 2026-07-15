#pragma once

#include "../audio_driver.h"

#include "esp_codec_dev.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace sensesp_cockpit_display {

/// Audio-out driver for the Waveshare ESP32-P4-WIFI6-Touch-LCD-7B:
///   - ES8311 mono codec, I2C addr 0x18
///   - NS4150B class-D amplifier, enable on PA_CTRL = GPIO53
///   - I2S: MCLK=13, BCLK/SCLK=12, WS/LCLK=10, DOUT=9, DIN/DSIN=11
///     (production "FIB" mapping, matches Waveshare's own BSP)
///
/// NOTE: pins are verified on the 7B only. The 4B is a different
/// "86-box" board; its ES8311 wiring is UNVERIFIED and may differ.
/// On a 4B with different pins the codec still inits (no crash) but
/// stays silent until the pins are confirmed against the 4B BSP.
///
/// Codec control runs on a dedicated I2C master bus (I2C_NUM_1) on the
/// same SDA=7/SCL=8 pins the GT911 touch uses, but a separate port from
/// Arduino Wire (I2C_NUM_0) so the two drivers never share peripheral
/// state. The codec is configured once at init and then only poked for
/// volume; the touch bus stays hot on port 0.
///
/// play_pcm() copies into a small ring of buffers handed to an internal
/// audio task via a queue, so callers (including the LVGL event_loop
/// task) never block on the I2S write.
class WaveshareAudio : public AudioDriver {
 public:
  static constexpr uint32_t kSampleRate = 16000;

  void init() override;
  bool ready() const override { return ready_; }
  uint32_t sample_rate() const override { return kSampleRate; }
  void play_pcm(const int16_t* samples, size_t frames) override;
  void set_volume(uint8_t pct) override;
  void set_enabled(bool on) override;

 private:
  static void audio_task(void* arg);
  void run();  // audio task body

  i2c_master_bus_handle_t i2c_bus_ = nullptr;
  i2s_chan_handle_t tx_chan_ = nullptr;
  esp_codec_dev_handle_t codec_ = nullptr;
  esp_codec_dev_sample_info_t fs_ = {};
  double vol_pct_ = 50.0;

  QueueHandle_t queue_ = nullptr;  // of Clip (owned buffer + len)
  TaskHandle_t task_ = nullptr;
  volatile bool enabled_ = true;
  bool ready_ = false;
};

}  // namespace sensesp_cockpit_display
