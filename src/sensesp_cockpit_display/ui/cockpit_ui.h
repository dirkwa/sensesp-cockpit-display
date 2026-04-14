#pragma once

#include "lvgl.h"
#include "../hal/display_driver.h"
#include "../hal/touch_driver.h"
#include "pages/switch_page.h"
#include "pages/instrument_page.h"

namespace sensesp_cockpit_display {

/// Top-level UI manager. Creates the display, pages, and navigation.
class CockpitUI {
 public:
  CockpitUI(DisplayDriver* display, TouchDriver* touch);

  void init();
  void tick() { lv_timer_handler(); }

  SwitchPage* get_switch_page() { return switch_page_; }
  InstrumentPage* get_instrument_page() { return instrument_page_; }

 private:
  DisplayDriver* display_;
  TouchDriver* touch_;
  lv_obj_t* tabview_ = nullptr;
  SwitchPage* switch_page_ = nullptr;
  InstrumentPage* instrument_page_ = nullptr;
};

}  // namespace sensesp_cockpit_display
