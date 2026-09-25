// -----------------------------------------------------------------------------
// Module: can_id_deserializer
// Description: CAN 2.0A/B Arbitration ID Extractor FSM for Aegis-CPS.
// Design Notes: Extracts 11-bit or 29-bit Arbitration IDs from destuffed bits.
// -----------------------------------------------------------------------------

module can_id_deserializer (
    input  wire        clk,
    input  wire        rst_n,
    input  wire        destuffed_bit,
    input  wire        destuffed_valid,
    input  wire        sof_detected,
    output reg  [28:0] arb_id,
    output reg         id_valid,
    output reg         is_extended,
    output reg         rtr_bit
);

    // FSM States
    localparam STATE_IDLE          = 3'd0;
    localparam STATE_SHIFT_BASE_ID = 3'd1;
    localparam STATE_READ_SRR_RTR  = 3'd2;
    localparam STATE_READ_IDE      = 3'd3;
    localparam STATE_SHIFT_EXT_ID  = 3'd4;
    localparam STATE_READ_EXT_RTR  = 3'd5;
    localparam STATE_PASS_THROUGH  = 3'd6;

    reg [2:0]  state_q;
    reg [2:0]  state_d;
    
    reg [10:0] base_id_q;
    reg [10:0] base_id_d;
    
    reg [17:0] ext_id_q;
    reg [17:0] ext_id_d;
    
    reg [4:0]  bit_count_q;
    reg [4:0]  bit_count_d;
    
    reg        rtr_capture_q;
    reg        rtr_capture_d;

    reg [28:0] arb_id_d;
    reg        id_valid_d;
    reg        is_extended_d;
    reg        rtr_bit_d;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state_q       <= STATE_IDLE;
            base_id_q     <= 11'd0;
            ext_id_q      <= 18'd0;
            bit_count_q   <= 5'd0;
            rtr_capture_q <= 1'b0;
            
            arb_id        <= 29'd0;
            id_valid      <= 1'b0;
            is_extended   <= 1'b0;
            rtr_bit       <= 1'b0;
        end else begin
            state_q       <= state_d;
            base_id_q     <= base_id_d;
            ext_id_q      <= ext_id_d;
            bit_count_q   <= bit_count_d;
            rtr_capture_q <= rtr_capture_d;
            
            arb_id        <= arb_id_d;
            id_valid      <= id_valid_d;
            is_extended   <= is_extended_d;
            rtr_bit       <= rtr_bit_d;
        end
    end

    always @(*) begin
        // Default assignments
        state_d       = state_q;
        base_id_d     = base_id_q;
        ext_id_d      = ext_id_q;
        bit_count_d   = bit_count_q;
        rtr_capture_d = rtr_capture_q;
        
        arb_id_d      = arb_id;
        id_valid_d    = 1'b0; // Single pulse
        is_extended_d = is_extended;
        rtr_bit_d     = rtr_bit;

        if (sof_detected) begin
            state_d     = STATE_SHIFT_BASE_ID;
            base_id_d   = 11'd0;
            bit_count_d = 5'd0;
        end else if (destuffed_valid) begin
            case (state_q)
                STATE_IDLE: begin
                    // Wait for SOF
                end
                
                STATE_SHIFT_BASE_ID: begin
                    base_id_d = {base_id_q[9:0], destuffed_bit};
                    if (bit_count_q == 5'd10) begin
                        state_d = STATE_READ_SRR_RTR;
                    end else begin
                        bit_count_d = bit_count_q + 5'd1;
                    end
                end
                
                STATE_READ_SRR_RTR: begin
                    rtr_capture_d = destuffed_bit;
                    state_d = STATE_READ_IDE;
                end
                
                STATE_READ_IDE: begin
                    if (destuffed_bit == 1'b0) begin
                        // Standard Frame (IDE = 0)
                        arb_id_d      = {18'b0, base_id_q};
                        is_extended_d = 1'b0;
                        rtr_bit_d     = rtr_capture_q;
                        id_valid_d    = 1'b1;
                        state_d       = STATE_PASS_THROUGH;
                    end else begin
                        // Extended Frame (IDE = 1)
                        state_d       = STATE_SHIFT_EXT_ID;
                        bit_count_d   = 5'd0;
                    end
                end
                
                STATE_SHIFT_EXT_ID: begin
                    ext_id_d = {ext_id_q[16:0], destuffed_bit};
                    if (bit_count_q == 5'd17) begin
                        state_d = STATE_READ_EXT_RTR;
                    end else begin
                        bit_count_d = bit_count_q + 5'd1;
                    end
                end
                
                STATE_READ_EXT_RTR: begin
                    arb_id_d      = {base_id_q, ext_id_q};
                    is_extended_d = 1'b1;
                    rtr_bit_d     = destuffed_bit;
                    id_valid_d    = 1'b1;
                    state_d       = STATE_PASS_THROUGH;
                end
                
                STATE_PASS_THROUGH: begin
                    // Remain here until next SOF
                end
                
                default: begin
                    state_d = STATE_IDLE;
                end
            endcase
        end
    end

endmodule
