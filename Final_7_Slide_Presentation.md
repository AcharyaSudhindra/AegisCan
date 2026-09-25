# 🏆 Aegis-CPS: 7-Slide Hackathon Final Presentation Deck

> **How to use this deck:** Copy the **Bullet Points** directly onto your slides, use the **Visual/Diagram cues** for your graphics/screenshots, and read the **Speaker Notes** during your pitch to the judges.

---

## SLIDE 1: Title & Executive Overview (The Hook)

### 📌 Slide Text
* **Project Title:** **Aegis-CPS: Multi-Tier Hardware-Accelerated Cyber-Physical Firewall & Cryptographic Telemetry Gateway**
* **Subtitle:** Shifting CAN Bus Intrusion Prevention from Millisecond Software to Sub-180ns Silicon Logic
* **Lead Architect:** Sudhindra M Acharya | B.E. Electronics Engineering (VLSI Design & Technology)
* **Domains:** VLSI Digital Design (FPGA RTL) • Embedded RTOS Systems • Automotive Cyber-Physical Security
* **Headline Prototype Metrics:**
  * ⚡ **`< 180 ns`** Reaction & Frame-Kill Latency (50 MHz FPGA Clock)
  * 🧱 **`435 Logic Elements`** (1.3% utilization on Altera Cyclone II `EP2C35F672C6`)
  * 🔒 **`AES-256-GCM`** Hardware-Accelerated Black-Box Forensics on ESP32-S3
  * 🎯 **`0 µs`** Bus Forwarding Delay (Non-intrusive parallel bus tap)

### 🎨 Visual on Slide
* High-impact photo of your physical 4-node hardware testbench + GitHub QR code linking to `github.com/AcharyaSudhindra/AegisCan`.

### 🎙️ Speaker Script (30 sec)
> *"Good morning judges. Modern vehicles and industrial robots run on the CAN bus—a 1980s protocol with zero authentication. Today's automotive firewalls run in software, taking milliseconds to detect an attack—by which time the malicious frame has already engaged the brakes or steering. We built Aegis-CPS: a bare-metal FPGA and Dual-Core ESP32-S3 security architecture that detects and physically destroys malicious CAN frames mid-flight in under 180 nanoseconds."*

---

## SLIDE 2: The Problem & The "Millisecond Failure Gap"

### 📌 Slide Text
* **The Core Vulnerability:**
  * CAN 2.0B is a broadcast bus with **no sender authentication or encryption**.
  * Any compromised node (Infotainment, Telematics, OBD-II port) can spoof safety-critical ECU IDs (e.g., `0x050` Brakes, `0x100` Engine RPM).
* **Why Software & MCU Firewalls Fail:**
  * **Store-and-Forward Delay:** Traditional MCU gateways must receive the *entire* 108-bit CAN frame (`~216 µs` at 500 kbps) + CRC before inspecting it.
  * **OS & Interrupt Jitter:** Linux/RTOS interrupt latency adds `500 µs – 5 ms` of non-deterministic delay.
  * **The Fatal Flaw:** In a broadcast CAN bus, the Victim ECU receives the frame at the *exact same instant* as the software firewall. **Detecting an attack after the frame finishes is too late—the car has already crashed.**
* **The Aegis-CPS Paradigm Shift:**
  * Inspect bits **on-the-fly** during the 11-bit Arbitration Field and **destroy the frame on the physical wire** before the Data and CRC fields are ever transmitted.

### 🎨 Visual on Slide
* Timeline comparison diagram:
  * **Top bar (Standard CAN Frame = 216 µs):** `[SOF][11-bit ID (22µs)][Control][64-bit Data][15-bit CRC][ACK][EOF]`
  * **Red Arrow at 22 µs + 180 ns:** *"Aegis-CPS FPGA Kills Frame Here!"*
  * **Gray Arrow at 1,500 µs:** *"Software IDS detects attack here (Too Late — ECU already actuated)"*

---

## SLIDE 3: Multi-Tier Defense-in-Depth Architecture

