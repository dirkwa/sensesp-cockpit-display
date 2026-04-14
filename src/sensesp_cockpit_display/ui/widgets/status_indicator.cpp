#include "status_indicator.h"
#include "../theme.h"

namespace sensesp_cockpit_display {

static lv_color_t level_color(StatusLevel level) {
  switch (level) {
    case StatusLevel::kOk:      return theme::on_color();
    case StatusLevel::kWarning: return theme::pending();
    case StatusLevel::kError:   return theme::off_color();
    case StatusLevel::kUnknown:
    default:                    return theme::separator();
  }
}

StatusIndicator::StatusIndicator(lv_obj_t* parent, const char* label_text) {
  container_ = lv_obj_create(parent);
  lv_obj_set_size(container_, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(container_, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_bg_color(container_, theme::panel(), 0);
  lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(container_, 6, 0);
  lv_obj_set_style_border_width(container_, 1, 0);
  lv_obj_set_style_border_color(container_, theme::separator(), 0);
  lv_obj_set_style_pad_hor(container_, 12, 0);
  lv_obj_set_style_pad_ver(container_, 10, 0);
  lv_obj_set_style_pad_column(container_, 12, 0);

  // Status dot (LED widget)
  dot_ = lv_led_create(container_);
  lv_obj_set_size(dot_, 18, 18);
  lv_led_set_color(dot_, theme::separator());
  lv_led_on(dot_);

  // Label
  label_ = lv_label_create(container_);
  lv_label_set_text(label_, label_text);
  lv_obj_set_style_text_color(label_, theme::label(), 0);
  lv_obj_set_style_text_font(label_, &lv_font_montserrat_20, 0);
  lv_obj_set_flex_grow(label_, 1);

  // Status text (right-aligned via flex)
  status_ = lv_label_create(container_);
  lv_label_set_text(status_, "unknown");
  lv_obj_set_style_text_color(status_, theme::value(), 0);
  lv_obj_set_style_text_font(status_, &lv_font_montserrat_20, 0);
}

void StatusIndicator::set_status(StatusLevel level, const char* text) {
  lv_led_set_color(dot_, level_color(level));
  lv_label_set_text(status_, text);
  lv_obj_set_style_text_color(status_, level_color(level), 0);
}

}  // namespace sensesp_cockpit_display
