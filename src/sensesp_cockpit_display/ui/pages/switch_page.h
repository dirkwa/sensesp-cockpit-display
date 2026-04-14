#pragma once

#include <vector>
#include <memory>
#include <functional>
#include "lvgl.h"
#include "../widgets/switch_widget.h"

namespace sensesp_cockpit_display {

/// A page displaying a grid of SwitchWidgets.
/// Automatically adapts columns to the screen width.
class SwitchPage {
 public:
  /// Create the switch page on the given parent (typically a tabview page).
  explicit SwitchPage(lv_obj_t* parent);

  /// Add a switch to the grid. Returns a pointer to the widget for binding.
  SwitchWidget* add_switch(const char* label);

 private:
  lv_obj_t* grid_;
  std::vector<std::unique_ptr<SwitchWidget>> switches_;
};

}  // namespace sensesp_cockpit_display
