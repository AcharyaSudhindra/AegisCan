# Victim firmware corrections and remaining bench requirements

## What each indication means

- **FPGA kill report:** a complete SPI descriptor matching the FPGA's current format was received. It reports that the FPGA asserted its kill output. It does not measure latency or prove that a victim ECU rejected the frame.
- **Rule observation:** software heuristics found a timing/value change in a received CAN frame. No trained ML model and no software-to-FPGA blocking command are implemented.
- **SPI initialized:** the slave driver and receive buffers were initialized. There is no FPGA ready/ack handshake, CRC, or sequence number in the existing 64-bit interface, so delivery cannot be guaranteed or every missing/corrupt packet detected.
- **CAN driver running:** the ESP TWAI driver is running. This does not verify wiring, termination, oscillator settings, acknowledgement, or delivery.
- **Saved records:** complete fixed-size records recovered from disk, plus writes successfully flushed and synced this boot. This count is separate from received reports.

## Diagnostics

Only standard data frames addressed to 0x7E0 enter the diagnostic parser; replies use 0x7E8. Requests are bounded to 64 bytes with length, sequence, and timeout checks. TesterPresent (0x3E/0x00) is supported, including suppression of a positive response. SecurityAccess and data-ID read/write services return service-not-supported. Their previous unlock/configuration-success responses were placeholders. Responses longer than seven bytes are explicitly rejected, not partially transmitted.

## Logging and MQTT

Existing files are preserved on mount/write failures. Automatic formatting is disabled. A blank/unformatted flight_log partition therefore needs deliberate first-time filesystem initialization before logging is available; the dashboard reports this state. Storage exhaustion stops logging rather than deleting older evidence. Metadata is reconstructed from complete records after restart; an incomplete final record is excluded.

The encrypted record container remains 104 bytes. New records encrypt a deterministic 64-byte payload: version=1 at byte 0, source at 1, extended flag at 2, error at 3, big-endian FPGA tick at 4..7, ID at 8..11, ESP microseconds at 12..19, sequence at 20..23; other bytes zero. Older records used a native C structure and require the legacy decoder. The existing prototype AES key is retained to avoid silently losing access to old records. This is not production key provisioning or a tamper-evident log chain.

MQTT is disabled until a real broker URI and trusted CA are configured in app_config.h. No invented flash usage is published. Local logging/dashboard continue without MQTT.

## Physical wiring still required

The current wiring guide connects the victim ESP to the FPGA over SPI only. Native TWAI additionally requires ESP GPIO4 (TX) and GPIO5 (RX) connected through a compatible external CAN transceiver. Do **not** tie GPIO4, the FPGA output, and an MCP2515 TX output together on one transceiver TXD pin. Use a separately driven transceiver or a designed arbitration/gating circuit. Confirm the exact module schematic before altering wiring. GPIO5 must receive a 3.3 V-safe RX signal. All nodes require the appropriate shared ground and bus termination.

GPIO9 IRQ from the FPGA is currently informational; firmware reception is prequeued SPI DMA, not an IRQ-driven red-alert input. GPIO13 is not needed by the FPGA's one-way SPI telemetry link.

Builds use PlatformIO espressif32 6.10.0. DIO flash mode is consistent with the project's documented N16R8 setup; PSRAM configuration comes from sdkconfig.defaults. Confirm the actual module variant before uploading.

## Verification

`node tests/test_dashboard.cjs` tests unavailable/zero memory readings, backend counts, service state, and stale status after disconnection.

`gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -I main tests/test_protocols.c -o /tmp/aegis_test_protocols && /tmp/aegis_test_protocols` tests valid/malformed FPGA packets, portable event encoding, ISO-TP lengths/sequences/timeouts, and 100,000 malformed parser inputs.

`gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined -I tests/stubs -I main tests/test_uds.c main/uds_server.c -o /tmp/aegis_test_uds && /tmp/aegis_test_uds` tests TesterPresent and negative diagnostic responses against the real UDS handler. Run these commands from the esp directory in WSL/Linux; the stubs replace only ESP logging/error definitions.

PlatformIO Core is installed locally at `C:\AEIGIS\.tools\pio-venv\Scripts\platformio.exe`; its packages are in `C:\AEIGIS\.tools\platformio`. To build from PowerShell:

```powershell
$env:PLATFORMIO_CORE_DIR = 'C:\AEIGIS\.tools\platformio'
& C:\AEIGIS\.tools\pio-venv\Scripts\platformio.exe run -d C:\AEIGIS\AegisCan\esp
```

A successful firmware build or these tests do not establish end-to-end CAN protection. Bench verification must compare allowed and blocked frames at a separately connected CAN receiver, correlate FPGA/SPI reports, test disconnected inputs, and check logging across reboot.
