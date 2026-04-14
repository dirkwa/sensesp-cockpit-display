#pragma once

#include "lvgl.h"

namespace sensesp_cockpit_display {

/// A marine instrument gauge: label + large value + unit.
class GaugeWidget {
 public:
  GaugeWidget(lv_obj_t* parent, const char* label, const char* unit);

  void set_value(float value, int decimals = 1);
  void set_stale(bool stale);

  lv_obj_t* get_obj() { return container_; }

 private:
  lv_obj_t* container_;
  lv_obj_t* label_;
  lv_obj_t* value_;
  lv_obj_t* unit_;
  bool stale_ = true;
};

}  // namespace sensesp_cockpit_display
