#include "cockpit_ui.h"
#include "../lvgl/lv_drivers.h"
#include "theme.h"
#include "esp_log.h"

static const char* TAG = "cockpit_ui";

namespace sensesp_cockpit_display {

CockpitUI::CockpitUI(DisplayDriver* display, TouchDriver* touch)
    : display_(display), touch_(touch) {}

void CockpitUI::init() {
  // Initialize LVGL + display + touch
  lvgl_init(display_, touch_);

  // Set background color
  lv_obj_t* scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, theme::bg(), 0);

  // Create tabview — tabs at the bottom for easy thumb access
  tabview_ = lv_tabview_create(scr);
  lv_tabview_set_tab_bar_position(tabview_, LV_DIR_BOTTOM);
  lv_tabview_set_tab_bar_size(tabview_, 40);
  lv_obj_set_size(tabview_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(tabview_, theme::bg(), 0);

  // Style the tab bar
  lv_obj_t* tab_bar = lv_tabview_get_tab_bar(tabview_);
  lv_obj_set_style_bg_color(tab_bar, theme::header(), 0);
  lv_obj_set_style_bg_opa(tab_bar, LV_OPA_COVER, 0);

  // Switches page
  lv_obj_t* sw_tab = lv_tabview_add_tab(tabview_, "Switches");
  switch_page_ = new SwitchPage(sw_tab);

  // Instruments page
  lv_obj_t* instr_tab = lv_tabview_add_tab(tabview_, "Instruments");
  instrument_page_ = new InstrumentPage(instr_tab);

  lv_obj_t* status_tab = lv_tabview_add_tab(tabview_, "Status");
  status_page_ = new StatusPage(status_tab);

  ESP_LOGI(TAG, "Cockpit UI initialized with %d tabs", 3);
}

SwitchPage* CockpitUI::add_switch_page(const char* tab_name) {
  // Insert before the Instruments tab (which is currently at index 1).
  // Simpler: just append — Instruments and Status move to the right.
  lv_obj_t* tab = lv_tabview_add_tab(tabview_, tab_name);
  return new SwitchPage(tab);
}

}  // namespace sensesp_cockpit_display
