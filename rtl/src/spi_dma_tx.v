/*
 * Module: spi_dma_tx
 * Description: SPI Master Transmitter.
 *              Sends 64-bit violation descriptor as two 32-bit words via SPI Mode 0.
 *              Generates IRQ for 4 SPI clock cycles to wake ESP32-S3 before transmission.
 */
module spi_dma_tx (
    input  wire        clk,
    input  wire        rst_n,
    input  wire [63:0] descriptor,
    input  wire        descriptor_ready,
    output wire        spi_clk,
    output wire        spi_mosi,
    output reg         spi_cs_n,
    output reg         irq_out,
    output reg         tx_busy,
    output reg         tx_done
);

    localparam IDLE = 2'd0;
    localparam WAKE = 2'd1;
    localparam TX   = 2'd2;
    localparam DONE = 2'd3;

    reg [1:0] state;
    reg [1:0] clk_div;
    reg [6:0] bit_cnt;
    reg [2:0] wake_cnt;
    reg [63:0] shift_reg;

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state <= IDLE;
            clk_div <= 2'd0;
            bit_cnt <= 7'd0;
            wake_cnt <= 3'd0;
            shift_reg <= 64'd0;
            spi_cs_n <= 1'b1;
            irq_out <= 1'b0;
            tx_busy <= 1'b0;
            tx_done <= 1'b0;
        end else begin
            tx_done <= 1'b0; // Default: no pulse
            
            case (state)
                IDLE: begin
                    clk_div <= 2'd0;
                    bit_cnt <= 7'd0;
                    wake_cnt <= 3'd0;
                    spi_cs_n <= 1'b1;
                    irq_out <= 1'b0;
                    if (descriptor_ready) begin
                        shift_reg <= descriptor;
                        tx_busy <= 1'b1;
                        irq_out <= 1'b1;
                        state <= WAKE;
                    end
                end
                
                WAKE: begin
                    clk_div <= clk_div + 1'b1;
                    // SPI clock transitions happen every 4 system clocks
                    if (clk_div == 2'b11) begin
                        wake_cnt <= wake_cnt + 1'b1;
                        if (wake_cnt == 3'd3) begin
                            state <= TX;
                            spi_cs_n <= 1'b0; // Assert CS
                        end
                    end
                end
                
                TX: begin
                    clk_div <= clk_div + 1'b1;
                    // Shift on the falling edge of spi_clk (when clk_div wraps)
                    if (clk_div == 2'b11) begin
                        bit_cnt <= bit_cnt + 1'b1;
                        shift_reg <= {shift_reg[62:0], 1'b0};
                        if (bit_cnt == 7'd63) begin
                            state <= DONE;
                            spi_cs_n <= 1'b1; // De-assert CS
                            irq_out <= 1'b0;  // De-assert IRQ
                        end
                    end
                end
                
                DONE: begin
                    tx_busy <= 1'b0;
                    tx_done <= 1'b1; // Pulse tx_done
                    state <= IDLE;
                end
                
                default: state <= IDLE;
            endcase
        end
    end

    // SPI Mode 0: Data sampled on rising edge, shifted on falling edge.
    // spi_clk is 0 during clk_div = 00, 01 and 1 during clk_div = 10, 11
    assign spi_clk = (state == TX) ? clk_div[1] : 1'b0;
    
    // MSB first output
    assign spi_mosi = shift_reg[63];

endmodule
