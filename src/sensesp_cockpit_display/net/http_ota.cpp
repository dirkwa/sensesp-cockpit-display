#include "http_ota.h"

#include <cstring>
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_server.h"

static const char* TAG = "http_ota";

namespace sensesp_cockpit_display {

static esp_err_t ota_post_handler(httpd_req_t* req) {
  ESP_LOGW(TAG, "OTA update started, size=%d", req->content_len);

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

    // Brief yield after each chunk — esp_hosted SDIO WiFi stalls under
    // sustained continuous inbound reads. A small delay lets the SDIO
    // driver drain its buffers (same pattern that makes MJPEG streaming
    // work at 400KB/s while raw bulk upload stalls).
    vTaskDelay(pdMS_TO_TICKS(2));

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

void http_ota_start(uint16_t port) {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = port;
  config.stack_size = 8192;
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

  ESP_LOGI(TAG, "HTTP OTA server on port %u — curl -F firmware=@file http://ip:%u/update",
           port, port);
}

}  // namespace sensesp_cockpit_display
