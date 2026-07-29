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

  bool begin_stream(uint32_t rate, uint8_t bits, uint8_t channels) override;
  size_t write_stream(const int16_t* samples, size_t frames) override;
  void end_stream() override;

  bool can_capture() const override { return capture_ready_; }
  uint32_t capture_rate() const override { return kSampleRate; }
  size_t record_pcm(int16_t* out, size_t max_frames) override;
  void start_capture() override;
  void stop_capture() override;

 private:
  static void audio_task(void* arg);
  void run();  // audio task body

  // Reopen the codec at `rate` Hz if it isn't already. Serialised by
  // codec_mutex_. Returns false on codec error. Used by both the chime
  // path (kSampleRate) and streaming (audio-start's rate).
  bool ensure_rate(uint32_t rate);

  i2c_master_bus_handle_t i2c_bus_ = nullptr;
  i2s_chan_handle_t tx_chan_ = nullptr;
  i2s_chan_handle_t rx_chan_ = nullptr;  // codec ADC -> P4 (mic)
  esp_codec_dev_handle_t codec_ = nullptr;    // OUT (DAC / speaker)
  esp_codec_dev_handle_t codec_in_ = nullptr;  // IN (ADC / mic)
  esp_codec_dev_sample_info_t fs_ = {};
  esp_codec_dev_sample_info_t fs_in_ = {};
  bool capture_ready_ = false;    // ADC brought up at init
  volatile bool capturing_ = false;  // codec_in_ currently open
  double vol_pct_ = 50.0;
  uint32_t open_rate_ = 0;  // rate the codec is currently opened at

  QueueHandle_t queue_ = nullptr;  // of Clip (owned buffer + len)
  TaskHandle_t task_ = nullptr;
  volatile bool enabled_ = true;
  bool ready_ = false;

  // Serialises codec open/close/write between the chime audio_task and a
  // streaming caller (the Wyoming socket task). A stream and a chime never
  // play at once: streaming_ makes play_pcm drop while a stream is active.
  SemaphoreHandle_t codec_mutex_ = nullptr;
  volatile bool streaming_ = false;
  // Reusable interleave buffer for streaming L/R duplication (avoids a
  // malloc per chunk). Grown on demand; freed at end_stream / never huge.
  int16_t* stream_stereo_ = nullptr;
  size_t stream_stereo_frames_ = 0;
};

}  // namespace sensesp_cockpit_display
