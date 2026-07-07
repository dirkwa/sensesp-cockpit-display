#include "waveshare_4b.h"

#include <cstring>
#include <Wire.h>

#include "esp_lcd_st7703.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_lcd_panel_ops.h"
#include "esp_ldo_regulator.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

static const char* TAG = "ws4b";

// Board pin definitions (Waveshare ESP32-P4-WIFI6-Touch-LCD-4B BSP).
static constexpr gpio_num_t kBacklightGpio = GPIO_NUM_26;
static constexpr gpio_num_t kResetGpio = GPIO_NUM_27;
// The 4B needs TWO LDO rails, unlike the 7B: the MIPI D-PHY power
// (VO3, same as the 7B) plus VO4 for the panel's own supply.
static constexpr int kLdoDphyChan = 3;
static constexpr int kLdoDphyMv = 2500;
static constexpr int kLdoVo4Chan = 4;
static constexpr int kLdoVo4Mv = 3300;
static constexpr int kMipiLaneNum = 2;
static constexpr int kMipiLaneRateMbps = 480;
static constexpr int kTouchSda = 7;
static constexpr int kTouchScl = 8;
static constexpr uint8_t kGT911Addr = 0x5D;
static constexpr uint16_t kStatusReg = 0x814E;
static constexpr uint16_t kPointReg = 0x814F;

namespace sensesp_cockpit_display {

// ============================================================
// Display
// ============================================================

void Waveshare4BDisplay::init() {
  init_ldo();

  ESP_LOGI(TAG, "Initializing MIPI-DSI bus");
  esp_lcd_dsi_bus_config_t bus_cfg = {
      .bus_id = 0,
      .num_data_lanes = kMipiLaneNum,
      .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT,
      .lane_bit_rate_mbps = kMipiLaneRateMbps,
  };
  ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_cfg, &dsi_bus_));

  esp_lcd_dbi_io_config_t dbi_cfg = {
      .virtual_channel = 0,
      .lcd_cmd_bits = 8,
      .lcd_param_bits = 8,
  };
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(dsi_bus_, &dbi_cfg, &dbi_io_));

  // ST7703 DPI panel config — expanded from
  // ST7703_720_720_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565)
  // because C++ doesn't allow the nested designator .flags.use_dma2d.
  // Timings are the driver's own; do NOT hand-tune the porches.
  esp_lcd_dpi_panel_config_t dpi_cfg = {};
  dpi_cfg.dpi_clk_src = MIPI_DSI_DPI_CLK_SRC_DEFAULT;
  dpi_cfg.dpi_clock_freq_mhz = 46;
  dpi_cfg.virtual_channel = 0;
  dpi_cfg.pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565;
  dpi_cfg.num_fbs = kNumBuffers;
  dpi_cfg.video_timing.h_size = 720;
  dpi_cfg.video_timing.v_size = 720;
  dpi_cfg.video_timing.hsync_back_porch = 120;
  dpi_cfg.video_timing.hsync_pulse_width = 60;
  dpi_cfg.video_timing.hsync_front_porch = 106;
  dpi_cfg.video_timing.vsync_back_porch = 20;
  dpi_cfg.video_timing.vsync_pulse_width = 4;
  dpi_cfg.video_timing.vsync_front_porch = 20;
  dpi_cfg.flags.use_dma2d = true;

  st7703_vendor_config_t vendor_cfg = {};
  vendor_cfg.mipi_config.dsi_bus = dsi_bus_;
  vendor_cfg.mipi_config.dpi_config = &dpi_cfg;
  vendor_cfg.flags.use_mipi_interface = 1;

  esp_lcd_panel_dev_config_t panel_cfg = {
      .reset_gpio_num = kResetGpio,
      .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
      .bits_per_pixel = 16,
      .vendor_config = &vendor_cfg,
  };

  ESP_ERROR_CHECK(esp_lcd_new_panel_st7703(dbi_io_, &panel_cfg, &panel_));
  // ST7703 drives its own reset sequence over the reset GPIO; the 7B's
  // EK79007 didn't, so this reset() call is board-specific.
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel_));

  // Rotation handled by LVGL (lv_display_set_rotation)

  ESP_ERROR_CHECK(esp_lcd_dpi_panel_get_frame_buffer(
      panel_, kNumBuffers, &framebuffers_[0], &framebuffers_[1]));

  memset(framebuffers_[0], 0, kBufferSize);
  memset(framebuffers_[1], 0, kBufferSize);

  init_backlight();
  ESP_LOGI(TAG, "Display: %dx%d RGB565, %d DMA buffers", kWidth, kHeight,
           kNumBuffers);
}

