#include "waveshare_audio.h"

#include <cstdlib>
#include <cstring>

#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp32-hal-i2c.h"  // i2cBusHandle() — share Arduino Wire's bus

static const char* TAG = "ws-audio";

// The ES8311 sits on the SAME I2C pins (7/8) as the GT911 touch, which
// the board HAL brings up via Arduino Wire on port 0. We must NOT open a
// second bus on those pins — doing so wedges Wire with ESP_ERR_INVALID_
// STATE and kills touch. Instead we reuse the underlying IDF i2c_master
// handle Arduino created for port 0 (i2cBusHandle), so codec + touch are
// two devices on one shared bus, exactly the i2c_master sharing model.
static constexpr int kI2cPort = 0;

// Shared audio pin map for the Waveshare P4 7B / 4B panels. These match
// the production (FIB) board mapping in Waveshare's own BSP
// (esp32_p4_function_ev_board.h): SCLK=12, MCLK=13, LCLK/WS=10, DOUT=9,
// DSIN=11. An earlier guess swapped WS(=11) and DIN(=10); a wrong WS
// line means the codec never gets valid L/R frame sync → amp hisses but
// emits no tone. Do NOT "simplify" these back to a guessed order.
static constexpr int kI2sMclk = 13;
static constexpr int kI2sBclk = 12;  // SCLK
static constexpr int kI2sWs = 10;    // LCLK / word-select
static constexpr int kI2sDout = 9;   // P4 -> codec (playback)
static constexpr int kI2sDin = 11;   // codec -> P4 (mic; unused for now)
// I2C (SDA=7/SCL=8) is owned by the touch HAL's Arduino Wire; we borrow
// its port-0 bus handle rather than re-declaring the pins here.
static constexpr int kPaCtrl = 53;   // NS4150B enable, active high
// ES8311_CODEC_DEFAULT_ADDR (0x30) is the 8-bit form; the i2c ctrl
// driver right-shifts it to the 7-bit 0x18 the datasheet lists.
static constexpr uint8_t kEs8311Addr = ES8311_CODEC_DEFAULT_ADDR;

// One playback clip in flight: heap buffer of int16 mono samples that
// the audio task owns and frees after writing. Kept small — a chime is
// a fraction of a second at 16 kHz.
namespace {
struct Clip {
  int16_t* samples;
  size_t frames;
};
constexpr int kQueueDepth = 4;
}  // namespace

namespace sensesp_cockpit_display {

void WaveshareAudio::init() {
  // --- Reuse the touch bus (Arduino Wire, port 0). Must run AFTER the
  //     touch HAL's Wire.begin(7,8); main.cpp inits audio after
  //     lvgl_init (which inits touch), so the bus already exists. ---
  if (!i2cIsInit(kI2cPort)) {
    ESP_LOGE(TAG, "I2C port %d not initialised by touch yet — no audio",
             kI2cPort);
    return;
  }
  i2c_bus_ = (i2c_master_bus_handle_t)i2cBusHandle(kI2cPort);
  if (!i2c_bus_) {
    ESP_LOGE(TAG, "i2cBusHandle(%d) null — no audio", kI2cPort);
    return;
  }

  // --- I2S standard mode, TX only, master, provides MCLK. ---
  esp_err_t err;
  i2s_chan_config_t chan_cfg =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  err = i2s_new_channel(&chan_cfg, &tx_chan_, nullptr);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "i2s channel init failed: %s", esp_err_to_name(err));
    return;
  }

  i2s_std_config_t std_cfg = {};
  std_cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(kSampleRate);
  // Stereo slot even though the ES8311 is mono: the codec's I2S
  // interface expects a full L/R frame and takes the left channel;
  // a MONO slot left the DAC starved (hiss, no tone) on real hardware.
  std_cfg.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
      I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  std_cfg.gpio_cfg.mclk = (gpio_num_t)kI2sMclk;
  std_cfg.gpio_cfg.bclk = (gpio_num_t)kI2sBclk;
  std_cfg.gpio_cfg.ws = (gpio_num_t)kI2sWs;
  std_cfg.gpio_cfg.dout = (gpio_num_t)kI2sDout;
  std_cfg.gpio_cfg.din = (gpio_num_t)kI2sDin;
  err = i2s_channel_init_std_mode(tx_chan_, &std_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "i2s std init failed: %s", esp_err_to_name(err));
    return;
  }
  ESP_ERROR_CHECK(i2s_channel_enable(tx_chan_));

  // --- ES8311 codec via esp_codec_dev. It drives PA_CTRL itself
  //     (pa_pin) so the amplifier is only powered while playing. ---
  audio_codec_i2c_cfg_t i2c_ctrl_cfg = {};
  i2c_ctrl_cfg.port = kI2cPort;  // shared with touch; bus_handle is what's used
  i2c_ctrl_cfg.addr = kEs8311Addr;
  i2c_ctrl_cfg.bus_handle = i2c_bus_;
  const audio_codec_ctrl_if_t* ctrl_if = audio_codec_new_i2c_ctrl(&i2c_ctrl_cfg);

  audio_codec_i2s_cfg_t i2s_data_cfg = {};
  i2s_data_cfg.port = I2S_NUM_0;
  i2s_data_cfg.tx_handle = tx_chan_;
  const audio_codec_data_if_t* data_if = audio_codec_new_i2s_data(&i2s_data_cfg);

  const audio_codec_gpio_if_t* gpio_if = audio_codec_new_gpio();

  es8311_codec_cfg_t es_cfg = {};
  es_cfg.ctrl_if = ctrl_if;
  es_cfg.gpio_if = gpio_if;
  es_cfg.codec_mode = ESP_CODEC_DEV_WORK_MODE_DAC;
  es_cfg.pa_pin = kPaCtrl;
  es_cfg.use_mclk = true;
  es_cfg.hw_gain.pa_voltage = 5.0;
  es_cfg.hw_gain.codec_dac_voltage = 3.3;
  const audio_codec_if_t* codec_if = es8311_codec_new(&es_cfg);
  if (!codec_if) {
    ESP_LOGE(TAG, "es8311_codec_new failed");
    return;
  }

  esp_codec_dev_cfg_t dev_cfg = {};
  dev_cfg.dev_type = ESP_CODEC_DEV_TYPE_OUT;
  dev_cfg.codec_if = codec_if;
  dev_cfg.data_if = data_if;
  codec_ = esp_codec_dev_new(&dev_cfg);
  if (!codec_) {
    ESP_LOGE(TAG, "esp_codec_dev_new failed");
    return;
  }

  // Open the codec once and keep it open; per-clip open/close proved
  // unreliable on hardware. The amp is enabled via PA_CTRL (below) and
  // each clip carries a silence tail so the I2S DMA ring settles to
  // zero between beeps — idle is genuinely silent (verified, no hiss).
  fs_.sample_rate = kSampleRate;
  fs_.channel = 2;  // stereo frame; mono clips are duplicated L/R on write
  fs_.bits_per_sample = 16;
  err = esp_codec_dev_open(codec_, &fs_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_codec_dev_open failed: %s", esp_err_to_name(err));
    return;
  }
  esp_codec_dev_set_out_vol(codec_, vol_pct_);

  // Force the NS4150B amp enable HIGH directly, independent of the
  // codec driver's own pa_pin handling.
  gpio_config_t pa_cfg = {};
  pa_cfg.pin_bit_mask = 1ULL << kPaCtrl;
  pa_cfg.mode = GPIO_MODE_OUTPUT;
  gpio_config(&pa_cfg);
  gpio_set_level((gpio_num_t)kPaCtrl, 1);

  // --- Playback plumbing. ---
  queue_ = xQueueCreate(kQueueDepth, sizeof(Clip));
  if (!queue_) {
    ESP_LOGE(TAG, "queue alloc failed");
    return;
  }
  // Stack from internal RAM (task stacks can't live in PSRAM). 4 KB is
  // ample for the write loop; no recursion, no big locals.
  if (xTaskCreate(&WaveshareAudio::audio_task, "audio", 4096, this, 5,
                  &task_) != pdPASS) {
    ESP_LOGE(TAG, "audio task create failed");
    return;
  }

  ready_ = true;
  ESP_LOGI(TAG, "ES8311 audio ready (%lu Hz mono)", (unsigned long)kSampleRate);
}

