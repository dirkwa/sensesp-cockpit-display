#include "waveshare_7b.h"

#include <cstring>
#include <Wire.h>

#include "esp_lcd_ek79007.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

static const char* TAG = "ws7b";

// Board pin definitions
static constexpr gpio_num_t kBacklightGpio = GPIO_NUM_32;
static constexpr gpio_num_t kResetGpio = GPIO_NUM_33;
static constexpr int kLdoMipiChan = 3;
static constexpr int kLdoMipiMv = 2500;
static constexpr int kTouchSda = 7;
static constexpr int kTouchScl = 8;
static constexpr uint8_t kGT911Addr = 0x5D;
static constexpr uint16_t kStatusReg = 0x814E;
static constexpr uint16_t kPointReg = 0x814F;

namespace sensesp_cockpit_display {

// ============================================================
// Display
// ============================================================

void Waveshare7BDisplay::init() {
  init_ldo();

  ESP_LOGI(TAG, "Initializing MIPI-DSI bus");
  esp_lcd_dsi_bus_config_t bus_cfg = EK79007_PANEL_BUS_DSI_2CH_CONFIG();
  ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_cfg, &dsi_bus_));

  esp_lcd_dbi_io_config_t dbi_cfg = EK79007_PANEL_IO_DBI_CONFIG();
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(dsi_bus_, &dbi_cfg, &dbi_io_));

  // EK79007 DPI panel config — expanded from macro (C++ doesn't allow
  // nested designators like .flags.use_dma2d)
  esp_lcd_dpi_panel_config_t dpi_cfg = {};
  dpi_cfg.dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT;
  dpi_cfg.dpi_clock_freq_mhz = 52;
  dpi_cfg.virtual_channel = 0;
  dpi_cfg.pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565;
  dpi_cfg.num_fbs = kNumBuffers;
  dpi_cfg.video_timing.h_size = 1024;
  dpi_cfg.video_timing.v_size = 600;
  dpi_cfg.video_timing.hsync_back_porch = 160;
  dpi_cfg.video_timing.hsync_pulse_width = 10;
  dpi_cfg.video_timing.hsync_front_porch = 160;
  dpi_cfg.video_timing.vsync_back_porch = 23;
  dpi_cfg.video_timing.vsync_pulse_width = 1;
  dpi_cfg.video_timing.vsync_front_porch = 12;
  dpi_cfg.flags.use_dma2d = true;

  ek79007_vendor_config_t vendor_cfg = {
      .init_cmds = nullptr,
      .init_cmds_size = 0,
      .mipi_config = {
          .dsi_bus = dsi_bus_,
          .dpi_config = &dpi_cfg,
          .lane_num = 2,
      },
  };

  esp_lcd_panel_dev_config_t panel_cfg = {
      .reset_gpio_num = kResetGpio,
      .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
      .bits_per_pixel = 16,
      .vendor_config = &vendor_cfg,
  };

  ESP_ERROR_CHECK(esp_lcd_new_panel_ek79007(dbi_io_, &panel_cfg, &panel_));
  gpio_set_level(kResetGpio, 1);
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));

  ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(
      panel_, kNumBuffers, &framebuffers_[0], &framebuffers_[1]));

  memset(framebuffers_[0], 0, kBufferSize);
  memset(framebuffers_[1], 0, kBufferSize);

  init_backlight();
  ESP_LOGI(TAG, "Display: %dx%d RGB565, %d DMA buffers", kWidth, kHeight,
           kNumBuffers);
}

void* Waveshare7BDisplay::get_draw_buffer(int index) {
  return framebuffers_[index % kNumBuffers];
}

void Waveshare7BDisplay::flush(const void* buf) {
  esp_lcd_panel_draw_bitmap(panel_, 0, 0, kWidth, kHeight, buf);
}

void Waveshare7BDisplay::init_ldo() {
  esp_ldo_channel_handle_t ldo = nullptr;
  esp_ldo_channel_config_t cfg = {
      .chan_id = kLdoMipiChan,
      .voltage_mv = kLdoMipiMv,
  };
  ESP_ERROR_CHECK(esp_ldo_acquire_channel(&cfg, &ldo));
}

void Waveshare7BDisplay::init_backlight() {
  ledc_timer_config_t timer = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .duty_resolution = LEDC_TIMER_10_BIT,
      .timer_num = LEDC_TIMER_1,
      .freq_hz = 1000,
      .clk_cfg = LEDC_AUTO_CLK,
  };
  ESP_ERROR_CHECK(ledc_timer_config(&timer));

  ledc_channel_config_t ch = {
      .gpio_num = kBacklightGpio,
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .channel = LEDC_CHANNEL_1,
      .intr_type = LEDC_INTR_DISABLE,
      .timer_sel = LEDC_TIMER_1,
      .duty = 0,
      .hpoint = 0,
      .flags = {.output_invert = 1},
  };
  ESP_ERROR_CHECK(ledc_channel_config(&ch));
  ESP_ERROR_CHECK(
      ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, (1023 * 95) / 100));
  ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1));
}

// ============================================================
// Touch (GT911 via Arduino Wire)
// ============================================================

static uint8_t gt911_read_reg(uint16_t reg) {
  Wire.beginTransmission(kGT911Addr);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)(reg & 0xFF));
  Wire.endTransmission(false);
  Wire.requestFrom(kGT911Addr, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}

static void gt911_write_reg(uint16_t reg, uint8_t val) {
  Wire.beginTransmission(kGT911Addr);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)(reg & 0xFF));
  Wire.write(val);
  Wire.endTransmission();
}

static void gt911_read_regs(uint16_t reg, uint8_t* buf, uint8_t len) {
  Wire.beginTransmission(kGT911Addr);
  Wire.write((uint8_t)(reg >> 8));
  Wire.write((uint8_t)(reg & 0xFF));
  Wire.endTransmission(false);
  Wire.requestFrom(kGT911Addr, len);
  for (int i = 0; i < len && Wire.available(); i++) {
    buf[i] = Wire.read();
  }
}

void Waveshare7BTouch::init() {
  Wire.begin(kTouchSda, kTouchScl, 400000);

  // Verify GT911 is present
  uint8_t id[4] = {};
  gt911_read_regs(0x8140, id, 4);
  if (id[0] == '9' && id[1] == '1' && id[2] == '1') {
    ESP_LOGI(TAG, "GT911 touch initialized");
    gt911_write_reg(kStatusReg, 0x00);
    initialized_ = true;
  } else {
    ESP_LOGE(TAG, "GT911 not found (ID: %c%c%c)", id[0], id[1], id[2]);
  }
}

TouchDriver::TouchPoint Waveshare7BTouch::read() {
  TouchPoint pt = {0, 0, false};
  if (!initialized_) return pt;

  uint8_t status = gt911_read_reg(kStatusReg);
  if (status & 0x80) {
    uint8_t cnt = status & 0x0F;
    if (cnt > 0 && cnt <= 5) {
      uint8_t tdata[8] = {};
      gt911_read_regs(kPointReg, tdata, 8);
      // Raw touch; LVGL rotation handles the flip
      pt.x = tdata[1] | (tdata[2] << 8);
      pt.y = tdata[3] | (tdata[4] << 8);
      pt.pressed = true;
    }
    gt911_write_reg(kStatusReg, 0x00);
  }
  return pt;
}

}  // namespace sensesp_cockpit_display
