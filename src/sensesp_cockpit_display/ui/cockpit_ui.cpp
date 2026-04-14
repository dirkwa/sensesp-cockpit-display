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

  // Placeholder tabs for future pages
  lv_obj_t* instr_tab = lv_tabview_add_tab(tabview_, "Instruments");
  lv_obj_t* instr_lbl = lv_label_create(instr_tab);
  lv_label_set_text(instr_lbl, "Instruments — coming soon");
  lv_obj_set_style_text_color(instr_lbl, theme::label(), 0);
  lv_obj_center(instr_lbl);

  lv_obj_t* status_tab = lv_tabview_add_tab(tabview_, "Status");
  lv_obj_t* status_lbl = lv_label_create(status_tab);
  lv_label_set_text(status_lbl, "System Status — coming soon");
  lv_obj_set_style_text_color(status_lbl, theme::label(), 0);
  lv_obj_center(status_lbl);

  ESP_LOGI(TAG, "Cockpit UI initialized with %d tabs", 3);
}

}  // namespace sensesp_cockpit_display
