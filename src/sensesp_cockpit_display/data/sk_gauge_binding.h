#pragma once

#include <functional>
#include "sensesp/signalk/signalk_value_listener.h"
#include "sensesp/system/lambda_consumer.h"
#include "../ui/widgets/gauge_widget.h"

namespace sensesp_cockpit_display {

/// Binds a GaugeWidget to a SignalK path with optional unit conversion.
class SKGaugeBinding {
 public:
  /// @param sk_path SignalK path, e.g. "navigation.speedOverGround"
  /// @param widget The gauge widget to update
  /// @param converter Unit conversion function (e.g. m/s → knots), nullptr for raw
  /// @param decimals Number of decimal places to display
  /// @param listen_delay_ms Update interval
  SKGaugeBinding(const String& sk_path, GaugeWidget* widget,
                 std::function<float(float)> converter = nullptr,
                 int decimals = 1, int listen_delay_ms = 1000);

 private:
  sensesp::FloatSKListener listener_;
  GaugeWidget* widget_;
  std::function<float(float)> converter_;
  int decimals_;
};

// Common unit converters
inline float rad_to_deg(float rad) { return rad * 57.2957795f; }
inline float ms_to_knots(float ms) { return ms * 1.94384f; }
inline float kelvin_to_c(float k) { return k - 273.15f; }
inline float pa_to_hpa(float pa) { return pa / 100.0f; }
inline float ratio_to_pct(float r) { return r * 100.0f; }

}  // namespace sensesp_cockpit_display
