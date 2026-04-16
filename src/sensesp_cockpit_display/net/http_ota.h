#pragma once

#include <cstdint>

namespace sensesp_cockpit_display {

/// Start an HTTP OTA update server on the given port.
/// Upload firmware with:
///   curl -F "firmware=@firmware.bin" http://<ip>:8080/update
///
/// Uses TCP (not UDP like ArduinoOTA), so it works reliably over
/// slow/lossy WiFi like esp_hosted SDIO.
void http_ota_start(uint16_t port = 8080);

}  // namespace sensesp_cockpit_display
