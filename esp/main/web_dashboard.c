#include "web_dashboard.h"
#include "app_config.h"
#include "spi_dma_listener.h"
#include "can_tp.h"
#include "flight_recorder.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_netif.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
static httpd_handle_t server;
static atomic_uint fpga_reports, rule_reports;

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
"      <span id='online'>Status unknown</span>\n"
"    </div>\n"
"  </div>\n"
"  <div class='grid'>\n"
"    <div class='card'>\n"
"      <div class='card-label'>FPGA Kill Reports</div>\n"
"      <div class='card-value' id='stat-count'>0</div>\n"
"      <div class='card-meta'>Received since ESP boot; not proof of victim rejection</div>\n"
"    </div>\n"
"    <div class='card'>\n"
"      <div class='card-label'>SPI Receiver</div>\n"
"      <div class='card-value' id='spi-state' style='font-size:16px;'>UNKNOWN</div>\n"
"      <div class='card-meta'>DMA Mode 0 (GPIO 10,11,12,13)</div>\n"
"    </div>\n"
"    <div class='card'>\n"
"      <div class='card-label'>Octal PSRAM</div>\n"
"      <div class='card-value' id='stat-psram'>--</div>\n"
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
"      <div class='panel-title'>Received FPGA Reports and Rule Observations</div>\n"
"      <div class='panel-status' id='ws-indicator'>WebSocket: Connecting...</div>\n"
"    </div>\n"
"    <table>\n"
"      <thead>\n"
"        <tr>\n"
"          <th>ESP Time (µs)</th>\n"
"          <th>Arbitration ID</th>\n"
"          <th>Frame Type</th>\n"
"          <th>Error Code</th>\n"
"          <th>Action Taken</th>\n"
"        </tr>\n"
"      </thead>\n"
"      <tbody id='log-body'>\n"
"        <tr>\n"
"          <td colspan='5' style='text-align:center; padding:36px; color:var(--text-muted); font-family:sans-serif;'>\n"
"            No events received by this browser session.\n"
"          </td>\n"
"        </tr>\n"
"      </tbody>\n"
"    </table>\n"
"  </div>\n"
"  <div id='health'>Waiting for measured status...</div><div class='footer'>\n"
"    <div>Hardware: ESP32-S3 (Dual Core 240MHz, 16MB Flash, 8MB Octal PSRAM)</div>\n"
"    <div id='stat-uptime'>Uptime: 0s</div>\n"
"  </div>\n"
"</div>\n"
"<script>\n"
"  let rows = 0;\n"
"  function connect() {\n"
"    const ws = new WebSocket('ws://' + location.host + '/ws');\n"
"    const ind = document.getElementById('ws-indicator');\n"
"    ws.onopen = () => { ind.textContent = 'WebSocket: Connected'; };\n"
"    ws.onclose = () => { ind.textContent = 'WebSocket: Disconnected'; setTimeout(connect, 3000); };\n"
"    ws.onmessage = (e) => {\n"
"      try {\n"
"        const d = JSON.parse(e.data);\n"
"        if (!Number.isInteger(d.id) || ![1, 2].includes(d.source)) return;\n"
"        const body = document.getElementById('log-body');\n"
"        if (rows++ === 0) body.innerHTML = '';\n"
"        const row = document.createElement('tr');\n"
"        const values = [d.esp_us, '0x' + d.id.toString(16).toUpperCase(),\n"
"          d.is_ext ? 'Extended' : 'Standard', d.err,\n"
"          d.source === 1 ? 'FPGA kill asserted (reported)' : 'Rule anomaly (not blocked)'];\n"
"        values.forEach(value => {\n"
"          const cell = document.createElement('td');\n"
"          cell.textContent = String(value);\n"
"          row.appendChild(cell);\n"
"        });\n"
"        body.insertBefore(row, body.firstChild);\n"
"        while (body.children.length > 200) body.removeChild(body.lastChild);\n"
"        updateStatus();\n"
"      } catch (err) {}\n"
"    };\n"
"  }\n"
"  async function updateStatus() {\n"
"    try {\n"
"      const response = await fetch('/api/status', {cache: 'no-store'});\n"
"      if (!response.ok) throw new Error('Status unavailable');\n"
"      const d = await response.json();\n"
"      document.getElementById('online').textContent = 'ESP responding';\n"
"      document.getElementById('stat-count').textContent = d.fpga_reports;\n"
"      document.getElementById('spi-state').textContent = d.spi_ready ? 'INITIALIZED' : 'UNAVAILABLE';\n"
"      document.getElementById('stat-psram').textContent = d.psram_free + ' KB';\n"
"      document.getElementById('stat-ip').textContent = d.ip;\n"
"      document.getElementById('stat-uptime').textContent = 'Uptime: ' + d.uptime + 's';\n"
"      document.getElementById('health').textContent =\n"
"        'CAN driver: ' + (d.can_ready ? 'running (wiring unverified)' : 'unavailable / stopped') +\n"
"        ' | Log: ' + (d.log_ready ? 'ready' : 'unavailable') +\n"
"        ' | Saved records: ' + d.logged + ' | Log errors/drops: ' + d.log_errors +\n"
"        ' | Rejected SPI packets: ' + d.spi_rejected + ' | SPI queue drops: ' + d.spi_dropped +\n"
"        ' | Rule observations: ' + d.rule_reports;\n"
"    } catch (err) {\n"
"      document.getElementById('online').textContent = 'Status unavailable — readings may be stale';\n"
"      document.getElementById('spi-state').textContent = 'UNKNOWN';\n"
"    }\n"
"  }\n"
"  connect();\n"
"  setInterval(updateStatus, 3000);\n"
"  updateStatus();\n"
"</script>\n"
"</body>\n"
"</html>\n";

static esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, DASHBOARD_HTML, HTTPD_RESP_USE_STRLEN);
}
static esp_err_t status_get_handler(httpd_req_t *req) {
    char json[640];
    esp_netif_ip_info_t ip = {0};
    esp_netif_t *net = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (net) esp_netif_get_ip_info(net, &ip);
    snprintf(json, sizeof(json),
        "{\"psram_free\":%u,\"uptime\":%lld,\"fpga_reports\":%u,\"rule_reports\":%u,"
        "\"spi_ready\":%s,\"spi_rejected\":%u,\"spi_dropped\":%u,"
        "\"can_ready\":%s,\"log_ready\":%s,\"logged\":%u,\"log_errors\":%u,\"ip\":\"" IPSTR "\"}",
        (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)/1024),
        (long long)(esp_timer_get_time()/1000000),
        atomic_load(&fpga_reports), atomic_load(&rule_reports),
        spi_listener_ready() ? "true" : "false", (unsigned)spi_listener_rejected(),
        (unsigned)spi_listener_dropped(), can_tp_ready() ? "true" : "false",
        flight_recorder_ready() ? "true" : "false", (unsigned)flight_recorder_get_count(),
        (unsigned)flight_recorder_errors(), IP2STR(&ip.ip));
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}
static esp_err_t ws_handler(httpd_req_t *req) {
    if (req->method == HTTP_GET) return ESP_OK;
    httpd_ws_frame_t packet = {0};
    esp_err_t ret = httpd_ws_recv_frame(req, &packet, 0);
    // This is an outbound event stream. Close unexpected inbound data.
    if (ret != ESP_OK || packet.len > 0) return ESP_FAIL;
    return ESP_OK;
}
static const httpd_uri_t root_uri = {.uri="/", .method=HTTP_GET, .handler=root_get_handler};
static const httpd_uri_t status_uri = {.uri="/api/status", .method=HTTP_GET, .handler=status_get_handler};
static const httpd_uri_t ws_uri = {.uri="/ws", .method=HTTP_GET, .handler=ws_handler, .is_websocket=true};
typedef struct { char payload[192]; size_t len; } ws_event_t;
static void broadcast(void *opaque) {
    ws_event_t *event = opaque;
    int clients[5]; size_t count = 5;
    if (httpd_get_client_list(server, &count, clients) == ESP_OK) {
        for (size_t i=0; i<count; ++i) {
            if (httpd_ws_get_fd_info(server, clients[i]) != HTTPD_WS_CLIENT_WEBSOCKET) continue;
            httpd_ws_frame_t frame = {.final=true, .type=HTTPD_WS_TYPE_TEXT,
                                     .payload=(uint8_t *)event->payload, .len=event->len};
            if (httpd_ws_send_frame_async(server, clients[i], &frame) != ESP_OK)
                httpd_sess_trigger_close(server, clients[i]);
        }
    }
    free(event);
}
esp_err_t web_dashboard_start(void) {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_open_sockets = 5; cfg.lru_purge_enable = true;
    esp_err_t ret = httpd_start(&server, &cfg);
    if (ret != ESP_OK) return ret;
    if ((ret = httpd_register_uri_handler(server, &root_uri)) != ESP_OK ||
        (ret = httpd_register_uri_handler(server, &status_uri)) != ESP_OK ||
        (ret = httpd_register_uri_handler(server, &ws_uri)) != ESP_OK) {
        httpd_stop(server); server = NULL;
    }
    return ret;
}
void task_web_dashboard(void *unused) {
    if (web_dashboard_start() != ESP_OK) ESP_LOGE("WEB", "HTTP server unavailable");
    violation_descriptor_t event;
    while (1) {
        if (xQueueReceive(violation_queue_ws, &event, portMAX_DELAY) != pdTRUE) continue;
        if (event.source == EVENT_SOURCE_FPGA) atomic_fetch_add(&fpga_reports, 1);
        else if (event.source == EVENT_SOURCE_RULE) atomic_fetch_add(&rule_reports, 1);
        else continue;
        if (!server) continue;
        ws_event_t *message = malloc(sizeof(*message));
        if (!message) continue;
        int len = snprintf(message->payload, sizeof(message->payload),
            "{\"tick\":%lu,\"id\":%lu,\"is_ext\":%u,\"err\":%u,\"source\":%u,\"esp_us\":%lld}",
            (unsigned long)event.timestamp_24, (unsigned long)event.arb_id,
            event.is_extended, event.error_code, event.source, (long long)event.esp_timestamp_us);
        if (len < 0 || (size_t)len >= sizeof(message->payload)) { free(message); continue; }
        message->len = len;
        if (httpd_queue_work(server, broadcast, message) != ESP_OK) free(message);
    }
}
