#pragma once

#include "lvgl.h"

namespace sensesp_cockpit_display {

enum class StatusLevel {
  kOk,       // green
  kWarning,  // yellow
  kError,    // red
  kUnknown,  // gray
};

/// A status indicator: colored dot + label + status text.
class StatusIndicator {
 public:
  StatusIndicator(lv_obj_t* parent, const char* label);

  void set_status(StatusLevel level, const char* text);

  lv_obj_t* get_obj() { return container_; }

 private:
  lv_obj_t* container_;
  lv_obj_t* dot_;
  lv_obj_t* label_;
  lv_obj_t* status_;
};

}  // namespace sensesp_cockpit_display
