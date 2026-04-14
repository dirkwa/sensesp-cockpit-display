#include "sk_switch_binding.h"
#include "esp_log.h"

static const char* TAG = "sk_switch";

namespace sensesp_cockpit_display {

SKSwitchBinding::SKSwitchBinding(const String& sk_path, SwitchWidget* widget,
                                 int listen_delay_ms)
    : listener_(sk_path, listen_delay_ms),
      put_request_(sk_path, "", false),
      widget_(widget) {

  // Server → widget: when the switch state changes on the N2K bus,
  // update the widget to reflect the new state.
  listener_.connect_to(new sensesp::LambdaConsumer<int>(
      [this](int new_state) {
        current_state_ = new_state;
        widget_->set_state(new_state != 0);
        ESP_LOGD(TAG, "%s = %d", listener_.get_sk_path().c_str(), new_state);
      }));

  // Widget → server: when the user taps the switch, send a PUT request
  // to toggle it.
  widget_->set_on_toggle([this](bool new_state) {
    int val = new_state ? 1 : 0;
    ESP_LOGI(TAG, "PUT %s = %d", listener_.get_sk_path().c_str(), val);
    put_request_.set(val);
  });
}

}  // namespace sensesp_cockpit_display
