// -----------------------------------------------------------------------------
// Module:      can_destuffer
// Project:     Aegis-CPS Silicon Sentry
// Description: Hardware Bit De-Stuffer for CAN 2.0A/B.
//              Per ISO 11898-1 Section 10.5:
//              After 5 consecutive bits of identical polarity in SOF, Arbitration,
//              Control, Data, and CRC fields, a stuff bit of opposite polarity
//              is inserted by the transmitter and must be removed by receivers.
//              If 6 consecutive identical bits are detected (no inversion),
//              a Bit Stuffing Error is signalled.
//
// Timing:      Combinational output on same clock cycle as bit_valid pulse.
// Target:      Altera Cyclone II, 50 MHz system clock, 500 kbps CAN bus.
// -----------------------------------------------------------------------------

module can_destuffer (
    input  wire clk,
    input  wire rst_n,

    // From can_btl
    input  wire sampled_bit,      // Majority-voted sampled bit value
    input  wire bit_valid,        // 1-cycle pulse: new bit is ready
    input  wire sof_detected,     // 1-cycle pulse: SOF dominant edge seen

    // To can_id_deserializer
    output reg  destuffed_bit,    // De-stuffed bit value (valid only when destuffed_valid = 1)
    output reg  destuffed_valid,  // 1-cycle pulse: destuffed_bit is a real data bit

    // Error signal
    output reg  stuff_error       // 1-cycle pulse: illegal 6-consecutive-bit sequence detected
);

    reg [2:0] consec_count; // Count of consecutive identical bits (1..5)
    reg       last_bit;     // Polarity of the current run
    reg       in_sof_bit;   // Ignores SOF bit sample point

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            consec_count    <= 3'd0;
            last_bit        <= 1'b1; // Default: recessive (bus idle)
            destuffed_bit   <= 1'b1;
            destuffed_valid <= 1'b0;
            stuff_error     <= 1'b0;
            in_sof_bit      <= 1'b0;
        end else begin
            // Outputs default to deasserted every cycle
            destuffed_valid <= 1'b0;
            stuff_error     <= 1'b0;

            // Priority 1: SOF resets the engine
            if (sof_detected) begin
                // SOF is dominant (0). ISO 11898: stuffing applies from SOF through CRC.
                consec_count    <= 3'd1;
                last_bit        <= 1'b0; // dominant
                in_sof_bit      <= 1'b1; // Skip passing the SOF bit to deserializer

            end else if (bit_valid) begin
                if (in_sof_bit) begin
                    // Consume the SOF sample point; real ID bits begin on next bit
                    in_sof_bit <= 1'b0;
                end else if (consec_count == 3'd5) begin
                    // We have seen 5 identical bits. The current bit must be the
                    // inverted stuff bit. Evaluate it:
                    if (sampled_bit == last_bit) begin
                        // 6 consecutive identical bits: Bit Stuffing Error (ISO 11898-1 §10.5)
                        // Signal error. Do NOT update state — upstream kill_wire_ctrl handles recovery.
                        stuff_error <= 1'b1;
                    end else begin
                        // Valid stuff bit: discard it (do NOT assert destuffed_valid).
                        // Per ISO 11898: the stuff bit itself starts a new run of 1.
                        consec_count <= 3'd1;
                        last_bit     <= sampled_bit; // new polarity = stuff bit's polarity
                    end

                end else begin
                    // Normal data bit: pass to deserializer.
                    destuffed_bit   <= sampled_bit;
                    destuffed_valid <= 1'b1;

                    if (sampled_bit == last_bit) begin
                        // Same polarity: extend the run
                        consec_count <= consec_count + 3'd1;
                    end else begin
                        // Polarity changed: start a fresh run
                        consec_count <= 3'd1;
                        last_bit     <= sampled_bit;
                    end
                end
            end
        end
    end

endmodule
