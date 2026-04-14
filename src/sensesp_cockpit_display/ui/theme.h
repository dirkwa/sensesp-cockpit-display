#pragma once

#include "lvgl.h"

namespace sensesp_cockpit_display {

/// Marine-optimized dark color theme. Use inline functions because
/// lv_color_make() isn't constexpr.
namespace theme {
  inline lv_color_t bg()        { return lv_color_make(20, 25, 35); }
  inline lv_color_t panel()     { return lv_color_make(30, 38, 50); }
  inline lv_color_t label()     { return lv_color_make(120, 140, 160); }
  inline lv_color_t value()     { return lv_color_make(220, 240, 255); }
  inline lv_color_t on_color()  { return lv_color_make(40, 200, 80); }
  inline lv_color_t off_color() { return lv_color_make(220, 60, 60); }
  inline lv_color_t pending()   { return lv_color_make(220, 200, 40); }
  inline lv_color_t separator() { return lv_color_make(50, 60, 75); }
  inline lv_color_t header()    { return lv_color_make(25, 32, 45); }
}

}  // namespace sensesp_cockpit_display
