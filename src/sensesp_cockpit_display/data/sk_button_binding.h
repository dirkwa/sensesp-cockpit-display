#pragma once

#include "sensesp/signalk/signalk_value_listener.h"
#include "sensesp/signalk/signalk_put_request.h"
#include "sensesp/system/lambda_consumer.h"
#include "../ui/widgets/switch_button.h"

namespace sensesp_cockpit_display {

/// Bidirectional binding for SwitchButton ↔ SignalK path.
class SKButtonBinding {
 public:
  SKButtonBinding(const String& sk_path, SwitchButton* btn,
                  int listen_delay_ms = 500);

 private:
  sensesp::IntSKListener listener_;
  sensesp::SKPutRequest<int> put_request_;
  SwitchButton* btn_;
};

}  // namespace sensesp_cockpit_display
