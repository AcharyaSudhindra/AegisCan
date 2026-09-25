#include "web_dashboard.h"
#include "app_config.h"
#include "spi_dma_listener.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_netif.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "WEB_DASHBOARD";
static httpd_handle_t server = NULL;
static int ws_client_fd = -1;
static uint32_t attack_counter = 0;

extern QueueHandle_t violation_queue_ws;

// =============================================================================
// Clean Industrial Light-Theme Dashboard
// =============================================================================
static const char DASHBOARD_HTML[] = 
"<!DOCTYPE html>\n"
"<html lang='en'>\n"
"<head>\n"
"<meta charset='UTF-8'>\n"
"<meta name='viewport' content='width=device-width, initial-scale=1.0'>\n"
"<title>Aegis-CAN Gateway & Telematics</title>\n"
"<style>\n"
"  :root {\n"
"    --bg: #f8fafc;\n"
"    --surface: #ffffff;\n"
"    --border: #e2e8f0;\n"
"    --text-primary: #0f172a;\n"
"    --text-secondary: #475569;\n"
"    --text-muted: #94a3b8;\n"
"    --accent: #2563eb;\n"
"    --success: #059669;\n"
"    --success-bg: #ecfdf5;\n"
"    --danger: #dc2626;\n"
"    --danger-bg: #fef2f2;\n"
"    --mono: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace;\n"
"  }\n"
"  * { box-sizing: border-box; }\n"
"  body {\n"
"    font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;\n"
"    background: var(--bg);\n"
"    color: var(--text-primary);\n"
"    margin: 0;\n"
"    padding: 24px;\n"
"    font-size: 14px;\n"
"    line-height: 1.5;\n"
"  }\n"
"  .container { max-width: 1040px; margin: 0 auto; }\n"
"  .header {\n"
"    display: flex;\n"
"    justify-content: space-between;\n"
"    align-items: center;\n"
"    background: var(--surface);\n"
"    border: 1px solid var(--border);\n"
"    border-radius: 8px;\n"
"    padding: 16px 20px;\n"
"    margin-bottom: 20px;\n"
"  }\n"
"  .title { font-size: 16px; font-weight: 600; color: var(--text-primary); margin: 0; }\n"
"  .subtitle { font-size: 12px; color: var(--text-secondary); margin-top: 2px; }\n"
"  .pill {\n"
"    display: inline-flex;\n"
"    align-items: center;\n"
"    gap: 6px;\n"
"    background: var(--success-bg);\n"
"    color: var(--success);\n"
"    border: 1px solid #bbf7d0;\n"
"    font-size: 12px;\n"
"    font-weight: 500;\n"
"    padding: 4px 10px;\n"
"    border-radius: 9999px;\n"
"  }\n"
"  .grid {\n"
"    display: grid;\n"
"    grid-template-columns: repeat(4, 1fr);\n"
"    gap: 16px;\n"
"    margin-bottom: 20px;\n"
"  }\n"
"  @media(max-width: 800px) { .grid { grid-template-columns: repeat(2, 1fr); } }\n"
"  .card {\n"
"    background: var(--surface);\n"
"    border: 1px solid var(--border);\n"
"    border-radius: 8px;\n"
"    padding: 16px;\n"
"  }\n"
"  .card-label {\n"
"    font-size: 11px;\n"
"    font-weight: 600;\n"
"    text-transform: uppercase;\n"
"    letter-spacing: 0.5px;\n"
"    color: var(--text-secondary);\n"
"    margin-bottom: 6px;\n"
"  }\n"
"  .card-value {\n"
"    font-size: 20px;\n"
"    font-weight: 700;\n"
"    color: var(--text-primary);\n"
"    font-family: var(--mono);\n"
"  }\n"
"  .card-meta {\n"
"    font-size: 12px;\n"
"    color: var(--text-muted);\n"
"    margin-top: 4px;\n"
"  }\n"
"  .panel {\n"
"    background: var(--surface);\n"
"    border: 1px solid var(--border);\n"
"    border-radius: 8px;\n"
"    overflow: hidden;\n"
"  }\n"
"  .panel-header {\n"
"    display: flex;\n"
"    justify-content: space-between;\n"
"    align-items: center;\n"
"    padding: 14px 18px;\n"
"    border-bottom: 1px solid var(--border);\n"
"    background: #fafafa;\n"
"  }\n"
"  .panel-title { font-size: 13px; font-weight: 600; color: var(--text-primary); }\n"
"  .panel-status { font-size: 12px; color: var(--text-secondary); font-family: var(--mono); }\n"
"  table { width: 100%; border-collapse: collapse; font-size: 13px; text-align: left; }\n"
"  th {\n"
"    background: #f1f5f9;\n"
"    color: var(--text-secondary);\n"
"    font-weight: 600;\n"
"    font-size: 11px;\n"
"    text-transform: uppercase;\n"
"    letter-spacing: 0.5px;\n"
"    padding: 10px 16px;\n"
"    border-bottom: 1px solid var(--border);\n"
"  }\n"
"  td {\n"
"    padding: 10px 16px;\n"
"    border-bottom: 1px solid var(--border);\n"
"    font-family: var(--mono);\n"
"    color: var(--text-primary);\n"
"  }\n"
"  tr:last-child td { border-bottom: none; }\n"
"  .badge-danger {\n"
"    background: var(--danger-bg);\n"
"    color: var(--danger);\n"
"    border: 1px solid #fecaca;\n"
"    padding: 2px 6px;\n"
"    border-radius: 4px;\n"
"    font-size: 11px;\n"
"    font-weight: 500;\n"
"  }\n"
"  .footer {\n"
"    display: flex;\n"
"    justify-content: space-between;\n"
"    margin-top: 16px;\n"
"    font-size: 12px;\n"
"    color: var(--text-muted);\n"
"  }\n"
"</style>\n"
"</head>\n"
"<body>\n"
"<div class='container'>\n"
"  <div class='header'>\n"
"    <div>\n"
"      <div class='title'>Aegis-CAN Gateway & Telematics Console</div>\n"
"      <div class='subtitle'>Hardware Telematics Node (ESP32-S3) | Altera Cyclone II FPGA Link</div>\n"
"    </div>\n"
"    <div class='pill'>\n"
"      <span style='display:inline-block;width:6px;height:6px;background:var(--success);border-radius:50%;'></span>\n"
"      Online\n"
"    </div>\n"
"  </div>\n"
"  <div class='grid'>\n"
"    <div class='card'>\n"
"      <div class='card-label'>Interdictions Logged</div>\n"
"      <div class='card-value' id='stat-count'>0</div>\n"
"      <div class='card-meta'>Hardware verified events</div>\n"
"    </div>\n"
"    <div class='card'>\n"
"      <div class='card-label'>SPI Receiver</div>\n"
"      <div class='card-value' style='font-size:16px; color:var(--success);'>ARMED</div>\n"
"      <div class='card-meta'>DMA Mode 0 (GPIO 10,11,12,13)</div>\n"
"    </div>\n"
"    <div class='card'>\n"
"      <div class='card-label'>Octal PSRAM</div>\n"
"      <div class='card-value' id='stat-psram'>8,189 KB</div>\n"
"      <div class='card-meta'>Free heap memory</div>\n"
"    </div>\n"
"    <div class='card'>\n"
"      <div class='card-label'>Network IP</div>\n"
"      <div class='card-value' style='font-size:14px;' id='stat-ip'>--</div>\n"
"      <div class='card-meta'>Station (Sudhindra)</div>\n"
"    </div>\n"
"  </div>\n"
"  <div class='panel'>\n"
"    <div class='panel-header'>\n"
"      <div class='panel-title'>Physical Bus Security Event Log</div>\n"
"      <div class='panel-status' id='ws-indicator'>WebSocket: Connecting...</div>\n"
"    </div>\n"
"    <table>\n"
"      <thead>\n"
"        <tr>\n"
"          <th>Timestamp (Tick)</th>\n"
"          <th>Arbitration ID</th>\n"
"          <th>Frame Type</th>\n"
"          <th>Error Code</th>\n"
"          <th>Action Taken</th>\n"
"        </tr>\n"
"      </thead>\n"
"      <tbody id='log-body'>\n"
"        <tr>\n"
"          <td colspan='5' style='text-align:center; padding:36px; color:var(--text-muted); font-family:sans-serif;'>\n"
"            No physical interdictions recorded. Hardware SPI DMA receiver is listening for FPGA triggers.\n"
"          </td>\n"
"        </tr>\n"
"      </tbody>\n"
"    </table>\n"
"  </div>\n"
"  <div class='footer'>\n"
"    <div>Hardware: ESP32-S3 (Dual Core 240MHz, 16MB Flash, 8MB Octal PSRAM)</div>\n"
"    <div id='stat-uptime'>Uptime: 0s</div>\n"
"  </div>\n"
"</div>\n"
"<script>\n"
"  let eventCount = 0;\n"
"  function connect() {\n"
"    const ws = new WebSocket('ws://' + location.host + '/ws');\n"
"    const ind = document.getElementById('ws-indicator');\n"
"    ws.onopen = () => { ind.textContent = 'WebSocket: Connected'; ind.style.color = 'var(--success)'; };\n"
"    ws.onclose = () => { ind.textContent = 'WebSocket: Disconnected'; ind.style.color = 'var(--danger)'; setTimeout(connect, 3000); };\n"
"    ws.onmessage = (e) => {\n"
"      try {\n"
"        const d = JSON.parse(e.data);\n"
"        eventCount++;\n"
"        document.getElementById('stat-count').textContent = eventCount;\n"
"        const body = document.getElementById('log-body');\n"
"        if (eventCount === 1) body.innerHTML = '';\n"
"        const tr = document.createElement('tr');\n"
"        tr.innerHTML = `<td>${d.tick}</td>` +\n"
"                       `<td><strong>0x${d.id.toString(16).toUpperCase()}</strong></td>` +\n"
"                       `<td>${d.is_ext ? 'Extended (29-bit)' : 'Standard (11-bit)'}</td>` +\n"
"                       `<td>Code ${d.err}</td>` +\n"
"                       `<td><span class='badge-danger'>INTERDICTED &lt;40ns</span></td>`;\n"
"        body.insertBefore(tr, body.firstChild);\n"
"      } catch(err){}\n"
"    };\n"
"  }\n"
"  connect();\n"
"  function updateStatus() {\n"
"    fetch('/api/status').then(r => r.json()).then(d => {\n"
"      if (d.psram_free) document.getElementById('stat-psram').textContent = d.psram_free + ' KB';\n"
"      if (d.ip) document.getElementById('stat-ip').textContent = d.ip;\n"
"      if (d.uptime !== undefined) {\n"
"        const m = Math.floor(d.uptime / 60);\n"
"        const s = d.uptime % 60;\n"
"        document.getElementById('stat-uptime').textContent = 'Uptime: ' + m + 'm ' + s + 's';\n"
"      }\n"
"    }).catch(()=>{});\n"
"  }\n"
"  setInterval(updateStatus, 3000);\n"
"  updateStatus();\n"
"</script>\n"
"</body>\n"
"</html>";

