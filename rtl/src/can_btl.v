// -----------------------------------------------------------------------------
// Module:      can_btl
// Project:     Aegis-CPS Silicon Sentry
// Description: Bit-Timing Logic (BTL) & 50 MHz Oversampler for CAN 2.0A/B.
//
// Function:
//   - Double-flop synchronizes the raw CAN_RX pin to the 50 MHz domain.
//   - Detects SOF on the first recessive-to-dominant edge after bus idle.
//   - Runs a modulo-100 counter (100 cycles/bit at 500 kbps).
//   - Applies 3-point majority voting at 78%, 79%, 80% of the bit period
//     for robust noise immunity (covers ±20% clock edge jitter).
//   - Hard resynchronization: any recessive-to-dominant edge in the second
//     half of a bit period resets the counter, tracking the actual bus edge.
//   - Detects bus idle after 11 consecutive recessive bit periods (1100 cycles).
//
// Timing:
//   System clock:   50 MHz (Tclk = 20 ns)
//   CAN baud rate:  500 kbps (Tbit = 2000 ns = 100 clock cycles)
//   Sample point:   80% = cycle 80 after bit start
//   Majority voter: samples at cycles 78, 79, 80
//
// Target: Altera Cyclone II (EP2C5T144), 50 MHz crystal oscillator.
// -----------------------------------------------------------------------------

module can_btl (
    input  wire clk,          // 50 MHz system clock
    input  wire rst_n,        // Active-low asynchronous reset

    input  wire can_rx,       // Raw CAN RX from SN65HVD230 transceiver

    output reg  sampled_bit,  // Majority-voted bit value (valid when bit_valid = 1)
    output reg  bit_valid,    // 1-cycle pulse: sampled_bit is ready
    output reg  sof_detected, // 1-cycle pulse: Start-of-Frame detected (bus was idle)
    output reg  bus_idle      // Level: 1 = bus is currently idle (11+ recessive bits)
);

    // -------------------------------------------------------------------------
    // Parameters
    // -------------------------------------------------------------------------
    localparam BIT_CYCLES  = 7'd100; // Cycles per CAN bit at 500 kbps / 50 MHz
    localparam SAMPLE_PT1  = 7'd78;  // First  majority-vote sample point
    localparam SAMPLE_PT2  = 7'd79;  // Second majority-vote sample point
    localparam SAMPLE_PT3  = 7'd80;  // Third  majority-vote sample point (nominal)
    // Hard-resync only applies when we are past the midpoint to avoid false triggers
    localparam RESYNC_GUARD = 7'd50;

    // 11 recessive bit periods = 1100 cycles before bus is declared idle
    // Counter width: ceil(log2(1100)) = 11 bits
    localparam IDLE_THRESHOLD = 11'd1100;

    // -------------------------------------------------------------------------
    // Double-flop Synchronizer (eliminates metastability on CAN_RX)
    // -------------------------------------------------------------------------
    reg rx_q0, rx_q1;
    wire rx_sync = rx_q1;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            rx_q0 <= 1'b1; // recessive idle
            rx_q1 <= 1'b1;
        end else begin
            rx_q0 <= can_rx;
            rx_q1 <= rx_q0;
        end
    end

    // -------------------------------------------------------------------------
    // Edge Detection (recessive -> dominant = falling edge on rx_sync)
    // -------------------------------------------------------------------------
    reg rx_prev;
    wire rx_falling_edge = rx_prev & ~rx_sync; // previous=1, current=0

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) rx_prev <= 1'b1;
        else        rx_prev <= rx_sync;
    end

    // -------------------------------------------------------------------------
    // Bus Idle Counter
    // Idle is declared after 1100 consecutive recessive cycles (11 bit periods).
    // -------------------------------------------------------------------------
    reg [10:0] idle_counter;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            idle_counter <= 11'd0;
            bus_idle     <= 1'b1; // Start in idle state
        end else begin
            if (rx_sync == 1'b1) begin
                // Recessive: count toward idle
                if (idle_counter < IDLE_THRESHOLD)
                    idle_counter <= idle_counter + 11'd1;
                else
                    bus_idle <= 1'b1;
            end else begin
                // Dominant: any dominant bit immediately resets idle
                idle_counter <= 11'd0;
                bus_idle     <= 1'b0;
            end
        end
    end

    // -------------------------------------------------------------------------
    // Bit Counter FSM & Majority-Voter Sampling
    // -------------------------------------------------------------------------
    reg [6:0] bit_counter;
    reg sample1, sample2, sample3;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            bit_counter  <= 7'd0;
            sof_detected <= 1'b0;
            bit_valid    <= 1'b0;
            sampled_bit  <= 1'b1;
            sample1      <= 1'b1;
            sample2      <= 1'b1;
            sample3      <= 1'b1;
        end else begin
            // Default: clear single-cycle pulses
            bit_valid    <= 1'b0;
            sof_detected <= 1'b0;

            if (bus_idle && rx_falling_edge) begin
                // -------------------------------------------------------
                // SOF: First dominant edge after bus idle.
                // Reset counter and declare SOF.
                // -------------------------------------------------------
                bit_counter  <= 7'd0;
                sof_detected <= 1'b1;
                // bus_idle is NOT cleared here; it's cleared by the idle counter
                // logic when idle_counter resets on the next dominant cycle.

            end else if (!bus_idle) begin
                // -------------------------------------------------------
                // Hard Resynchronization (ISO 11898-1 §10.4.2):
                // Recessive-to-dominant edge in the second half of a bit
                // period resets the counter (stretches the bit time).
                // -------------------------------------------------------
                if (rx_falling_edge && (bit_counter > RESYNC_GUARD)) begin
                    bit_counter <= 7'd0;
                end else begin
                    // Normal counter advance
                    if (bit_counter == BIT_CYCLES - 7'd1)
                        bit_counter <= 7'd0;
                    else
                        bit_counter <= bit_counter + 7'd1;
                end

                // -------------------------------------------------------
                // 3-Point Majority Voter
                // Samples at 78%, 79%, 80% of the bit period.
                // Result: sampled_bit = majority of the three samples.
                // -------------------------------------------------------
                if (bit_counter == SAMPLE_PT1) sample1 <= rx_sync;
                if (bit_counter == SAMPLE_PT2) sample2 <= rx_sync;
                if (bit_counter == SAMPLE_PT3) begin
                    sample3     <= rx_sync;
                    // Majority logic: at least 2 of 3 must agree
                    sampled_bit <= (sample1 & sample2) |
                                   (sample1 & rx_sync) |
                                   (sample2 & rx_sync);
                    bit_valid   <= 1'b1;
                end
            end
        end
    end

endmodule
