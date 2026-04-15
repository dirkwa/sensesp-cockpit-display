#pragma once

#include <vector>
#include "lvgl.h"
#include "../hal/display_driver.h"
#include "../hal/touch_driver.h"
#include "pages/switch_page.h"
#include "pages/button_switch_page.h"
#include "pages/instrument_page.h"
#include "pages/status_page.h"

namespace sensesp_cockpit_display {

/// Top-level UI manager. Creates the display, pages, and navigation.
class CockpitUI {
 public:
  CockpitUI(DisplayDriver* display, TouchDriver* touch);

  /// Initialize LVGL + display. Call before add_switch_page().
  /// After adding all switch pages, call finalize() to append the
  /// Instruments + Status tabs at the end.
  void init();
  void finalize();
  void tick() { lv_timer_handler(); }

  /// Add a switch page tab. Call these before finalize().
  SwitchPage* add_switch_page(const char* tab_name);

  /// Add a lightweight button-based switch page tab.
  ButtonSwitchPage* add_button_switch_page(const char* tab_name);

  /// Convenience: returns the first switch page (creates one named
  /// "SW" if none exist yet).
  SwitchPage* get_switch_page();

  InstrumentPage* get_instrument_page() { return instrument_page_; }
  StatusPage* get_status_page() { return status_page_; }

 private:
  DisplayDriver* display_;
  TouchDriver* touch_;
  lv_obj_t* tabview_ = nullptr;
  std::vector<SwitchPage*> switch_pages_;
  InstrumentPage* instrument_page_ = nullptr;
  StatusPage* status_page_ = nullptr;
  bool finalized_ = false;
};

}  // namespace sensesp_cockpit_display
