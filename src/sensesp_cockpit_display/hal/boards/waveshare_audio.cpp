#include "waveshare_audio.h"

#include <climits>
#include <cstdlib>
#include <cstring>

#include "esp_codec_dev_defaults.h"
#include "es7210_adc.h"  // dual-mic capture codec (U17); mics are on this, not ES8311
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
static constexpr int kI2sDin = 11;   // ES7210 SDOUT -> P4 (mic capture)
// I2C (SDA=7/SCL=8) is owned by the touch HAL's Arduino Wire; we borrow
// its port-0 bus handle rather than re-declaring the pins here.
static constexpr int kPaCtrl = 53;   // NS4150B enable, active high
// ES8311_CODEC_DEFAULT_ADDR (0x30) is the 8-bit form; the i2c ctrl
// driver right-shifts it to the 7-bit 0x18 the datasheet lists.
static constexpr uint8_t kEs8311Addr = ES8311_CODEC_DEFAULT_ADDR;

static constexpr int kMclkMultiple = 384;

// Mic capture: the two onboard mics are wired to the ES7210 ADC (U17), NOT
// the ES8311 — confirmed on the 7B schematic (ES8311 MIC input is unpopulated;
// the ES7210's SDOUT1/TDMOUT = GPIO11/ASDOUT is our I2S DIN via R144). ES7210
// I2C addr is 8-bit 0x80 (7-bit 0x40, A0/A1 to GND on the board); the P4 is
// I2S master so the ES7210 is a slave.
// Capture MIC1 ONLY: MIC2 is dead on this board (flatlines at ~2 counts), and
// selecting MIC1|MIC2 into a mono stream buries the live MIC1 voice — the
// capture level collapses. Verified on hardware: MIC1 tracks speech, MIC2
// does not.
static constexpr uint8_t kEs7210Addr = ES7210_CODEC_DEFAULT_ADDR;  // 0x80
static constexpr uint8_t kEs7210MicSel = ES7210_SEL_MIC1;

// One playback clip in flight: heap buffer of int16 mono samples that
// the audio task owns and frees after writing. Kept small — a chime is
// a fraction of a second at 16 kHz.
namespace {
// Upper bound on a single clip: 5 s of mono frames at the codec rate.
// Far above any alert tone, well below the size where frames*channels*
// bytes could overflow size_t.
constexpr size_t kMaxClipFrames =
    5 * sensesp_cockpit_display::WaveshareAudio::kSampleRate;
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

