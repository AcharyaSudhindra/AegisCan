# Manual FPGA Firewall Demo

This is a separate DE2 bitstream for demonstrating the FPGA blacklist without a CAN physical layer. It uses the same preloaded standard-ID policy as `bram_whitelist.v`: `0x100`, `0x010`, `0x001`, and `0x1AA` block; other standard IDs accept.

1. Program `manual_firewall_demo.sof` through USB-Blaster.
2. Use `SW[10:0]` as an 11-bit hexadecimal CAN ID. The latched ID appears on `LEDR[10:0]` and `HEX2:HEX0`.
3. Press and release **KEY0** to request one check. The press is debounced for 20 ms.
4. `LEDG0 = ACCEPT`; `LEDG1 = BLOCK`; `LEDG2 = ALERT`; `LEDG3 = CHECK COMPLETE`. `LEDR17` is on while the press/check is pending.
5. KEY1 is active-low reset, clearing the displayed result.

Examples: `0x100` means SW8 on and all other SW0..SW10 off; it lights BLOCK and ALERT. `0x050` means SW6, SW4 on; it lights ACCEPT. The switch positions are not a CAN frame and this demo does not demonstrate CAN timing, packet reception, or an external ECU response.
