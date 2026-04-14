#pragma once

#include <functional>
#include "sensesp/signalk/signalk_value_listener.h"
#include "sensesp/signalk/signalk_put_request.h"
#include "sensesp/system/lambda_consumer.h"
#include "../ui/widgets/switch_widget.h"

namespace sensesp_cockpit_display {

/// Bidirectional binding between a SwitchWidget and a SignalK switch path.
///
/// Listens for state changes from the server (PGN 127501 → SignalK delta)
/// and updates the widget. When the user taps the widget, sends a PUT
/// request to toggle the switch (→ PGN 127502 → N2K bus).
class SKSwitchBinding {
 public:
  /// @param sk_path Full SignalK path, e.g. "electrical.switches.bank.6.1.state"
  /// @param widget The SwitchWidget to bind to
  /// @param listen_delay_ms How often to accept updates from the server
  SKSwitchBinding(const String& sk_path, SwitchWidget* widget,
                  int listen_delay_ms = 500);

 private:
  sensesp::IntSKListener listener_;
  sensesp::SKPutRequest<int> put_request_;
  SwitchWidget* widget_;
  int current_state_ = 0;
};

}  // namespace sensesp_cockpit_display