  // --- I2S standard mode, master, provides MCLK. Full-duplex on one port:
  //     TX drives the ES8311 DAC (speaker); RX carries the ES7210 ADC's
  //     SDOUT (the onboard mics). Both share MCLK/BCLK/WS. ---
  esp_err_t err;
  i2s_chan_config_t chan_cfg =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;  // clear stale DMA data (matches Waveshare ref)
  err = i2s_new_channel(&chan_cfg, &tx_chan_, &rx_chan_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "i2s channel init failed: %s", esp_err_to_name(err));
    return;
  }

  i2s_std_config_t std_cfg = {};
  std_cfg.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(kSampleRate);
  std_cfg.clk_cfg.mclk_multiple = (i2s_mclk_multiple_t)kMclkMultiple;
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
  err = i2s_channel_enable(tx_chan_);
  if (err != ESP_OK) {
    // ESP_ERROR_CHECK would be a no-op in non-assert builds and let init
    // fall through to advertise a ready path that never enabled TX.
    ESP_LOGE(TAG, "i2s channel enable failed: %s", esp_err_to_name(err));
    i2s_del_channel(tx_chan_);
    tx_chan_ = nullptr;
    return;
  }

  // RX (mic) shares the full clock/slot/pin config with TX — the ES7210's
  // SDOUT drives DIN (GPIO 11). Enabled at init (not deferred to first read)
  // so the RX DMA is clocked from the start. A capture failure is non-fatal:
  // playback still works.
  err = i2s_channel_init_std_mode(rx_chan_, &std_cfg);
  if (err == ESP_OK) err = i2s_channel_enable(rx_chan_);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "i2s rx init/enable failed (no mic): %s",
             esp_err_to_name(err));
    i2s_del_channel(rx_chan_);
    rx_chan_ = nullptr;
  }

  // --- ES8311 codec via esp_codec_dev. ---
  audio_codec_i2c_cfg_t i2c_ctrl_cfg = {};
  i2c_ctrl_cfg.port = kI2cPort;  // shared with touch; bus_handle is what's used
  i2c_ctrl_cfg.addr = kEs8311Addr;
  i2c_ctrl_cfg.bus_handle = i2c_bus_;
  const audio_codec_ctrl_if_t* ctrl_if = audio_codec_new_i2c_ctrl(&i2c_ctrl_cfg);

  audio_codec_i2s_cfg_t i2s_data_cfg = {};
  i2s_data_cfg.port = I2S_NUM_0;
  i2s_data_cfg.tx_handle = tx_chan_;
  i2s_data_cfg.rx_handle = rx_chan_;  // null if RX init failed above
  const audio_codec_data_if_t* data_if = audio_codec_new_i2s_data(&i2s_data_cfg);

  const audio_codec_gpio_if_t* gpio_if = audio_codec_new_gpio();

  es8311_codec_cfg_t es_cfg = {};
  es_cfg.ctrl_if = ctrl_if;
  es_cfg.gpio_if = gpio_if;
  // DAC only — the ES8311 handles speaker output; capture is the ES7210's
  // job (the ES8311 mic input is unpopulated on this board).
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
  open_rate_ = kSampleRate;
  esp_codec_dev_set_out_vol(codec_, vol_pct_);

  // --- Mic capture device: ES7210 ADC (U17, I2C 0x80). Its SDOUT feeds our
  //     I2S RX (rx_handle on DIN=11). Its own I2C control interface + the
  //     shared I2S data interface. Opened lazily in start_capture(). ---
  if (rx_chan_) {
    audio_codec_i2c_cfg_t adc_i2c = {};
    adc_i2c.port = kI2cPort;  // shared touch/codec bus
    adc_i2c.addr = kEs7210Addr;
    adc_i2c.bus_handle = i2c_bus_;
    const audio_codec_ctrl_if_t* adc_ctrl = audio_codec_new_i2c_ctrl(&adc_i2c);

    es7210_codec_cfg_t adc_cfg = {};
    adc_cfg.ctrl_if = adc_ctrl;
    adc_cfg.mic_selected = kEs7210MicSel;
    adc_cfg.master_mode = false;  // P4 I2S is master
    const audio_codec_if_t* adc_if = es7210_codec_new(&adc_cfg);

    if (adc_if) {
      esp_codec_dev_cfg_t in_cfg = {};
      in_cfg.dev_type = ESP_CODEC_DEV_TYPE_IN;
      in_cfg.codec_if = adc_if;
      in_cfg.data_if = data_if;
      codec_in_ = esp_codec_dev_new(&in_cfg);
    }
    if (codec_in_) {
      capture_ready_ = true;
      ESP_LOGI(TAG, "ES7210 mic capture available (%lu Hz)",
               (unsigned long)kSampleRate);
    } else {
      ESP_LOGW(TAG, "ES7210 capture init failed — no mic");
    }
  }

  // Serialises codec open/close/write between the chime task and a
  // streaming caller. Created before the audio task starts.
  codec_mutex_ = xSemaphoreCreateMutex();
  if (!codec_mutex_) {
    ESP_LOGE(TAG, "codec mutex alloc failed");
    return;
  }
  // Serialises the capture refcount + ADC open/close between the wake engine
  // and the run_mic pipeline (different tasks). Separate from codec_mutex_,
  // which guards the playback codec.
  capture_mutex_ = xSemaphoreCreateMutex();
  if (!capture_mutex_) {
    ESP_LOGE(TAG, "capture mutex alloc failed");
    // capture_ready_ was set true when codec_in_ opened; clear it so
    // can_capture() reports false and start_capture()/stop_capture() bail out
    // rather than running the refcount + open/close UNGUARDED (the very race
    // this mutex exists to prevent).
    capture_ready_ = false;
    return;
  }

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
  // A chime during an active voice stream is disposable — drop it rather
  // than fight the stream for the codec (and gap the speech with a tail).
  if (streaming_) return;
  // Cap the clip length. Alerts are well under a second; this both
  // rejects absurd inputs and keeps every downstream byte-size
  // computation (here and the stereo+tail buffer in run()) far from
  // size_t overflow.
  if (frames > kMaxClipFrames) return;

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
  enabled_ = on;
  // Also drop the amp so anything already queued or mid-write goes
  // quiet; without this, disabling would only stop future enqueues.
  if (ready_) gpio_set_level((gpio_num_t)kPaCtrl, on ? 1 : 0);
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
      // Share the codec with the streaming path. If a voice stream grabbed
      // the codec meanwhile, skip this chime (it's disposable).
      if (xSemaphoreTake(codec_mutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (!streaming_) {
          ensure_rate(kSampleRate);
          esp_codec_dev_write(codec_, stereo, total * 2 * sizeof(int16_t));
        }
        xSemaphoreGive(codec_mutex_);
      }
      free(stereo);
    }
    free(clip.samples);
  }
}

