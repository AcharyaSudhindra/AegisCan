// -----------------------------------------------------------------------------
// Manual firewall demonstration for the Terasic DE2.
//
// This module deliberately does not sample or transmit CAN.  It demonstrates
// the exact standard-ID blacklist stored in bram_whitelist by checking a
// switch-selected 11-bit value after KEY[0] is pressed.
// -----------------------------------------------------------------------------
module manual_firewall_demo #(
    // 20 ms at the DE2's 50 MHz oscillator.  Override in simulation only.
    parameter integer DEBOUNCE_CYCLES = 1_000_000
) (
    input  wire        CLOCK_50,
    input  wire [1:0]  KEY,       // Active-low: KEY[0] check, KEY[1] reset
    input  wire [10:0] SW,
    output reg  [17:0] LEDR,
    output reg  [8:0]  LEDG,
    output wire [6:0]  HEX0,
    output wire [6:0]  HEX1,
    output wire [6:0]  HEX2
);
    localparam ST_IDLE     = 3'd0;
    localparam ST_DEBOUNCE = 3'd1;
    localparam ST_ISSUE    = 3'd2;
    localparam ST_WAIT     = 3'd3;
    localparam ST_HOLD     = 3'd4;

    reg [2:0] state;
    reg [19:0] debounce_count;
    reg key0_meta, key0_sync, key0_prev;
    reg [10:0] captured_id;
    reg id_valid;
    reg accepted, blocked, checked;
    wire key0_falling = key0_prev & ~key0_sync;

    wire wl_blocked, wl_done;
    bram_whitelist u_policy (
        .clk(CLOCK_50), .rst_n(KEY[1]),
        .arb_id({18'd0, captured_id}), .id_valid(id_valid), .is_extended(1'b0),
        .wr_en(1'b0), .wr_addr(11'd0), .wr_data(1'b0),
        .wr_ext_en(1'b0), .wr_ext_addr(6'd0), .wr_ext_id(29'd0), .wr_ext_valid(1'b0),
        .id_blocked(wl_blocked), .lookup_done(wl_done)
    );

    always @(posedge CLOCK_50 or negedge KEY[1]) begin
        if (!KEY[1]) begin
            key0_meta <= 1'b1;
            key0_sync <= 1'b1;
            key0_prev <= 1'b1;
        end else begin
            key0_meta <= KEY[0];
            key0_sync <= key0_meta;
            key0_prev <= key0_sync;
        end
    end

    always @(posedge CLOCK_50 or negedge KEY[1]) begin
        if (!KEY[1]) begin
            state          <= ST_IDLE;
            debounce_count <= 20'd0;
            captured_id    <= 11'd0;
            id_valid       <= 1'b0;
            accepted       <= 1'b0;
            blocked        <= 1'b0;
            checked        <= 1'b0;
        end else begin
            id_valid <= 1'b0;
            case (state)
                ST_IDLE: begin
                    if (key0_falling) begin
                        debounce_count <= 20'd0;
                        state <= ST_DEBOUNCE;
                    end
                end
                ST_DEBOUNCE: begin
                    // A release during the debounce interval rejects a bounce.
                    if (key0_sync) begin
                        state <= ST_IDLE;
                    end else if (debounce_count == DEBOUNCE_CYCLES - 1) begin
                        captured_id <= SW;
                        accepted <= 1'b0;
                        blocked <= 1'b0;
                        checked <= 1'b0;
                        state <= ST_ISSUE;
                    end else begin
                        debounce_count <= debounce_count + 20'd1;
                    end
                end
                ST_ISSUE: begin
                    id_valid <= 1'b1;
                    state <= ST_WAIT;
                end
                ST_WAIT: begin
                    if (wl_done) begin
                        blocked <= wl_blocked;
                        accepted <= ~wl_blocked;
                        checked <= 1'b1;
                        state <= ST_HOLD;
                    end
                end
                ST_HOLD: begin
                    // One result per press. Release KEY[0] before the next ID.
                    if (key0_sync) state <= ST_IDLE;
                end
                default: state <= ST_IDLE;
            endcase
        end
    end

    always @(*) begin
        LEDR = 18'd0;
        LEDR[10:0] = captured_id; // Latched, checked CAN ID in binary.
        LEDR[17] = (state == ST_DEBOUNCE) || (state == ST_ISSUE) || (state == ST_WAIT);
        LEDG = 9'd0;
        LEDG[0] = accepted; // ACCEPT
        LEDG[1] = blocked;  // BLOCK
        LEDG[2] = blocked;  // ALERT
        LEDG[3] = checked;  // CHECK COMPLETE
    end

    // DE2 seven-segment displays are active-low. HEX2 shows ID[10:8].
    function [6:0] hex;
        input [3:0] value;
        begin
            case (value)
                4'h0: hex=7'b1000000; 4'h1: hex=7'b1111001;
                4'h2: hex=7'b0100100; 4'h3: hex=7'b0110000;
                4'h4: hex=7'b0011001; 4'h5: hex=7'b0010010;
                4'h6: hex=7'b0000010; 4'h7: hex=7'b1111000;
                4'h8: hex=7'b0000000; 4'h9: hex=7'b0010000;
                4'ha: hex=7'b0001000; 4'hb: hex=7'b0000011;
                4'hc: hex=7'b1000110; 4'hd: hex=7'b0100001;
                4'he: hex=7'b0000110; default: hex=7'b0001110;
            endcase
        end
    endfunction
    assign HEX0 = hex(captured_id[3:0]);
    assign HEX1 = hex(captured_id[7:4]);
    assign HEX2 = hex({1'b0, captured_id[10:8]});
endmodule
