#include "remote_log.h"

#include <cstring>
#include <cstdarg>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

namespace sensesp_cockpit_display {

static int s_listen_fd = -1;
static int s_client_fd = -1;  // only one client at a time
static vprintf_like_t s_prev_printer = nullptr;

// Log hook — called by ESP_LOGx. Forwards to the TCP client and falls
// through to the previous printer (typically the UART).
static int log_printer(const char* fmt, va_list args) {
  va_list args_copy;
  va_copy(args_copy, args);

  // 1. Call previous printer (USB serial)
  int n = 0;
  if (s_prev_printer) {
    n = s_prev_printer(fmt, args);
  }

  // 2. Also send to TCP client if connected
  if (s_client_fd >= 0) {
    char buf[256];
    int len = vsnprintf(buf, sizeof(buf), fmt, args_copy);
    if (len > 0) {
      if (len >= (int)sizeof(buf)) len = sizeof(buf) - 1;
      ssize_t sent = send(s_client_fd, buf, len, MSG_NOSIGNAL);
      if (sent < 0) {
        close(s_client_fd);
        s_client_fd = -1;
      }
    }
  }

  va_end(args_copy);
  return n;
}

static void accept_task(void* arg) {
  uint16_t port = (uint16_t)(uintptr_t)arg;

  s_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
  if (s_listen_fd < 0) {
    vTaskDelete(nullptr);
    return;
  }

  int one = 1;
  setsockopt(s_listen_fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

  struct sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(port);
  if (bind(s_listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
    close(s_listen_fd);
    s_listen_fd = -1;
    vTaskDelete(nullptr);
    return;
  }
  listen(s_listen_fd, 1);

  ESP_LOGI("remote_log", "TCP log server listening on port %u", port);

  while (true) {
    struct sockaddr_in client_addr;
    socklen_t alen = sizeof(client_addr);
    int fd = accept(s_listen_fd, (struct sockaddr*)&client_addr, &alen);
    if (fd < 0) {
      vTaskDelay(pdMS_TO_TICKS(100));
      continue;
    }

    // Drop previous client if any
    if (s_client_fd >= 0) {
      close(s_client_fd);
    }
    s_client_fd = fd;

    const char* banner =
        "# sensesp-cockpit remote log — streaming...\r\n";
    send(s_client_fd, banner, strlen(banner), MSG_NOSIGNAL);
  }
}

void remote_log_start(uint16_t port) {
  s_prev_printer = esp_log_set_vprintf(log_printer);
  xTaskCreate(accept_task, "remote_log", 4096,
              (void*)(uintptr_t)port, 3, nullptr);
}

}  // namespace sensesp_cockpit_display
