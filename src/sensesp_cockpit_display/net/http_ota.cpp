#include "http_ota.h"

#include <cstring>
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_server.h"
// esp_hosted_ota.h is plain C; wrap so the C++ name mangling doesn't break linking.
extern "C" {
#include "esp_hosted_ota.h"
}

static const char* TAG = "http_ota";

namespace sensesp_cockpit_display {

static OtaQuiesceCallback g_quiesce_cb;

void set_ota_quiesce_callback(OtaQuiesceCallback cb) {
  g_quiesce_cb = std::move(cb);
}

static esp_err_t ota_post_handler(httpd_req_t* req) {
  ESP_LOGW(TAG, "OTA update started, size=%d", req->content_len);

  // Quiesce SDIO/WiFi-heavy tasks before the OTA write begins. Without
  // this, candump TCP TX + TWAI rx + RPC polling saturate SDIO and the
  // OTA recv stalls. After OTA succeeds we reboot so resume is moot.
  if (g_quiesce_cb) {
    ESP_LOGW(TAG, "Quiescing background tasks for OTA");
    g_quiesce_cb();
  }

  esp_ota_handle_t ota_handle;
  const esp_partition_t* update_partition =
      esp_ota_get_next_update_partition(nullptr);
  if (!update_partition) {
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "No OTA partition");
    return ESP_FAIL;
  }

  esp_err_t err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_ota_begin failed: %s", esp_err_to_name(err));
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA begin failed");
    return ESP_FAIL;
  }

  char buf[4096];
  int received = 0;
  int total = req->content_len;

  while (received < total) {
    int len = httpd_req_recv(req, buf, sizeof(buf));
    if (len <= 0) {
      if (len == HTTPD_SOCK_ERR_TIMEOUT) continue;
      ESP_LOGE(TAG, "Receive error at %d/%d", received, total);
      esp_ota_abort(ota_handle);
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Receive error");
      return ESP_FAIL;
    }

    err = esp_ota_write(ota_handle, buf, len);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "OTA write failed: %s", esp_err_to_name(err));
      esp_ota_abort(ota_handle);
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Write failed");
      return ESP_FAIL;
    }

    received += len;

    if (received % (100 * 1024) < 4096) {
      ESP_LOGI(TAG, "OTA progress: %d/%d (%d%%)", received, total,
               (int)(100LL * received / total));
    }
  }

  err = esp_ota_end(ota_handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "OTA end failed: %s", esp_err_to_name(err));
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA end failed");
    return ESP_FAIL;
  }

  err = esp_ota_set_boot_partition(update_partition);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Set boot partition failed: %s", esp_err_to_name(err));
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "Set boot failed");
    return ESP_FAIL;
  }

  ESP_LOGW(TAG, "OTA complete! Rebooting in 1s...");
  httpd_resp_sendstr(req, "OK — rebooting\n");

  vTaskDelay(pdMS_TO_TICKS(1000));
  esp_restart();
  return ESP_OK;
}

// Slave (C6) firmware update via SDIO. POST the network_adapter.bin
// (built from esp_hosted/slave/) to /update-slave. The host streams
// chunks straight to esp_hosted_slave_ota_write() over the existing
// SDIO link; no UART/IO9 jumper needed. After verify, activate +
// reboot the host so the new slave gets used cleanly.
static esp_err_t slave_ota_post_handler(httpd_req_t* req) {
  ESP_LOGW(TAG, "Slave OTA started, size=%d", req->content_len);

  // Quiesce N2K/candump so the SDIO link is free for the OTA bytes.
  if (g_quiesce_cb) {
    ESP_LOGW(TAG, "Quiescing background tasks for slave OTA");
    g_quiesce_cb();
  }

  esp_err_t err = esp_hosted_slave_ota_begin();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_hosted_slave_ota_begin: %s", esp_err_to_name(err));
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "slave OTA begin failed");
    return ESP_FAIL;
  }

  char buf[4096];
  int received = 0;
  int total = req->content_len;

  while (received < total) {
    int len = httpd_req_recv(req, buf, sizeof(buf));
    if (len <= 0) {
      if (len == HTTPD_SOCK_ERR_TIMEOUT) continue;
      ESP_LOGE(TAG, "Slave OTA recv error at %d/%d", received, total);
      esp_hosted_slave_ota_end();  // best-effort cleanup
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                          "recv error");
      return ESP_FAIL;
    }
    err = esp_hosted_slave_ota_write((uint8_t*)buf, len);
    if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_hosted_slave_ota_write: %s",
               esp_err_to_name(err));
      esp_hosted_slave_ota_end();
      httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                          "slave write failed");
      return ESP_FAIL;
    }
    received += len;
    if (received % (100 * 1024) < 4096) {
      ESP_LOGI(TAG, "Slave OTA progress: %d/%d (%d%%)", received, total,
               (int)(100LL * received / total));
    }
  }

  err = esp_hosted_slave_ota_end();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_hosted_slave_ota_end: %s", esp_err_to_name(err));
    httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                        "slave end failed");
    return ESP_FAIL;
  }

  // Activate is supported on slave fw >= 2.6.0; ours is >= 2.12 so safe.
  err = esp_hosted_slave_ota_activate();
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "esp_hosted_slave_ota_activate: %s (continuing)",
             esp_err_to_name(err));
  }

  ESP_LOGW(TAG, "Slave OTA complete. Rebooting host in 1s to re-sync.");
  httpd_resp_sendstr(req, "OK — host rebooting to sync with new slave\n");
  vTaskDelay(pdMS_TO_TICKS(1000));
  esp_restart();
  return ESP_OK;
}

void http_ota_start(uint16_t port) {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = port;
  config.stack_size = 8192;
  // Each httpd instance needs a UNIQUE ctrl_port (internal control socket).
  // Default is 32768; shift by +port-offset so we don't collide with the
  // SensESP main HTTP server's default ctrl_port (which is 32768).
  config.ctrl_port = 32768 + port;
  // Large timeouts for slow esp_hosted WiFi
  config.recv_wait_timeout = 120;
  config.send_wait_timeout = 30;
  config.lru_purge_enable = true;
  config.max_uri_handlers = 4;

  httpd_handle_t server = nullptr;
  if (httpd_start(&server, &config) != ESP_OK) {
    ESP_LOGE(TAG, "Failed to start HTTP OTA server on port %u", port);
    return;
  }

  httpd_uri_t ota_uri = {
      .uri = "/update",
      .method = HTTP_POST,
      .handler = ota_post_handler,
      .user_ctx = nullptr,
  };
  httpd_register_uri_handler(server, &ota_uri);

  httpd_uri_t slave_ota_uri = {
      .uri = "/update-slave",
      .method = HTTP_POST,
      .handler = slave_ota_post_handler,
      .user_ctx = nullptr,
  };
  httpd_register_uri_handler(server, &slave_ota_uri);

  ESP_LOGI(TAG, "HTTP OTA server on port %u (host: /update, slave: /update-slave)",
           port);
}

}  // namespace sensesp_cockpit_display
