#include "status_page.h"
#include "../theme.h"
#include <cstdio>

namespace sensesp_cockpit_display {

StatusPage::StatusPage(lv_obj_t* parent) {
  container_ = lv_obj_create(parent);
  lv_obj_set_size(container_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(container_, theme::bg(), 0);
  lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(container_, 16, 0);
  lv_obj_set_style_pad_row(container_, 8, 0);
  lv_obj_set_style_border_width(container_, 0, 0);

  lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(container_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_CENTER);

  wifi_ = new StatusIndicator(container_, "WiFi");
  sk_ws_ = new StatusIndicator(container_, "SignalK WebSocket");
  n2k_ = new StatusIndicator(container_, "NMEA 2000");
  ble_ = new StatusIndicator(container_, "Bluetooth LE");

  info_label_ = lv_label_create(container_);
  lv_label_set_text(info_label_, "Uptime: 0s | Heap: 0 KB");
  lv_obj_set_style_text_color(info_label_, theme::label(), 0);
  lv_obj_set_style_text_font(info_label_, &lv_font_montserrat_16, 0);
  lv_obj_set_style_pad_top(info_label_, 12, 0);

  // Default states
  wifi_->set_status(StatusLevel::kUnknown, "initializing");
  sk_ws_->set_status(StatusLevel::kUnknown, "initializing");
  n2k_->set_status(StatusLevel::kUnknown, "initializing");
  ble_->set_status(StatusLevel::kUnknown, "disabled");
}

void StatusPage::update_info(uint32_t uptime_s, uint32_t free_heap,
                             int64_t n2k_rx_idle_s, uint32_t n2k_clients) {
  char buf[128];
  uint32_t h = uptime_s / 3600;
  uint32_t m = (uptime_s / 60) % 60;
  uint32_t s = uptime_s % 60;
  char n2k_buf[32];
  if (n2k_rx_idle_s < 0) {
    snprintf(n2k_buf, sizeof(n2k_buf), "no data yet");
  } else if (n2k_rx_idle_s < 2) {
    snprintf(n2k_buf, sizeof(n2k_buf), "live");
  } else {
    snprintf(n2k_buf, sizeof(n2k_buf), "idle %llds",
             (long long)n2k_rx_idle_s);
  }
  snprintf(buf, sizeof(buf),
           "Uptime: %02lu:%02lu:%02lu  |  Heap: %lu KB  |  "
           "N2K %s  |  clients=%lu",
           (unsigned long)h, (unsigned long)m, (unsigned long)s,
           (unsigned long)(free_heap / 1024),
           n2k_buf, (unsigned long)n2k_clients);
  lv_label_set_text(info_label_, buf);
}

}  // namespace sensesp_cockpit_display
