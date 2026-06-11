#include "lv_drivers.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"

static const char* TAG = "lv_drivers";

namespace sensesp_cockpit_display {

static DisplayDriver* s_display = nullptr;

// LVGL flush callback — rotates the partial region 180° in software
// and writes it to the mirrored location on the panel.
static void flush_cb(lv_display_t* disp, const lv_area_t* area,
                     uint8_t* px_map) {
  if (s_display) {
    int w = area->x2 - area->x1 + 1;
    int h = area->y2 - area->y1 + 1;
    int disp_w = s_display->width();
    int disp_h = s_display->height();

    // Reverse the pixel order in place (180° rotation).
    uint16_t* pixels = reinterpret_cast<uint16_t*>(px_map);
    int total = w * h;
    for (int i = 0; i < total / 2; i++) {
      uint16_t tmp = pixels[i];
      pixels[i] = pixels[total - 1 - i];
      pixels[total - 1 - i] = tmp;
    }

    // Flip the area coordinates to the opposite side of the display.
    int flipped_x = disp_w - (area->x1 + w);
    int flipped_y = disp_h - (area->y1 + h);
    s_display->flush(flipped_x, flipped_y, w, h, px_map);
  }
  lv_display_flush_ready(disp);
}

// LVGL input device read callback — reads GT911 touch state.
static void indev_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
  auto* touch = static_cast<TouchDriver*>(lv_indev_get_user_data(indev));
  auto pt = touch->read();
  data->point.x = pt.x;
  data->point.y = pt.y;
  data->state = pt.pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// LVGL tick source — called from esp_timer at 1ms interval.
static esp_timer_handle_t tick_timer = nullptr;

static void tick_timer_cb(void* arg) {
  lv_tick_inc(1);
}

void lvgl_init(DisplayDriver* display, TouchDriver* touch) {
  s_display = display;

  // Initialize hardware
  display->init();
  touch->init();

  // Initialize LVGL
  lv_init();
  ESP_LOGI(TAG, "LVGL %d.%d.%d initialized", lv_version_major(),
           lv_version_minor(), lv_version_patch());

  // PARTIAL render mode — LVGL allocates small buffers in PSRAM,
  // rotates each rendered strip in software, and calls flush_cb to
  // write it into the DPI framebuffer at the correct position.
  // This mirrors Waveshare's own LVGL demo pattern (sw_rotate = true).
  lv_display_t* lv_disp = lv_display_create(display->width(),
                                             display->height());
  lv_display_set_flush_cb(lv_disp, flush_cb);

  size_t buf_size = display->width() * (display->height() / 10) *
                    sizeof(uint16_t);
  static void* partial_buf1 =
      heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
  static void* partial_buf2 =
      heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM);
  lv_display_set_buffers(lv_disp, partial_buf1, partial_buf2, buf_size,
                         LV_DISPLAY_RENDER_MODE_PARTIAL);

  // Rotation is handled manually in flush_cb and indev_read_cb.
  // LVGL's set_rotation doesn't work reliably with partial mode + our flush.

  // Create LVGL input device for touch
  lv_indev_t* lv_indev = lv_indev_create();
  lv_indev_set_type(lv_indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(lv_indev, indev_read_cb);
  lv_indev_set_user_data(lv_indev, touch);

  // Start 1ms tick timer for LVGL
  esp_timer_create_args_t timer_args = {
      .callback = tick_timer_cb,
      .arg = nullptr,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "lvgl_tick",
  };
  ESP_ERROR_CHECK(esp_timer_create(&timer_args, &tick_timer));
  ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, 1000));  // 1ms

  ESP_LOGI(TAG, "Display %dx%d, touch enabled, tick running",
           display->width(), display->height());
}

void lvgl_tick() {
  lv_timer_handler();
}

DisplayDriver* get_display() { return s_display; }

}  // namespace sensesp_cockpit_display
