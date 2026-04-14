#include "lv_drivers.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char* TAG = "lv_drivers";

namespace sensesp_cockpit_display {

static DisplayDriver* s_display = nullptr;

// LVGL flush callback — pushes the rendered buffer to the display hardware.
static void flush_cb(lv_display_t* disp, const lv_area_t* area,
                     uint8_t* px_map) {
  if (s_display) {
    s_display->flush(px_map);
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

  // Create LVGL display using the DMA framebuffers from the display driver
  lv_display_t* lv_disp = lv_display_create(display->width(), display->height());
  lv_display_set_flush_cb(lv_disp, flush_cb);
  lv_display_set_buffers(lv_disp,
                         display->get_draw_buffer(0),
                         display->get_draw_buffer(1),
                         display->get_draw_buffer_size(),
                         LV_DISPLAY_RENDER_MODE_DIRECT);

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

}  // namespace sensesp_cockpit_display