// =============================================================================
// HTTP Handlers
// =============================================================================
static esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, DASHBOARD_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t status_get_handler(httpd_req_t *req) {
    char json[256];
    size_t psram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t sram_free  = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    int64_t uptime_sec = esp_timer_get_time() / 1000000;

    esp_netif_ip_info_t ip_info = {0};
    esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (sta_netif) {
        esp_netif_get_ip_info(sta_netif, &ip_info);
    }

    snprintf(json, sizeof(json),
        "{\"psram_free\":%u,\"sram_free\":%u,\"uptime\":%lld,\"attacks\":%lu,\"ip\":\"" IPSTR "\"}",
        (unsigned int)(psram_free / 1024),
        (unsigned int)(sram_free / 1024),
        (long long)uptime_sec,
        (unsigned long)attack_counter,
        IP2STR(&ip_info.ip)
    );
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) {
        ws_client_fd = httpd_req_to_sockfd(req);
        ESP_LOGI(TAG, "WebSocket client connected: fd=%d", ws_client_fd);
        return ESP_OK;
    }
    return ESP_OK;
}

static const httpd_uri_t uri_root = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_status = {
    .uri       = "/api/status",
    .method    = HTTP_GET,
    .handler   = status_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t uri_ws = {
    .uri          = "/ws",
    .method       = HTTP_GET,
    .handler      = ws_handler,
    .user_ctx     = NULL,
    .is_websocket = true
};

typedef struct {
    httpd_handle_t hd;
    int fd;
    char payload[160];
    int len;
} ws_send_arg_t;

static void ws_send_worker(void *arg) {
    ws_send_arg_t *a = (ws_send_arg_t *)arg;
    httpd_ws_frame_t ws_pkt = {
        .final     = true,
        .fragmented = false,
        .type      = HTTPD_WS_TYPE_TEXT,
        .payload   = (uint8_t *)a->payload,
        .len       = a->len
    };
    esp_err_t res = httpd_ws_send_frame_async(a->hd, a->fd, &ws_pkt);
    if (res != ESP_OK) {
        ESP_LOGW(TAG, "WebSocket send error 0x%x, clearing client fd", res);
        ws_client_fd = -1;
    }
    free(a);
}

esp_err_t web_dashboard_start(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_open_sockets = 5;
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting HTTP & WebSocket server on port %d...", config.server_port);
    esp_err_t ret = httpd_start(&server, &config);
    if (ret == ESP_OK) {
        httpd_register_uri_handler(server, &uri_root);
        httpd_register_uri_handler(server, &uri_status);
        httpd_register_uri_handler(server, &uri_ws);
        ESP_LOGI(TAG, "Dashboard online");
        return ESP_OK;
    }
    ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
    return ret;
}

void task_web_dashboard(void *pvParameters) {
    ESP_LOGI(TAG, "Web Dashboard task running on Core %d", xPortGetCoreID());
    web_dashboard_start();

    violation_descriptor_t desc;

    while (1) {
        if (xQueueReceive(violation_queue_ws, &desc, portMAX_DELAY) == pdTRUE) {
            attack_counter++;

            if (server == NULL || ws_client_fd == -1) {
                continue;
            }

            ws_send_arg_t *arg = malloc(sizeof(ws_send_arg_t));
            if (!arg) {
                continue;
            }

            arg->hd = server;
            arg->fd = ws_client_fd;
            arg->len = snprintf(arg->payload, sizeof(arg->payload),
                "{\"tick\":%lu,\"id\":%lu,\"is_ext\":%d,\"err\":%d,\"count\":%lu}",
                (unsigned long)desc.timestamp_24,
                (unsigned long)desc.arb_id,
                (int)desc.is_extended,
                (int)desc.error_code,
                (unsigned long)attack_counter
            );

            esp_err_t res = httpd_queue_work(server, ws_send_worker, arg);
            if (res != ESP_OK) {
                free(arg);
            }
        }
    }
}
