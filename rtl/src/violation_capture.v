/*
 * Module: violation_capture
 * Description: 64-bit Violation Descriptor Register.
 *              Latches violation details on kill_active.
 *              Provides descriptor to SPI DMA.
 */
module violation_capture (
    input  wire        clk,
    input  wire        rst_n,
    input  wire        kill_active,
    input  wire [28:0] arb_id,
    input  wire        is_extended,
    input  wire [23:0] timestamp,
    input  wire        tx_busy,
    output reg  [63:0] descriptor,
    output reg         descriptor_ready
);

    reg kill_active_q;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            kill_active_q <= 1'b0;
            descriptor <= 64'd0;
            descriptor_ready <= 1'b0;
        end else begin
            kill_active_q <= kill_active;
            
            // Latch on rising edge of kill_active if not busy
            if (kill_active && !kill_active_q) begin
                if (!tx_busy && !descriptor_ready) begin
                    descriptor <= {
                        timestamp,           // [63:40]
                        is_extended,         // [39]
                        arb_id,              // [38:10]
                        4'b0001,             // [9:6] Error code: BLOCKED_ID
                        6'b000000            // [5:0] Reserved
                    };
                    descriptor_ready <= 1'b1;
                end
            end else if (tx_busy) begin
                // Clear ready when SPI acknowledges busy
                descriptor_ready <= 1'b0;
            end
        end
    end

endmodule
