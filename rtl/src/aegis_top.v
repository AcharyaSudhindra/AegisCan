// -----------------------------------------------------------------------------
// Module: aegis_top
// Description: Top-level integration for Aegis-CPS Tier 1 Silicon Sentry.
//              Fully synthesizable for Altera Cyclone II (EP2C5T144C8N).
//              Connects BTL, De-Stuffer, ID Deserializer, BRAM Whitelist,
//              Kill Wire, Violation Capture, and SPI DMA TX.
// -----------------------------------------------------------------------------

module aegis_top (
    // =========================================================================
    // Physical Clocks & Resets
    // =========================================================================
    input  wire        clk,             // 50 MHz on-board crystal (PIN_17)
    input  wire        rst_n,           // User key pushbutton, active-low (PIN_144)

    // =========================================================================
    // CAN Physical Bus Interface (3.3V LVCMOS to Transceiver)
    // =========================================================================
    input  wire        can_rx,          // CAN RX from transceiver (PIN_40)
    output wire        can_tx,          // CAN TX / Kill Wire to transceiver (PIN_41)

    // =========================================================================
    // SPI Master Telemetry Interface (to ESP32-S3 GPIOs)
    // =========================================================================
    output wire        spi_clk,         // SPI SCK to ESP32-S3 GPIO 12 (PIN_60)
    output wire        spi_mosi,        // SPI MOSI to ESP32-S3 GPIO 13 (PIN_64)
    output wire        spi_cs_n,        // SPI CS_N to ESP32-S3 GPIO 10 (PIN_67)
    output wire        irq_out,         // Hardware IRQ to ESP32-S3 GPIO 4 (PIN_70)

    // =========================================================================
    // On-Board Diagnostic Status LEDs (Active-Low on Cyclone II Mini Board)
    // =========================================================================
    output wire        led_kill_active, // LED D1: Lights when Kill Wire fires (PIN_3)
    output wire        led_bus_idle,    // LED D2: Lights when bus is idle (PIN_7)
    output wire        led_heartbeat    // LED D3: Toggles continuously @ 1 Hz (PIN_9)
);

    // =========================================================================
    // Internal Wires
    // =========================================================================

    // BTL outputs
    wire        btl_sampled_bit;
    wire        btl_bit_valid;
    wire        btl_sof_detected;
    wire        btl_bus_idle;

    // De-stuffer outputs
    wire        ds_destuffed_bit;
    wire        ds_destuffed_valid;
    wire        ds_stuff_error;

    // ID Deserializer outputs
    wire [28:0] deser_arb_id;
    wire        deser_id_valid;
    wire        deser_is_extended;
    wire        deser_rtr_bit;

    // BRAM Whitelist outputs
    wire        wl_id_blocked;
    wire        wl_lookup_done;

    // Kill Wire outputs
    wire        kw_can_tx_override;
    wire        kw_can_tx_oe;
    wire        kw_kill_active;

    // Violation Capture outputs
    wire [63:0] vc_descriptor;
    wire        vc_descriptor_ready;

    // SPI DMA TX outputs
    wire        spi_tx_busy;
    wire        spi_tx_done;

    // =========================================================================
    // Free-Running 24-bit Timestamp Counter (20 ns resolution)
    // =========================================================================
    reg [23:0] timestamp_counter;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n)
            timestamp_counter <= 24'd0;
        else
            timestamp_counter <= timestamp_counter + 24'd1;
    end

    // =========================================================================
    // 1 Hz Diagnostic Heartbeat Generator for On-Board LED
    // 50 MHz clock -> 25,000,000 cycles per half-second toggle
    // =========================================================================
    reg [24:0] heartbeat_cnt;
    reg        heartbeat_reg;

    // Heartbeat runs unconditionally on 50 MHz clk (fail-safe: unaffected by reset state)
    always @(posedge clk) begin
        if (heartbeat_cnt >= 25'd24_999_999) begin
            heartbeat_cnt <= 25'd0;
            heartbeat_reg <= ~heartbeat_reg;
        end else begin
            heartbeat_cnt <= heartbeat_cnt + 25'd1;
        end
    end

    // =========================================================================
    // CAN TX Output Multiplexer
    // When Kill Wire is active: forcibly drive dominant '0'.
    // Otherwise: pass recessive '1' (passive inline sentry).
    // =========================================================================
    assign can_tx = kw_can_tx_oe ? kw_can_tx_override : 1'b1;

    // =========================================================================
    // Module Instantiations
    // =========================================================================

    // --- Tier 1a: Bit-Timing Logic & 50 MHz Oversampler ---
    can_btl u_btl (
        .clk           (clk),
        .rst_n         (rst_n),
        .can_rx        (can_rx),
        .sampled_bit   (btl_sampled_bit),
        .bit_valid     (btl_bit_valid),
        .sof_detected  (btl_sof_detected),
        .bus_idle      (btl_bus_idle)
    );

    // --- Tier 1b: Hardware Bit De-Stuffer ---
    can_destuffer u_destuffer (
        .clk            (clk),
        .rst_n          (rst_n),
        .sampled_bit    (btl_sampled_bit),
        .bit_valid      (btl_bit_valid),
        .sof_detected   (btl_sof_detected),
        .destuffed_bit  (ds_destuffed_bit),
        .destuffed_valid(ds_destuffed_valid),
        .stuff_error    (ds_stuff_error)
    );

    // --- Tier 1c: CAN 2.0A/B Arbitration ID Deserializer ---
    can_id_deserializer u_deserializer (
        .clk            (clk),
        .rst_n          (rst_n),
        .destuffed_bit  (ds_destuffed_bit),
        .destuffed_valid(ds_destuffed_valid),
        .sof_detected   (btl_sof_detected),
        .arb_id         (deser_arb_id),
        .id_valid       (deser_id_valid),
        .is_extended    (deser_is_extended),
        .rtr_bit        (deser_rtr_bit)
    );

    // --- Tier 1d: BRAM Whitelist Matcher (Pre-loaded with Attack Signatures) ---
    bram_whitelist u_whitelist (
        .clk          (clk),
        .rst_n        (rst_n),
        .arb_id       (deser_arb_id),
        .id_valid     (deser_id_valid),
        .is_extended  (deser_is_extended),
        // Hardware write ports tied to safe inactive defaults (no floating pins)
        .wr_en        (1'b0),
        .wr_addr      (11'd0),
        .wr_data      (1'b0),
        .wr_ext_en    (1'b0),
        .wr_ext_addr  (6'd0),
        .wr_ext_id    (29'd0),
        .wr_ext_valid (1'b0),
        .id_blocked   (wl_id_blocked),
        .lookup_done  (wl_lookup_done)
    );

    // --- Tier 1e: Active Dominant Overdrive (Kill Wire) ---
    kill_wire_ctrl u_kill_wire (
        .clk             (clk),
        .rst_n           (rst_n),
        .id_blocked      (wl_id_blocked),
        .lookup_done     (wl_lookup_done),
        .bit_valid       (btl_bit_valid),
        .can_tx_override (kw_can_tx_override),
        .can_tx_oe       (kw_can_tx_oe),
        .kill_active     (kw_kill_active)
    );

    // --- Tier 1f: 64-bit Incident Descriptor Capture ---
    violation_capture u_violation (
        .clk              (clk),
        .rst_n            (rst_n),
        .kill_active      (kw_kill_active),
        .arb_id           (deser_arb_id),
        .is_extended      (deser_is_extended),
        .timestamp        (timestamp_counter),
        .tx_busy          (spi_tx_busy),
        .descriptor       (vc_descriptor),
        .descriptor_ready (vc_descriptor_ready)
    );

    // --- Tier 1g: High-Speed SPI Master DMA Transmitter ---
    spi_dma_tx u_spi_tx (
        .clk              (clk),
        .rst_n            (rst_n),
        .descriptor       (vc_descriptor),
        .descriptor_ready (vc_descriptor_ready),
        .spi_clk          (spi_clk),
        .spi_mosi         (spi_mosi),
        .spi_cs_n         (spi_cs_n),
        .irq_out          (irq_out),
        .tx_busy          (spi_tx_busy),
        .tx_done          (spi_tx_done)
    );

    // =========================================================================
    // Status LED Outputs (Active-High for Altera DE2 Green LEDs LEDG0..LEDG2)
    // =========================================================================
    // Retriggerable 200 ms indicator at 50 MHz; CAN TX timing is unchanged.
    reg [23:0] led_stretch_cnt;
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n)
            led_stretch_cnt <= 24'd0;
        else if (kw_kill_active)
            led_stretch_cnt <= 24'd10_000_000;
        else if (led_stretch_cnt != 24'd0)
            led_stretch_cnt <= led_stretch_cnt - 24'd1;
    end

    assign led_kill_active = kw_kill_active | (led_stretch_cnt != 24'd0);
    assign led_bus_idle    = btl_bus_idle;   // LEDG1: Lights ON when bus is idle
    assign led_heartbeat   = heartbeat_reg;  // LEDG2: Blinks at 1 Hz (Heartbeat)

endmodule
