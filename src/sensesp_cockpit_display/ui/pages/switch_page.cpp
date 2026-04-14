#include "switch_page.h"
#include "../theme.h"

namespace sensesp_cockpit_display {

SwitchPage::SwitchPage(lv_obj_t* parent) {
  grid_ = lv_obj_create(parent);
  lv_obj_set_size(grid_, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(grid_, theme::bg(), 0);
  lv_obj_set_style_bg_opa(grid_, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_all(grid_, 8, 0);
  lv_obj_set_style_pad_gap(grid_, 8, 0);
  lv_obj_set_style_border_width(grid_, 0, 0);

  // Use flex layout with wrapping — switches flow into a grid
  lv_obj_set_flex_flow(grid_, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_flex_align(grid_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  lv_obj_set_scrollbar_mode(grid_, LV_SCROLLBAR_MODE_AUTO);
}

SwitchWidget* SwitchPage::add_switch(const char* label) {
  auto sw = std::make_unique<SwitchWidget>(grid_, label);

  // Size each switch to fill ~1/4 of the width (4 columns on 1024px)
  // minus padding. This adapts naturally to different screen widths.
  lv_obj_set_width(sw->get_obj(), LV_PCT(23));

  auto* ptr = sw.get();
  switches_.push_back(std::move(sw));
  return ptr;
}

}  // namespace sensesp_cockpit_display
