# Aegis-CAN: Hardware-Enforced Cyber-Physical Firewall

**Nirmaan Hackathon Track 1 Submission**

**Current implementation status:** this is a prototype with FPGA ID-based blocking and ESP telemetry. The victim firmware now separates FPGA kill reports from software rule observations; a report is not proof of victim-frame rejection or a measured <40 ns latency. The detector is rule-based, UDS currently supports TesterPresent only, and MQTT requires a configured broker/CA. See [victim corrections, supported behavior, and wiring requirements](esp/CORRECTIONS.md). The feature descriptions below include the original project goals and should not be treated as end-to-end verification.

Aegis-CAN is a dual-chip, hardware-accelerated firewall designed to protect critical Controller Area Network (CAN) infrastructures in vehicular and industrial control systems. It provides robust defense against packet injection, arbitration spoofing, and Denial-of-Service (DoS) attacks.

Traditional software-based CAN firewalls suffer from interrupt latency, often allowing malicious frames to be fully transmitted before action can be taken. Aegis-CAN solves this by offloading physical-layer bit interdiction to a dedicated FPGA, while an ESP32-S3 serves as a secure, out-of-band telemetry and cryptographic logging coprocessor.

---

## 1. Executive Summary & Threat Model

The Controller Area Network protocol is inherently vulnerable due to its broadcast nature and lack of native authentication. Any compromised Electronic Control Unit (ECU) can spoof messages or monopolize the bus. 

Aegis-CAN is designed to mitigate the following threat vectors:
*   **Arbitration Spoofing:** Malicious nodes transmitting frames with high-priority IDs (e.g., 0x000) to control critical vehicle functions.
*   **Bus-Off Attacks (DoS):** Attackers deliberately triggering error states in legitimate ECUs to force them offline.
*   **Payload Injection:** Insertion of unauthorized data into legitimate control streams.

By analyzing the bus at the physical layer, Aegis-CAN detects unauthorized IDs and timing anomalies in real-time, deliberately violating the CAN protocol rules (by asserting a dominant bit) to destroy the malicious frame before the payload can be processed by other ECUs on the network.

---

## 2. System Architecture

Aegis-CAN employs an asymmetric, purpose-built architecture that segregates the workload between real-time line defense and secure telematics.

### 2.1. Physical Layer Defense (Altera DE2 Cyclone II)
*   **Hardware Engine:** Altera DE2 FPGA (Cyclone II EP2C35F672C6)
*   **Role:** Inline physical-layer sentinel.
*   **Mechanism of Action:** The FPGA state machine reads the RX line of the CAN transceiver natively. It matches the arbitration ID against a hardware-compiled whitelist/blacklist. 
*   **Interdiction Latency:** If a malicious signature is detected, the FPGA asserts a dominant bit (logic 0) onto the TX line in sub-40ns. At standard CAN speeds (e.g., 500kbps with a 2-microsecond bit time), this instantly triggers a Bit Error or Form Error, forcing the bus into an error frame state and neutralizing the threat before the data phase is transmitted.
*   **Data Handoff:** Following an interdiction, the FPGA packages the attack vector into a 64-bit metadata payload and pushes it to the ESP32-S3 via a high-speed SPI bus.

### 2.2. Telematics & Cryptography Engine (ESP32-S3)
*   **Hardware Engine:** ESP32-S3 N16R8 (16MB Flash, 8MB Octal PSRAM)
*   **Role:** Secure Telemetry Coprocessor and Web Server.
*   **Firmware Subsystems (FreeRTOS):**
    *   **SPI DMA Listener:** A high-priority FreeRTOS task ingests violation packets from the FPGA. It implements strict software-level noise filtering, actively dropping floating-pin transients (e.g., 0xFFFFFFFF) to prevent queue overflow and kernel panics when the FPGA is disconnected.
    *   **Cryptographic Audit Trail:** Security logs cannot be trusted if they are stored in plaintext. The ESP32-S3 encrypts all interdiction events using AES-256-GCM before writing them to non-volatile LittleFS storage, ensuring forensic integrity.
    *   **Dual-Band Connectivity:** The network stack utilizes a failover topology. It connects to primary infrastructure Wi-Fi (Station Mode) for wide-area uplink, while simultaneously broadcasting an isolated fallback Access Point (Aegis-CAN-Sentry) for localized operator access in dead zones.
    *   **Web HUD & WebSocket Server:** Hosts a lightweight, industrial-grade HTTP server delivering a clean, light-themed engineering dashboard. Telemetry and violation logs are streamed asynchronously to connected clients via WebSockets, eliminating polling overhead.

---

## 3. Telemetry & Hardware Profiling

The Aegis-CAN Web Dashboard completely rejects simulated data or artificial intelligence estimations. It exposes authentic, low-level hardware metrics directly from the ESP-IDF backend:
*   **Memory Profiling:** Live tracking of both the 8MB Octal PSRAM (used for high-capacity ring buffers and TinyML models) and internal SRAM.
*   **System Diagnostics:** Tracks continuous uptime, FreeRTOS task states, and total interdiction counts.
*   **Attack Signatures:** Displays the raw 64-bit hexadecimal payloads received via SPI, allowing engineers to reverse-engineer attack patterns in real-time.

---

## 4. Hardware Setup & Interface Specification

### SPI Bridge Pinout (FPGA to ESP32-S3)
The communication backbone between the FPGA and the ESP32-S3 relies on a 4-wire SPI configuration. The ESP32-S3 is configured with internal pull-up and pull-down resistors to ensure line stability.

| Signal             | ESP32-S3 Pin | Altera DE2 GPIO | Description                               |
| :----------------- | :----------- | :-------------- | :---------------------------------------- |
| CS (Chip Select)   | GPIO 10      | User Assigned   | Active-low frame synchronization signal.  |
| MOSI               | GPIO 11      | User Assigned   | Master Out Slave In (Data from FPGA).     |
| MISO               | GPIO 12      | User Assigned   | Master In Slave Out (Data to FPGA).       |
| SCLK (Clock)       | GPIO 13      | User Assigned   | SPI Synchronization Clock.                |

---

## 5. Build, Compilation, and Deployment

Building the firmware for the ESP32-S3 requires strict adherence to the ESP-IDF toolchain limitations and the N16R8 hardware profile.

### 5.1. Workspace Configuration (Critical)
The ESP-IDF CMake and Ninja build systems strictly prohibit whitespace in directory paths. Compilation will instantly fail if this rule is violated.
*   Invalid Path: `C:\Projects\Aegis can\esp\`
*   Valid Path: `C:\Aegis_ESP_Build\`

All project code must be synchronized to a whitespace-free directory prior to executing `idf.py build`.

### 5.2. Flashing the ESP32-S3 N16R8
The 16MB Flash / 8MB Octal PSRAM variant of the ESP32-S3 requires the flash mode to be explicitly set to Dual I/O (DIO). Relying on the default Quad I/O (QIO) mode will result in a fatal `ets_loader.c 78` ROM crash loop upon boot.

Execute the following command sequence to safely write the firmware:

```bash
# 1. Compile the firmware
idf.py build

# 2. Flash to the MCU (Ensure DIO mode is specified)
esptool.py -p COM7 -b 460800 --before default_reset --after hard_reset \
  --chip esp32s3 write_flash --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/aegis_can.bin
