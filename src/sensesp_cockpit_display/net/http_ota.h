#pragma once

#include <cstdint>
#include <functional>

namespace sensesp_cockpit_display {

/// Callback invoked at the start of an OTA upload so the application can
/// quiesce other SDIO/WiFi-heavy work (e.g. stop the candump TCP server
/// and the TWAI receiver) before the OTA write begins. Set via
/// `set_ota_quiesce_callback` before calling `http_ota_start`.
using OtaQuiesceCallback = std::function<void()>;

void set_ota_quiesce_callback(OtaQuiesceCallback cb);

/// Start an HTTP OTA update server on the given port.
/// Upload firmware with:
///   curl --data-binary @firmware.bin http://<ip>:8080/update
///
/// Uses TCP (not UDP like ArduinoOTA), so it works reliably over
/// slow/lossy WiFi like esp_hosted SDIO.
void http_ota_start(uint16_t port = 8080);

}  // namespace sensesp_cockpit_display
