#include "gauge_widget.h"
#include "../theme.h"
#include <cstdio>

namespace sensesp_cockpit_display {

GaugeWidget::GaugeWidget(lv_obj_t* parent, const char* label_text,
                         const char* unit_text) {
  container_ = lv_obj_create(parent);
  lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(container_, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_bg_color(container_, theme::panel(), 0);
  lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(container_, 8, 0);
  lv_obj_set_style_border_width(container_, 1, 0);
  lv_obj_set_style_border_color(container_, theme::separator(), 0);
  lv_obj_set_style_pad_all(container_, 8, 0);
  lv_obj_set_style_pad_row(container_, 2, 0);
  lv_obj_set_width(container_, LV_PCT(100));

  // Label at top
  label_ = lv_label_create(container_);
  lv_label_set_text(label_, label_text);
  lv_obj_set_style_text_color(label_, theme::label(), 0);
  lv_obj_set_style_text_font(label_, &lv_font_montserrat_14, 0);

  // Large value
  value_ = lv_label_create(container_);
  lv_label_set_text(value_, "---");
  lv_obj_set_style_text_color(value_, theme::label(), 0);  // gray until data
  lv_obj_set_style_text_font(value_, &lv_font_montserrat_36, 0);

  // Unit at bottom
  unit_ = lv_label_create(container_);
  lv_label_set_text(unit_, unit_text);
  lv_obj_set_style_text_color(unit_, theme::separator(), 0);
  lv_obj_set_style_text_font(unit_, &lv_font_montserrat_14, 0);
}

void GaugeWidget::set_value(float val, int decimals) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%.*f", decimals, val);
  lv_label_set_text(value_, buf);
  if (stale_) {
    lv_obj_set_style_text_color(value_, theme::value(), 0);
    stale_ = false;
  }
}

void GaugeWidget::set_stale(bool stale) {
  stale_ = stale;
  if (stale) {
    lv_label_set_text(value_, "---");
    lv_obj_set_style_text_color(value_, theme::label(), 0);
  }
}

}  // namespace sensesp_cockpit_display
