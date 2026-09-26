# AEGIS-CPS: Master Hardware Wiring Guide (DUAL ESP32-S3 ARCHITECTURE)

This document contains the final physical wiring connections for the entire Aegis-CPS Hackathon project, updated to use an ESP32-S3 for both the Attacker and the Victim/Dashboard, utilizing both Level Shifters.

---

## 🛡️ NODE 1: THE DEFENDER & DASHBOARD
**(FPGA + ESP32-S3 Victim + MCP2515 #1 + Level Shifter #1)**

This node intercepts CAN traffic, filters it through the FPGA hardware, and reports status to the ESP32-S3 WiFi Dashboard.

**Firmware correction:** the connections below provide FPGA/SPI reporting only. The victim ESP's native CAN controller additionally needs a compatible transceiver on GPIO4 (TX) / GPIO5 (RX). Do not tie multiple controller TX outputs together. See [victim corrections and bench requirements](esp/CORRECTIONS.md) before changing the circuit. GPIO9 IRQ is not currently used by the firmware to trigger alerts.

### 1. FPGA (Altera DE2 JP1 Header) ➔ ESP32 & Level Shifter #1
*All connections are on the bottom-most pins of the `JP1` Expansion Header (closest to the RS232 port).*

| FPGA Pin | Signal Name | Wire Color | Connects To... |
| :--- | :--- | :--- | :--- |
| **Pin 1** (Outer) | `CAN_RX` | 🟢 Green | Level Shifter #1 **`LV1`** |
| **Pin 2** (Inner) | `CAN_TX` | 🟡 Yellow | MCP2515 #1 ➔ **TJA1050 Pin 1 (TXD)** |
| **Pin 3** (Outer) | `SPI_CLK` | 🔴 Red | ESP32-S3 (Victim) **GPIO 12** |
| **Pin 4** (Inner) | `SPI_MOSI` | 🔵 Blue | ESP32-S3 (Victim) **GPIO 11** |
| **Pin 5** (Outer) | `SPI_CS_N` | 🟣 Purple | ESP32-S3 (Victim) **GPIO 10** |
| **Pin 6** (Inner) | `IRQ_OUT` | 🟠 Orange | ESP32-S3 (Victim) **GPIO 9** *(Triggers Red Alert!)* |

### 2. Level Shifter #1 (CAN RX Protection)
*Protects the FPGA's 3.3V CAN_RX pin from the 5V CAN network.*
* **`LV`:** ➔ ESP32-S3 (Victim) **`3V3`**
* **`HV`:** ➔ External **`5V`** Source
* **`GND`:** ➔ System **`GND`** (Shared)
* **`LV1`:** ➔ FPGA Pin 1 (Green Wire)
* **`HV1`:** ➔ MCP2515 #1 ➔ **TJA1050 Pin 4 (RXD)**

### 3. MCP2515 Module #1 (Defender Bus Connection)
* **`VCC`:** ➔ **`5V`** Source
* **`GND`:** ➔ System **`GND`**

---

## 🏴‍☠️ NODE 2: THE ATTACKER 
**(ESP32-S3 Attacker + MCP2515 #2 + Level Shifter #2)**

This node runs the Attacker script to inject malicious spoofed frames. Since the ESP32-S3 is 3.3V and the MCP2515 is 5V, we use Level Shifter #2 for the SPI lines.

### 1. Level Shifter #2 Power
* **`LV`:** ➔ ESP32-S3 (Attacker) **`3V3`**
* **`HV`:** ➔ External **`5V`**
* **`GND`:** ➔ Shared **`GND`**

### 2. SPI Data Translation (ESP32-S3 ➔ LS #2 ➔ MCP2515 #2)

| ESP32-S3 (Attacker) | Level Shifter #2 (Low) | Level Shifter #2 (High) | MCP2515 #2 | Function |
| :--- | :--- | :--- | :--- | :--- |
| **GPIO 10** | ➔ `LV1` | `HV1` ➔ | **`CS`** | Chip Select |
| **GPIO 11** | ➔ `LV2` | `HV2` ➔ | **`SI`** | MOSI |
| **GPIO 12** | ➔ `LV3` | `HV3` ➔ | **`SCK`** | SPI Clock |
| **GPIO 13** | ➔ `LV4` | `HV4` ➔ | **`SO`** | MISO |

### 3. MCP2515 Module #2 Power
* **`VCC`:** ➔ **`5V`**
* **`GND`:** ➔ Shared **`GND`**
* **`INT`:** ➔ ESP32-S3 (Attacker) **GPIO 2** *(Optional)*

---

## 🌐 THE CAN BUS (Connecting the Networks)

Bridge the two networks together using the green screw terminals on both MCP2515 modules:
* **Module 1 `CAN_H`** ⟷ **Module 2 `CAN_H`**
* **Module 1 `CAN_L`** ⟷ **Module 2 `CAN_L`**

---
*End of Document.*
