#pragma once

#include <vector>
#include <memory>
#include "lvgl.h"
#include "../widgets/gauge_widget.h"

namespace sensesp_cockpit_display {

/// A page displaying a grid of instrument gauges.
class InstrumentPage {
 public:
  explicit InstrumentPage(lv_obj_t* parent);

  GaugeWidget* add_gauge(const char* label, const char* unit);

 private:
  lv_obj_t* grid_;
  std::vector<std::unique_ptr<GaugeWidget>> gauges_;
};

}  // namespace sensesp_cockpit_display
