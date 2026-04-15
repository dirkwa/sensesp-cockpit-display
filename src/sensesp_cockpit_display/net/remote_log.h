#pragma once

#include <cstdint>

namespace sensesp_cockpit_display {

/// Start a TCP log server that mirrors ESP_LOGx output to any connected client.
/// Connect from anywhere on the LAN with: `nc <esp-ip> 2323`
///
/// Non-blocking, uses a small FreeRTOS task. Logs also continue to go
/// to the USB serial port unchanged.
void remote_log_start(uint16_t port = 2323);

}  // namespace sensesp_cockpit_display
