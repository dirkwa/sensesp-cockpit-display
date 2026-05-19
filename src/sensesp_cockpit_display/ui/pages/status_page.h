#pragma once

#include <cstdint>

#include "lvgl.h"
#include "../widgets/status_indicator.h"

namespace sensesp_cockpit_display {

/// System status page showing connection health and uptime.
class StatusPage {
 public:
  explicit StatusPage(lv_obj_t* parent);

  StatusIndicator* wifi() { return wifi_; }
  StatusIndicator* sk_ws() { return sk_ws_; }
  StatusIndicator* n2k() { return n2k_; }
  StatusIndicator* ble() { return ble_; }

  /// Refresh dynamic info (uptime, heap, etc.)
  /// n2k_rx_idle_s: seconds since last received N2K frame, or -1 if none yet.
  void update_info(uint32_t uptime_s, uint32_t free_heap,
                   int64_t n2k_rx_idle_s, uint32_t n2k_clients);

 private:
  lv_obj_t* container_;
  StatusIndicator* wifi_ = nullptr;
  StatusIndicator* sk_ws_ = nullptr;
  StatusIndicator* n2k_ = nullptr;
  StatusIndicator* ble_ = nullptr;
  lv_obj_t* info_label_ = nullptr;
};

}  // namespace sensesp_cockpit_display