### 📌 Slide Text
* **Tier 1: The Silicon Sentry (Altera Cyclone II FPGA @ 50 MHz)**
  * **10x Bit-Timing Logic (`can_btl.v`):** Triple-majority voting oversampler immune to analog bus noise.
  * **Live De-Stuffer & ID Deserializer (`can_destuffer.v`):** Strips protocol stuff bits in real time and extracts 11-bit Standard / 29-bit Extended IDs on the exact final clock edge.
  * **Single-Cycle BRAM Whitelist (`bram_whitelist.v`):** 2,048-entry M4K Dual-Port RAM lookup table (`O(1)` latency = `20 ns`).
  * **Active Dominant Overdrive (`kill_wire_ctrl.v`):** Pulls `can_tx` dominant (`0`) for 6 consecutive bit periods (`12 µs`), intentionally inducing a CAN Bit-Stuffing Error that forces all ECUs to discard the corrupted packet instantly.
* **Tier 2: Cryptographic Telemetry & AI Gateway (ESP32-S3 N16R8)**
  * Receives 64-bit violation descriptors (`ID + Error Code + 32-bit Timestamp`) via **12.5 MHz SPI DMA**.
  * Performs **AES-256-GCM** encryption, **LittleFS** flash logging, **TinyML** burst anomaly scoring, and hosts a live **WebSocket SOC Dashboard**.

### 🎨 Visual on Slide
* Block diagram showing the 4 hardware nodes: `Wireless Attacker ESP32-S3` ➔ `CAN Bus` ➔ `TJA1050 Transceiver Tap` ➔ `Cyclone II FPGA (Tier 1)` ➔ `12.5MHz SPI DMA` ➔ `ESP32-S3 Gateway (Tier 2)`.

---

## SLIDE 4: VLSI Hardware Implementation & Post-Synthesis Silicon Metrics

### 📌 Slide Text
* **Target FPGA:** Altera DE2 — Cyclone II `EP2C35F672C6` (Quartus II 13.0 SP1 Verified Synthesis)
* **Actual Post-Synthesis Resource Utilization:**
  * **Total Logic Elements (LEs):** `435 / 33,216` (**1.31%** of chip!)
  * **Combinational Functions:** `363`
  * **Dedicated Logic Registers:** `322` (**< 1%**)
  * **Embedded M4K Memory Bits:** `2,048 / 483,840` (**< 0.5%**)
  * **I/O Pins Used:** `11` (`clk`, `rst_n`, `can_rx`, `can_tx`, 4-wire `SPI` + `IRQ`, 3 diagnostic `LEDs`)
* **Cycle-Exact Deterministic Pipeline Latency (`50 MHz` = `20 ns` / clock):**
  1. Bit-Timing Synchronizer & Majority Vote: **1 clk (`20 ns`)**
  2. Dynamic Bit De-Stuffer & Deserializer Latch: **1 clk (`20 ns`)**
  3. M4K BRAM Whitelist Address Lookup: **1 clk (`20 ns`)**
  4. Kill-Wire FSM Dominant Overdrive Output: **1 clk (`20 ns`)**
  * **Total Internal Logic Latency:** **`80 ns`** *(4.5x faster than our `< 180 ns` specification and **25x faster** than a single 2,000 ns CAN bit!)*

### 🎨 Visual on Slide
* Screenshot of your **Quartus II Flow Summary report** (`435 LEs`, `Flow Status: Successful`) alongside the RTL block schematic.

---

## SLIDE 5: Dual-Core FreeRTOS Firmware & Interactive Hardware Testbed

### 📌 Slide Text
* **Asymmetric Dual-Core FreeRTOS Architecture (ESP32-S3):**
  * **Core 0 (Deterministic I/O & Protocol Stack):**
    * `spi_dma_listener` (`Prio 9`): Zero-CPU-copy DMA reception triggered by FPGA `irq_out`.
    * `can_tp` & `uds_server` (`Prio 7-8`): ISO 15765-2 multi-frame transport + ISO 14229 UDS (`0x27` Seed-Key Auth & `0x2E` Dynamic Whitelist Provisioning).
  * **Core 1 (Cryptography, Storage & Edge AI):**
    * `aes_gcm_engine` (`Prio 6`): Hardware-accelerated AES-256-GCM authenticated payload sealing.
    * `flight_recorder` (`Prio 5`): Automotive crash-proof circular log on wear-leveled LittleFS flash.
    * `tinyml_anomaly` (`Prio 3`): Sliding-window Z-score statistical detector for high-frequency bus flooding.