// Reclock the I2S TX channel to `rate` and (re)open the codec at that rate.
// Caller holds codec_mutex_. Returns false on failure with open_rate_ left
// consistent (or 0 if even the fallback failed).
bool WaveshareAudio::apply_rate(uint32_t rate) {
  // The MASTER I2S clock is what actually sets playback speed. Reopening only
  // the codec-dev (as before) left the I2S channel clock at its init rate, so
  // a 22050 Hz voice sometimes played at the 16000 Hz clock — the "talking
  // through a pipe" tone. Reconfigure the channel clock explicitly: the API
  // requires the channel be disabled (READY) first, then re-enabled.
  esp_codec_dev_close(codec_);
  i2s_channel_disable(tx_chan_);
  i2s_std_clk_config_t clk = I2S_STD_CLK_DEFAULT_CONFIG(rate);
  clk.mclk_multiple = (i2s_mclk_multiple_t)kMclkMultiple;
  esp_err_t err = i2s_channel_reconfig_std_clock(tx_chan_, &clk);
  if (err == ESP_OK) err = i2s_channel_enable(tx_chan_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "i2s reclock to %lu Hz failed: %s", (unsigned long)rate,
             esp_err_to_name(err));
    return false;
  }
  fs_.sample_rate = rate;
  fs_.channel = 2;
  fs_.bits_per_sample = 16;
  err = esp_codec_dev_open(codec_, &fs_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "codec open at %lu Hz failed: %s", (unsigned long)rate,
             esp_err_to_name(err));
    return false;
  }
  esp_codec_dev_set_out_vol(codec_, vol_pct_);
  open_rate_ = rate;
  return true;
}

bool WaveshareAudio::ensure_rate(uint32_t rate) {
  // Caller holds codec_mutex_. Only touch the hardware on an actual rate
  // switch — the codec + I2S clock stay put otherwise.
  if (rate == open_rate_) return true;
  if (apply_rate(rate)) return true;
  // Reclock/open failed — fall back to the native rate so the chime path and
  // idle stay functional rather than leaving the codec closed.
  if (!apply_rate(kSampleRate)) open_rate_ = 0;
  return false;
}

bool WaveshareAudio::begin_stream(uint32_t rate, uint8_t bits,
                                  uint8_t channels) {
  // Mono 16-bit only (the codec is opened as a stereo frame and we
  // duplicate L/R). A nonconforming format is refused so the caller can
  // fall back rather than play garbage. A muted board stays silent — like
  // play_pcm(), honour enabled_ so "a quiet helm stays quiet".
  if (!ready_ || !enabled_ || bits != 16 || channels != 1 || rate == 0) {
    return false;
  }
  // Non-reentrant: the mutex is held for the whole stream, so a second
  // begin_stream() before end_stream() would block forever. Refuse instead.
  if (streaming_) return false;
  if (xSemaphoreTake(codec_mutex_, portMAX_DELAY) != pdTRUE) return false;
  bool ok = ensure_rate(rate);
  if (ok) {
    streaming_ = true;
    // Make sure the amp is live for the stream.
    gpio_set_level((gpio_num_t)kPaCtrl, 1);
  } else {
    xSemaphoreGive(codec_mutex_);
  }
  // On success we HOLD codec_mutex_ across the whole stream so no chime can
  // reopen the codec mid-utterance; end_stream releases it.
  return ok;
}

size_t WaveshareAudio::write_stream(const int16_t* samples, size_t frames) {
  if (!streaming_ || !samples || frames == 0) return 0;
  // Duplicate mono -> interleaved L/R into a reusable buffer, then do the
  // BLOCKING codec write. Blocking here is the backpressure that paces the
  // sender; we never drop stream audio.
  if (frames > stream_stereo_frames_) {
    int16_t* nb = (int16_t*)realloc(stream_stereo_, frames * 2 * sizeof(int16_t));
    if (!nb) return 0;  // keep the old buffer; caller may retry smaller
    stream_stereo_ = nb;
    stream_stereo_frames_ = frames;
  }
  for (size_t i = 0; i < frames; ++i) {
    stream_stereo_[2 * i] = samples[i];
    stream_stereo_[2 * i + 1] = samples[i];
  }
  esp_err_t err =
      esp_codec_dev_write(codec_, stream_stereo_, frames * 2 * sizeof(int16_t));
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "stream write failed: %s", esp_err_to_name(err));
    return 0;  // report the codec fault rather than a phantom success
  }
  return frames;
}

