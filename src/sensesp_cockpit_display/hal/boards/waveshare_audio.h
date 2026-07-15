#pragma once

#include "../audio_driver.h"

#include "esp_codec_dev.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

namespace sensesp_cockpit_display {

/// Audio-out driver for the Waveshare ESP32-P4-WIFI6-Touch-LCD-7B
/// (ES8311 codec + NS4150B amp). Pin map lives in the .cpp.
///
/// The codec shares the GT911 touch's I2C bus (it sits on the same
/// SDA/SCL pins) rather than opening a second bus — a second bus on
/// those pins wedges the touch controller.
///
/// NOTE: pins are verified on the 7B only. The 4B is a different
/// "86-box" board whose ES8311 wiring is UNVERIFIED; there the codec
/// still inits but may stay silent until the 4B pins are confirmed.
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
