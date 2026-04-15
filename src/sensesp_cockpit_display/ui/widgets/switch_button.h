#pragma once

#include <functional>
#include "lvgl.h"

namespace sensesp_cockpit_display {

/// Lightweight switch widget: a single button that changes color
/// between green (ON) and dim (OFF) when tapped. Uses only lv_button
/// and lv_label — no nested containers or lv_switch.
/// About 3x smaller in RAM than SwitchWidget.
class SwitchButton {
 public:
  SwitchButton(lv_obj_t* parent, const char* label);

  void set_state(bool on);
  bool get_state() const { return state_; }
  void set_on_toggle(std::function<void(bool)> cb) { on_toggle_ = cb; }

  lv_obj_t* get_obj() { return button_; }

 private:
  static void event_cb(lv_event_t* e);
  void apply_style();

  lv_obj_t* button_;
  lv_obj_t* label_;
  bool state_ = false;
  std::function<void(bool)> on_toggle_;
};

}  // namespace sensesp_cockpit_display