void* Waveshare4BDisplay::get_draw_buffer(int index) {
  return framebuffers_[index % kNumBuffers];
}

void Waveshare4BDisplay::flush(int x, int y, int w, int h, const void* buf) {
  // Push a rectangular region to the panel. LVGL PARTIAL mode handles
  // rotation and gives us the already-rotated pixels for this region.
  esp_lcd_panel_draw_bitmap(panel_, x, y, x + w, y + h, buf);
}

void Waveshare4BDisplay::init_ldo() {
  // VO3 powers the MIPI D-PHY (shared with the 7B); VO4 is the panel's
  // supply rail, which the 7B board didn't route through an LDO. Both
  // must come up before the DSI bus is created.
  esp_ldo_channel_handle_t dphy = nullptr;
  esp_ldo_channel_config_t dphy_cfg = {
      .chan_id = kLdoDphyChan,
      .voltage_mv = kLdoDphyMv,
  };
  ESP_ERROR_CHECK(esp_ldo_acquire_channel(&dphy_cfg, &dphy));

  esp_ldo_channel_handle_t vo4 = nullptr;
  esp_ldo_channel_config_t vo4_cfg = {
      .chan_id = kLdoVo4Chan,
      .voltage_mv = kLdoVo4Mv,
  };
  ESP_ERROR_CHECK(esp_ldo_acquire_channel(&vo4_cfg, &vo4));
}

void Waveshare4BDisplay::init_backlight() {
  ledc_timer_config_t timer = {
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .duty_resolution = LEDC_TIMER_10_BIT,
      .timer_num = LEDC_TIMER_1,
      .freq_hz = 5000,
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

void Waveshare4BDisplay::set_brightness(uint8_t pct) {
  // Same LEDC duty mapping as the 7B: duty 0 = off, 1023 = full. The
  // output_invert flag on the channel doesn't reverse perceived
  // brightness on this board.
  if (pct > 100) pct = 100;
  uint32_t duty = (1023 * pct) / 100;
  ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty);
  ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

void Waveshare4BDisplay::set_display_on(bool on) {
  // With the panel off, the LCD's sync signals stop driving the ITO
  // grid, removing capacitive coupling into the GT911 touch layer so
  // the chip can still detect fingertips at 0% backlight.
  if (panel_) esp_lcd_panel_disp_on_off(panel_, on);
}

// ============================================================
// Touch (GT911 via Arduino Wire) — identical to the 7B: same chip,
// same I2C pins (SDA=7, SCL=8), same register map. The 4B routes the
// touch reset to GPIO 23 and leaves INT unconnected (poll-only), but
// the GT911 comes up addressable without us driving reset here.
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

void Waveshare4BTouch::init() {
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

TouchDriver::TouchPoint Waveshare4BTouch::read() {
  // Default state is "released at the last-seen coordinates". LVGL
  // distinguishes a click from a drag by comparing the press
  // coordinates with the release coordinates — if we returned (0,0)
  // on every release, every press at (x,y) where x|y != 0 would look
  // like a drag-off-widget and never fire LV_EVENT_CLICKED.
  static TouchPoint last = {0, 0, false};
  TouchPoint pt = {last.x, last.y, false};
  if (!initialized_) return pt;

  uint8_t status = gt911_read_reg(kStatusReg);
  if (status & 0x80) {
    uint8_t cnt = status & 0x0F;
    if (cnt > 0 && cnt <= 5) {
      uint8_t tdata[8] = {};
      gt911_read_regs(kPointReg, tdata, 8);
      // Raw touch — display is flipped in flush_cb, so the touch
      // coordinates already match the visible orientation.
      pt.x = tdata[1] | (tdata[2] << 8);
      pt.y = tdata[3] | (tdata[4] << 8);
      pt.pressed = true;
      ESP_LOGI(TAG, "touch x=%d y=%d", pt.x, pt.y);
      last = pt;
    }
    gt911_write_reg(kStatusReg, 0x00);
  }
  return pt;
}

}  // namespace sensesp_cockpit_display