void WaveshareAudio::play_pcm(const int16_t* samples, size_t frames) {
  if (!ready_ || !enabled_ || !samples || frames == 0) return;

  // Copy into a heap buffer the audio task will own and free. Dropping
  // on a full queue is intentional: a chime is disposable, and the
  // caller (often the LVGL event_loop task) must never block on audio.
  Clip clip;
  clip.frames = frames;
  clip.samples = (int16_t*)malloc(frames * sizeof(int16_t));
  if (!clip.samples) return;
  memcpy(clip.samples, samples, frames * sizeof(int16_t));

  if (xQueueSend(queue_, &clip, 0) != pdPASS) {
    free(clip.samples);
  }
}

void WaveshareAudio::set_volume(uint8_t pct) {
  if (pct > 100) pct = 100;
  vol_pct_ = (double)pct;
  if (codec_) esp_codec_dev_set_out_vol(codec_, vol_pct_);
}

void WaveshareAudio::set_enabled(bool on) {
  // play_pcm() early-outs when disabled, so no new clips are enqueued
  // and the codec simply plays out silence.
  enabled_ = on;
}

void WaveshareAudio::audio_task(void* arg) {
  static_cast<WaveshareAudio*>(arg)->run();
}

void WaveshareAudio::run() {
  Clip clip;
  for (;;) {
    if (xQueueReceive(queue_, &clip, portMAX_DELAY) != pdPASS) continue;
    if (!clip.samples) continue;

    // Codec is opened as 2-channel; duplicate the mono clip into an
    // interleaved L/R buffer so both slots carry the tone. A trailing
    // block of SILENCE is appended: the I2S DMA ring auto-repeats its
    // last buffer when no new data is written, so without a zero tail
    // the end of the clip loops forever as a continuous tone. The
    // silence leaves the ring at zero. Codec is kept open (see init());
    // blocking here is fine (dedicated task).
    size_t n = clip.frames;
    size_t tail = kSampleRate / 10;  // 100 ms of zeros
    size_t total = n + tail;
    int16_t* stereo = (int16_t*)malloc(total * 2 * sizeof(int16_t));
    if (stereo) {
      for (size_t i = 0; i < n; ++i) {
        stereo[2 * i] = clip.samples[i];
        stereo[2 * i + 1] = clip.samples[i];
      }
      memset(stereo + n * 2, 0, tail * 2 * sizeof(int16_t));
      esp_codec_dev_write(codec_, stereo, total * 2 * sizeof(int16_t));
      free(stereo);
    }
    free(clip.samples);
  }
}

}  // namespace sensesp_cockpit_display