* **Live 4-Node Hackathon Hardware Setup:**
  * **Wireless Attacker Node:** Spare ESP32-S3 hosting a SoftAP Hacker Portal (`Aegis_Attacker`) injecting live RPM spoofing (`0x100`), Brake cut (`0x050`), and Bus DoS (`0x000`) frames via MCP2515.
  * **Parasitic Transceiver Tap:** FPGA directly overrides the TJA1050 `TXD/RXD` lines through bidirectional MOSFET level shifters.

### 🎨 Visual on Slide
* Side-by-side screenshot of the **Red Attacker Injection UI** on smartphone and the **Aegis-CPS Live Security Dashboard** logging blocked attacks in real time.

---

## SLIDE 6: Verification, Logic Analyzer Proof & Competitive Benchmarking

### 📌 Slide Text
* **End-to-End Verification Suite:**
  * **Simulation:** 6 self-checking Icarus Verilog (`iverilog`) testbenches verifying bit-stuffing edge cases, BRAM boundary hits, and 12.5 MHz SPI DMA framing.
  * **Physical Bus Proof (24 MS/s USB Logic Analyzer):**
    * Captures `can_rx` (Attacker frame `0x100`), `can_tx` (FPGA Kill Wire assertion at bit 11), and immediate `spi_clk`/`spi_mosi` DMA bursts at **48 samples per CAN bit**.
* **Benchmarking Against Existing Solutions:**

| Metric / Capability | Linux SocketCAN IDS | Dual-Port MCU Firewall | **Aegis-CPS (Ours)** |
|---|---|---|---|
| **Reaction Latency** | `1,000,000+ ns` (1–5 ms) | `130,000+ ns` (130 µs) | **`< 180 ns` (80 ns core)** |
| **Stops Current Frame?** | ❌ No (Post-attack alert) | ⚠️ Only via Store-and-Forward | ✅ **Yes (Mid-frame wire kill)** |
| **Added Bus Latency** | `0 µs` (Passive) | `+200 µs` (Breaks timing) | **`0 µs` (Parallel inline tap)** |
| **Fail-Safe Behavior** | OS hang drops bus | MCU crash severs bus | **High-Z passive fail-open** |
| **Hardware Footprint** | Full MPU + Linux | High-speed Cortex-M7 | **435 FPGA LEs (~3.5K gates)** |

### 🎨 Visual on Slide
* Waveform capture (`aegis_top.vcd` / 24MHz Logic Analyzer) highlighting the exact nanosecond `can_tx` drops low to crush the spoofed ID.

---

## SLIDE 7: ASIC Commercial Viability, Scalability & Impact

### 📌 Slide Text
* **Ultra-Low Silicon Area ➔ Ready for ASIC Tape-Out:**
  * At just **435 Logic Elements (~3,500 NAND2-equivalent gates)**, the entire Aegis-CPS Tier 1 core occupies **`< 0.05 mm²`** in a standard 65nm/130nm CMOS node.
  * Can be embedded directly inside commercial CAN transceiver dies (next-gen TJA1050 / SN65HVD230) as a **"Smart Secure Transceiver"** adding **`< $0.15`** at volume manufacturing.
* **Plug-and-Play Legacy Retrofit:**
  * Requires **zero modifications to existing vehicle ECUs** or wiring harnesses—connects in parallel like an OBD-II security dongle.
* **Future Roadmap (Tier 3 Integration):**
  * Integration of TI Sitara PRU-ICSS (`200 MHz`) for **physical-layer clock-skew and ADC voltage transient fingerprinting**—detecting even compromised legitimate ECUs attempting to spoof peer IDs.
* **Target Industries:**
  * 🚗 **Passenger EVs & Autonomous Vehicles** (ISO/SAE 21434 compliance)
  * 🏭 **Industrial Robotics & Factory Automation** (CANopen / DeviceNet protection)
  * ✈️ **Defense & Unmanned Aerial Systems** (UAV actuator bus hardening)

### 🎨 Visual on Slide
* Roadmap graphic: `Current Prototype (Cyclone II + ESP32-S3)` ➔ `OBD-II Fleet Retrofit Module` ➔ `0.05 mm² ASIC Inside Every Automotive Transceiver`.