void WaveshareAudio::end_stream() {
  if (!streaming_) return;
  // Flush a short block of silence: the I2S DMA ring auto-repeats its last
  // buffer, so without a zero tail the final chunk drones. ~100 ms at the
  // stream rate.
  size_t tail = open_rate_ ? open_rate_ / 10 : kSampleRate / 10;
  size_t bytes = tail * 2 * sizeof(int16_t);
  void* zeros = calloc(1, bytes);
  if (zeros) {
    esp_err_t err = esp_codec_dev_write(codec_, zeros, bytes);
    if (err != ESP_OK) {
      // A failed drain leaves the DMA ring on the last audio buffer, so the
      // stream's tail can loop. Log it — the codec is in a bad state.
      ESP_LOGW(TAG, "silence drain failed: %s", esp_err_to_name(err));
    }
    free(zeros);
  }
  streaming_ = false;
  if (stream_stereo_) {
    free(stream_stereo_);
    stream_stereo_ = nullptr;
    stream_stereo_frames_ = 0;
  }
  // Release the codec so chimes can play again. (begin_stream took it.)
  xSemaphoreGive(codec_mutex_);
}

void WaveshareAudio::start_capture() {
  if (!capture_ready_) return;
  if (capture_mutex_) xSemaphoreTake(capture_mutex_, portMAX_DELAY);
  // Refcount: the ADC opens on the first consumer and stays open while any
  // consumer wants it. A second start_capture() (e.g. run_mic while the wake
  // engine is already listening — or a resume() racing run_mic's stop) just
  // bumps the count; it must NOT re-open (already open) nor let the other
  // consumer's stop close it underneath us.
  if (capture_users_++ == 0) {
    fs_in_.sample_rate = kSampleRate;
    fs_in_.channel = 1;  // mono capture — one ADC channel
    fs_in_.bits_per_sample = 16;
    esp_err_t err = esp_codec_dev_open(codec_in_, &fs_in_);
    if (err != ESP_OK) {
      ESP_LOGW(TAG, "mic open failed: %s", esp_err_to_name(err));
      capture_users_ = 0;
      if (capture_mutex_) xSemaphoreGive(capture_mutex_);
      return;
    }
    // Max the ES7210 analog PGA (37.5 dB). The onboard MEMS mics are quiet —
    // at 30 dB speech peaked near the noise floor (~100 on a 32767 scale),
    // which whisper tolerates but a wake detector will not. 37.5 dB is the
    // driver's ceiling (get_db clamps >36 dB there).
    esp_codec_dev_set_in_gain(codec_in_, 37.5);
    capturing_ = true;
  }
  if (capture_mutex_) xSemaphoreGive(capture_mutex_);
}

size_t WaveshareAudio::record_pcm(int16_t* out, size_t max_frames) {
  if (!capturing_ || !out || max_frames == 0) return 0;
  // BLOCKING read of mono 16-bit frames from the ES7210 (opened mono, so one
  // sample per frame — no L/R de-interleave needed). The block paces the
  // caller. Uses its OWN device handle, independent of the playback stream.
  // esp_codec_dev_read takes an int byte count — clamp so a huge request
  // can't overflow it and then be reported as a full success.
  size_t frames = max_frames;
  const size_t max_by_int = (size_t)INT_MAX / sizeof(int16_t);
  if (frames > max_by_int) frames = max_by_int;
  // Mark the read in flight so stop_capture() won't close the handle under us
  // (esp_codec_dev is not safe for concurrent read + close).
  reading_.store(true);
  esp_err_t err =
      esp_codec_dev_read(codec_in_, out, (int)(frames * sizeof(int16_t)));
  reading_.store(false);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "mic read failed: %s", esp_err_to_name(err));
    return 0;
  }
  return frames;
}

void WaveshareAudio::stop_capture() {
  if (!capture_ready_) return;
  if (capture_mutex_) xSemaphoreTake(capture_mutex_, portMAX_DELAY);
  // Only the LAST consumer to leave closes the ADC. This is what stops one
  // consumer's stop_capture() from cutting the mic out from under the other.
  if (capture_users_ > 0 && --capture_users_ == 0 && capturing_) {
    // Wait out any in-flight record_pcm() before closing — esp_codec_dev is
    // not safe for a concurrent read + close on one handle. A read is a single
    // ~32 ms chunk, so this settles almost immediately; bounded so a wedged
    // read can't hang teardown forever.
    for (int i = 0; i < 100 && reading_.load(); i++) {
      vTaskDelay(pdMS_TO_TICKS(2));
    }
    esp_codec_dev_close(codec_in_);
    capturing_ = false;
  }
  if (capture_mutex_) xSemaphoreGive(capture_mutex_);
}

}  // namespace sensesp_cockpit_display
