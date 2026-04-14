#include "switch_widget.h"
#include "../theme.h"

namespace sensesp_cockpit_display {

SwitchWidget::SwitchWidget(lv_obj_t* parent, const char* label_text) {
  // Container panel
  container_ = lv_obj_create(parent);
  lv_obj_set_size(container_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(container_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_bg_color(container_, theme::panel(), 0);
  lv_obj_set_style_bg_opa(container_, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(container_, 8, 0);
  lv_obj_set_style_border_width(container_, 1, 0);
  lv_obj_set_style_border_color(container_, theme::separator(), 0);
  lv_obj_set_style_pad_all(container_, 12, 0);
  lv_obj_set_style_pad_row(container_, 8, 0);
  lv_obj_set_width(container_, LV_PCT(100));

  // Label
  label_ = lv_label_create(container_);
  lv_label_set_text(label_, label_text);
  lv_obj_set_style_text_color(label_, theme::label(), 0);
  lv_obj_set_style_text_font(label_, &lv_font_montserrat_16, 0);

  // Switch — large touch target
  sw_ = lv_switch_create(container_);
  lv_obj_set_size(sw_, 80, 40);
  lv_obj_set_style_bg_color(sw_, theme::off_color(), 0);
  lv_obj_set_style_bg_color(sw_, theme::on_color(),
                            LV_PART_INDICATOR | LV_STATE_CHECKED);
  lv_obj_add_event_cb(sw_, event_cb, LV_EVENT_VALUE_CHANGED, this);

  // Status text
  status_ = lv_label_create(container_);
  lv_label_set_text(status_, "OFF");
  lv_obj_set_style_text_color(status_, theme::off_color(), 0);
  lv_obj_set_style_text_font(status_, &lv_font_montserrat_14, 0);
}

void SwitchWidget::set_state(bool on) {
  if (on) {
    lv_obj_add_state(sw_, LV_STATE_CHECKED);
    lv_label_set_text(status_, "ON");
    lv_obj_set_style_text_color(status_, theme::on_color(), 0);
  } else {
    lv_obj_remove_state(sw_, LV_STATE_CHECKED);
    lv_label_set_text(status_, "OFF");
    lv_obj_set_style_text_color(status_, theme::off_color(), 0);
  }
}

bool SwitchWidget::get_state() const {
  return lv_obj_has_state(sw_, LV_STATE_CHECKED);
}

void SwitchWidget::event_cb(lv_event_t* e) {
  auto* self = static_cast<SwitchWidget*>(lv_event_get_user_data(e));
  bool new_state = lv_obj_has_state(self->sw_, LV_STATE_CHECKED);

  // Update visual
  if (new_state) {
    lv_label_set_text(self->status_, "ON");
    lv_obj_set_style_text_color(self->status_, theme::on_color(), 0);
  } else {
    lv_label_set_text(self->status_, "OFF");
    lv_obj_set_style_text_color(self->status_, theme::off_color(), 0);
  }

  // Notify callback
  if (self->on_toggle_) {
    self->on_toggle_(new_state);
  }
}

}  // namespace sensesp_cockpit_display
