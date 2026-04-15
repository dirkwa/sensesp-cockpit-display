#pragma once

#include <vector>
#include <memory>
#include "lvgl.h"
#include "../widgets/switch_button.h"

namespace sensesp_cockpit_display {

/// A switch page using lightweight SwitchButton widgets.
/// Fits ~30+ switches per page depending on layout.
class ButtonSwitchPage {
 public:
  explicit ButtonSwitchPage(lv_obj_t* parent);
  SwitchButton* add_switch(const char* label);

 private:
  lv_obj_t* grid_;
  std::vector<std::unique_ptr<SwitchButton>> switches_;
};

}  // namespace sensesp_cockpit_display
