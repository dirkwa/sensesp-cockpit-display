#include "sk_button_binding.h"
#include "esp_log.h"

static const char* TAG = "sk_btn";

namespace sensesp_cockpit_display {

SKButtonBinding::SKButtonBinding(const String& sk_path, SwitchButton* btn,
                                 int listen_delay_ms)
    : listener_(sk_path, listen_delay_ms),
      put_request_(sk_path, "", false),
      btn_(btn) {

  listener_.connect_to(new sensesp::LambdaConsumer<int>(
      [this](int new_state) {
        btn_->set_state(new_state != 0);
      }));

  btn_->set_on_toggle([this, sk_path](bool new_state) {
    ESP_LOGI(TAG, "PUT %s = %d", sk_path.c_str(), (int)new_state);
    put_request_.set(new_state ? 1 : 0);
  });
}

}  // namespace sensesp_cockpit_display
