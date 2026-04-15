#include "switch_button.h"
#include "../theme.h"

namespace sensesp_cockpit_display {

SwitchButton::SwitchButton(lv_obj_t* parent, const char* label_text) {
  button_ = lv_button_create(parent);
  lv_obj_set_size(button_, LV_PCT(100), 52);
  lv_obj_set_style_radius(button_, 6, 0);
  lv_obj_set_style_border_width(button_, 1, 0);
  lv_obj_set_style_border_color(button_, theme::separator(), 0);
  lv_obj_set_style_shadow_width(button_, 0, 0);
  lv_obj_add_event_cb(button_, event_cb, LV_EVENT_CLICKED, this);

  label_ = lv_label_create(button_);
  lv_label_set_text(label_, label_text);
  lv_label_set_long_mode(label_, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(label_, LV_PCT(100));
  lv_obj_set_style_text_align(label_, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_font(label_, &lv_font_montserrat_14, 0);
  lv_obj_center(label_);

  apply_style();
}

void SwitchButton::set_state(bool on) {
  state_ = on;
  apply_style();
}

void SwitchButton::apply_style() {
  if (state_) {
    lv_obj_set_style_bg_color(button_, theme::on_color(), 0);
    lv_obj_set_style_text_color(label_, lv_color_white(), 0);
  } else {
    lv_obj_set_style_bg_color(button_, theme::panel(), 0);
    lv_obj_set_style_text_color(label_, theme::label(), 0);
  }
}

void SwitchButton::event_cb(lv_event_t* e) {
  auto* self = static_cast<SwitchButton*>(lv_event_get_user_data(e));
  self->state_ = !self->state_;
  self->apply_style();
  if (self->on_toggle_) self->on_toggle_(self->state_);
}

}  // namespace sensesp_cockpit_display
