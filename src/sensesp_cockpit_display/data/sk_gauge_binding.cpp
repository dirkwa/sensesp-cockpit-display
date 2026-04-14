#include "sk_gauge_binding.h"
#include "esp_log.h"

namespace sensesp_cockpit_display {

SKGaugeBinding::SKGaugeBinding(const String& sk_path, GaugeWidget* widget,
                               std::function<float(float)> converter,
                               int decimals, int listen_delay_ms)
    : listener_(sk_path, listen_delay_ms),
      widget_(widget),
      converter_(converter),
      decimals_(decimals) {

  listener_.connect_to(new sensesp::LambdaConsumer<float>(
      [this](float raw) {
        float display_val = converter_ ? converter_(raw) : raw;
        widget_->set_value(display_val, decimals_);
      }));
}

}  // namespace sensesp_cockpit_display
