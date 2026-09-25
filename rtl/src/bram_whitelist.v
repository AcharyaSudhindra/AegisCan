// -----------------------------------------------------------------------------
// Module: bram_whitelist
// Description: Dual-Port BRAM Whitelist + Extended-ID CAM for CAN Arbitration 
//              ID lookup.
// Hardware Ready: Pre-loads default blocked IDs on power-up so the FPGA protects
//                 the bus immediately without requiring external initialization.
// -----------------------------------------------------------------------------
module bram_whitelist (
    input  wire        clk,
    input  wire        rst_n,
    
    // Lookup interface (from can_id_deserializer)
    input  wire [28:0] arb_id,
    input  wire        id_valid,
    input  wire        is_extended,
    
    // Standard ID Write interface
    input  wire        wr_en,
    input  wire [10:0] wr_addr,
    input  wire        wr_data,
    
    // Extended ID Write interface
    input  wire        wr_ext_en,
    input  wire [5:0]  wr_ext_addr,
    input  wire [28:0] wr_ext_id,
    input  wire        wr_ext_valid,
    
    // Outputs
    output reg         id_blocked,
    output reg         lookup_done
);

    // Standard ID BRAM (2048 x 1-bit)
    // 0 = Allowed (Whitelisted)
    // 1 = Blocked (Blacklisted / Attack Frame)
    reg [0:0] std_bram [0:2047];

    // =========================================================================
    // Power-On Initialization (Synthesizable in Altera Cyclone II M4K RAM)
    // Pre-loads known attack IDs so system is active immediately on boot.
    // =========================================================================
    integer init_idx;
    initial begin
        // By default: all IDs whitelisted (0)
        for (init_idx = 0; init_idx < 2048; init_idx = init_idx + 1) begin
            std_bram[init_idx] = 1'b0;
        end

        // Pre-configure Demo Attack IDs to BLOCKED (1'b1):
        std_bram[11'h100] = 1'b1; // Attack Vector 1: Spoofed Brake Override
        std_bram[11'h010] = 1'b1; // Attack Vector 2: High-Priority Bus Flood ID
        std_bram[11'h001] = 1'b1; // Attack Vector 3: Critical Actuator Hijack
        std_bram[11'h1AA] = 1'b1; // Test Vector ID
    end
    
    // Write port
    always @(posedge clk) begin
        if (wr_en) begin
            std_bram[wr_addr] <= wr_data;
        end
    end
    
    // Read port (Synchronous 1-cycle lookup)
    reg std_bram_out;
    always @(posedge clk) begin
        std_bram_out <= std_bram[arb_id[10:0]];
    end

    // Extended ID CAM (64 entries)
    reg [28:0] ext_cam_id [0:63];
    reg        ext_cam_valid [0:63];
    
    integer i;
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            for (i = 0; i < 64; i = i + 1) begin
                ext_cam_valid[i] <= 1'b0;
                ext_cam_id[i]    <= 29'd0;
            end
            // Pre-load one default blocked extended ID for demo
            ext_cam_id[0]    <= 29'h0DEADBEE;
            ext_cam_valid[0] <= 1'b1;
        end else if (wr_ext_en) begin
            ext_cam_id[wr_ext_addr]    <= wr_ext_id;
            ext_cam_valid[wr_ext_addr] <= wr_ext_valid;
        end
    end

    // FSM State Machine for Whitelist Verification
    localparam STATE_IDLE     = 2'd0;
    localparam STATE_STD_WAIT = 2'd1;
    localparam STATE_EXT_SCAN = 2'd2;
    localparam STATE_DONE     = 2'd3;
    
    reg [1:0] state;
    reg [5:0] scan_count;
    
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state       <= STATE_IDLE;
            scan_count  <= 6'd0;
            id_blocked  <= 1'b0;
            lookup_done <= 1'b0;
        end else begin
            case (state)
                STATE_IDLE: begin
                    lookup_done <= 1'b0;
                    id_blocked  <= 1'b0;
                    scan_count  <= 6'd0;
                    if (id_valid) begin
                        if (is_extended) begin
                            state <= STATE_EXT_SCAN;
                        end else begin
                            state <= STATE_STD_WAIT;
                        end
                    end
                end
                
                // Standard 11-bit ID: BRAM read takes exactly 1 clock cycle
                STATE_STD_WAIT: begin
                    id_blocked  <= std_bram_out;
                    lookup_done <= 1'b1;
                    state       <= STATE_DONE;
                end
                
                // Extended 29-bit ID: Sequential CAM scan
                STATE_EXT_SCAN: begin
                    if (ext_cam_valid[scan_count] && (ext_cam_id[scan_count] == arb_id)) begin
                        id_blocked  <= 1'b1;
                        lookup_done <= 1'b1;
                        state       <= STATE_DONE;
                    end else if (scan_count == 6'd63) begin
                        id_blocked  <= 1'b0;
                        lookup_done <= 1'b1;
                        state       <= STATE_DONE;
                    end else begin
                        scan_count <= scan_count + 6'd1;
                    end
                end
                
                STATE_DONE: begin
                    if (!id_valid) begin
                        state       <= STATE_IDLE;
                        lookup_done <= 1'b0;
                    end
                end
                
                default: state <= STATE_IDLE;
            endcase
        end
    end

endmodule
