#include "button_switch_page.h"
#include "../theme.h"

namespace sensesp_cockpit_display {

ButtonSwitchPage::ButtonSwitchPage(lv_obj_t* parent) {
  grid_ = lv_obj_create(parent);
  lv_obj_set_size(grid_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(grid_, theme::bg(), 0);
  lv_obj_set_style_bg_opa(grid_, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(grid_, 6, 0);
  lv_obj_set_style_pad_gap(grid_, 6, 0);
  lv_obj_set_style_border_width(grid_, 0, 0);
  lv_obj_set_flex_flow(grid_, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(grid_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  lv_obj_set_scrollbar_mode(grid_, LV_SCROLLBAR_MODE_AUTO);
}

SwitchButton* ButtonSwitchPage::add_switch(const char* label) {
  auto b = std::make_unique<SwitchButton>(grid_, label);
  // 4 columns × many rows on 1024x600 — ~10 rows per page
  lv_obj_set_width(b->get_obj(), LV_PCT(24));
  auto* ptr = b.get();
  switches_.push_back(std::move(b));
  return ptr;
}

}  // namespace sensesp_cockpit_display
