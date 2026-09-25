// -----------------------------------------------------------------------------
// Module:      kill_wire_ctrl
// Project:     Aegis-CPS Silicon Sentry
// Description: Active Dominant Overdrive Controller (Kill Wire).
//
// Function:
//   When the BRAM whitelist reports a blacklisted Arbitration ID (id_blocked=1,
//   lookup_done=1), this module drives the CAN transceiver TXD pin LOW (dominant)
//   for 6 consecutive bit periods, creating a deliberate Bit Stuffing Error.
//
//   Per ISO 11898-1 §10.4.4: An Active Error Flag consists of 6 consecutive
//   dominant bits. All nodes on the bus detect this as a frame error and discard
//   the in-progress frame. The malicious payload is NEVER delivered.
//
//   After asserting the error flag, the module waits for one full bit period
//   (Error Delimiter / bus recovery) before returning to IDLE.
//
// Timing (500 kbps CAN, 50 MHz clock, 100 cycles/bit):
//   Kill duration:    6 bit periods = 600 clock cycles = 12 µs
//   Recovery period:  8 bit periods = 800 clock cycles = 16 µs (ISO error delimiter)
//
// Output:
//   can_tx_override = 1'b0 during KILLING (drive dominant on bus)
//   can_tx_oe       = 1'b1 during KILLING (output enable active)
//   kill_active     = 1'b1 during KILLING (status for LED and violation_capture)
//
// Target: Altera Cyclone II, 50 MHz clock.
// -----------------------------------------------------------------------------

module kill_wire_ctrl (
    input  wire clk,          // 50 MHz system clock
    input  wire rst_n,        // Active-low asynchronous reset

    // From bram_whitelist
    input  wire id_blocked,   // 1 = looked-up ID is on the blacklist
    input  wire lookup_done,  // 1-cycle pulse: lookup result is valid

    // From can_btl (unused directly but available for timing alignment)
    input  wire bit_valid,    // 1-cycle pulse: a new bit period has elapsed

    // To aegis_top TX mux
    output reg  can_tx_override, // Dominant (0) when killing
    output reg  can_tx_oe,       // Output enable: 1 = override is active
    output reg  kill_active      // Status: 1 = Kill Wire is currently asserting
);

    // -------------------------------------------------------------------------
    // Parameters
    // -------------------------------------------------------------------------
    // Active Error Flag: 6 dominant bit periods (ISO 11898-1 §10.4.4)
    localparam KILL_BITS     = 6;
    // Error Delimiter: 8 recessive bit periods (ISO 11898-1 §10.4.4)
    localparam RECOVERY_BITS = 8;

    // At 50 MHz / 500 kbps: 100 cycles per bit period
    localparam CYCLES_PER_BIT  = 10'd100;
    localparam KILL_CYCLES     = 10'd600; // 6 * 100
    localparam RECOVERY_CYCLES = 10'd800; // 8 * 100

    // -------------------------------------------------------------------------
    // FSM State Encoding
    // -------------------------------------------------------------------------
    localparam STATE_IDLE     = 2'd0; // Monitoring BRAM output
    localparam STATE_KILLING  = 2'd1; // Asserting 6-bit dominant error flag
    localparam STATE_RECOVERY = 2'd2; // Bus recovery: wait 8 recessive bit periods

    reg [1:0] state;
    reg [9:0] counter; // Maximum value needed: 800

    // -------------------------------------------------------------------------
    // FSM
    // -------------------------------------------------------------------------
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state           <= STATE_IDLE;
            counter         <= 10'd0;
            can_tx_override <= 1'b1; // Default: recessive (bus passive)
            can_tx_oe       <= 1'b0; // Output disabled (FPGA not driving TX)
            kill_active     <= 1'b0;
        end else begin
            case (state)

                // -----------------------------------------------------------
                // IDLE: Monitor whitelist result.
                // Transition to KILLING as soon as a blocked ID is confirmed.
                // -----------------------------------------------------------
                STATE_IDLE: begin
                    can_tx_override <= 1'b1; // Recessive
                    can_tx_oe       <= 1'b0; // Not driving
                    kill_active     <= 1'b0;
                    counter         <= 10'd0;

                    if (lookup_done && id_blocked) begin
                        // Begin Active Error Flag immediately
                        state           <= STATE_KILLING;
                        can_tx_override <= 1'b0; // DOMINANT
                        can_tx_oe       <= 1'b1; // Drive the TX pin
                        kill_active     <= 1'b1;
                    end
                end

                // -----------------------------------------------------------
                // KILLING: Drive CAN_TX dominant for 6 full bit periods.
                // This creates an Active Error Flag that all ECUs detect.
                // -----------------------------------------------------------
                STATE_KILLING: begin
                    if (counter == KILL_CYCLES - 10'd1) begin
                        // Release dominant after exactly 6 bit periods
                        state           <= STATE_RECOVERY;
                        can_tx_override <= 1'b1; // Release to recessive
                        can_tx_oe       <= 1'b0;
                        kill_active     <= 1'b0;
                        counter         <= 10'd0;
                    end else begin
                        counter <= counter + 10'd1;
                    end
                end

                // -----------------------------------------------------------
                // RECOVERY: Wait for Error Delimiter (8 recessive bit periods)
                // before accepting the next lookup trigger.
                // This prevents immediate re-trigger on the same frame remnants.
                // -----------------------------------------------------------
                STATE_RECOVERY: begin
                    if (counter == RECOVERY_CYCLES - 10'd1) begin
                        state   <= STATE_IDLE;
                        counter <= 10'd0;
                    end else begin
                        counter <= counter + 10'd1;
                    end
                end

                default: state <= STATE_IDLE;

            endcase
        end
    end

endmodule
