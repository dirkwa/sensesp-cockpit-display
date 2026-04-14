#pragma once

#include <functional>
#include "lvgl.h"

namespace sensesp_cockpit_display {

/// A toggle switch widget: label + large switch + status text.
/// Designed for marine digital switching panels.
class SwitchWidget {
 public:
  SwitchWidget(lv_obj_t* parent, const char* label);

  void set_state(bool on);
  bool get_state() const;

  /// Called when the user taps the switch. The callback receives the NEW state.
  void set_on_toggle(std::function<void(bool)> cb) { on_toggle_ = cb; }

  lv_obj_t* get_obj() { return container_; }

 private:
  static void event_cb(lv_event_t* e);

  lv_obj_t* container_;
  lv_obj_t* label_;
  lv_obj_t* sw_;
  lv_obj_t* status_;
  std::function<void(bool)> on_toggle_;
};

}  // namespace sensesp_cockpit_display
