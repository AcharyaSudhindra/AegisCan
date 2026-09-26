`timescale 1ns/1ps

module tb_can_id_deserializer;

    reg clk;
    reg rst_n;
    reg destuffed_bit;
    reg destuffed_valid;
    reg sof_detected;

    wire [28:0] arb_id;
    wire        id_valid;
    wire        is_extended;
    wire        rtr_bit;

    can_id_deserializer uut (
        .clk(clk),
        .rst_n(rst_n),
        .destuffed_bit(destuffed_bit),
        .destuffed_valid(destuffed_valid),
        .sof_detected(sof_detected),
        .arb_id(arb_id),
        .id_valid(id_valid),
        .is_extended(is_extended),
        .rtr_bit(rtr_bit)
    );

    // Clock generation (50 MHz)
    initial begin
        clk = 0;
        forever #10 clk = ~clk;
    end

    // Task to simulate a destuffed bit coming in
    task send_bit;
        input bit_val;
        begin
            #1980; // Wait ~100 clock cycles total for 500kbps bit period
            destuffed_bit = bit_val;
            destuffed_valid = 1;
            #20;
            destuffed_valid = 0;
        end
    endtask

    integer i;
    integer valid_count = 0;
    always @(posedge clk) if (rst_n && id_valid) valid_count = valid_count + 1;

    initial begin
        $dumpfile("can_id_deserializer.vcd");
        $dumpvars(0, tb_can_id_deserializer);

        // Initialize
        rst_n = 0;
        destuffed_bit = 0;
        destuffed_valid = 0;
        sof_detected = 0;
        #100;
        rst_n = 1;
        #100;

        // Test Case 1: Standard CAN frame with 11-bit ID = 0x123
        $display("Starting Test Case 1: Standard Frame (ID=0x123)");
        sof_detected = 1;
        #20;
        sof_detected = 0;
        
        // Base ID: 0x123 = 11'b001_0010_0011
        send_bit(0); send_bit(0); send_bit(1);
        send_bit(0); send_bit(0); send_bit(1); send_bit(0);
        send_bit(0); send_bit(0); send_bit(1); send_bit(1);

        // RTR=0
        send_bit(0);
        // IDE=0
        send_bit(0);

        #100;
        if (valid_count == 1 && arb_id == 29'h00000123 && !is_extended) begin
            $display("PASS: Test Case 1");
        end else begin
            $fatal(1, "Test Case 1: ID=%h, extended=%b, pulses=%0d", arb_id, is_extended, valid_count);
        end

        #2000;

        // Test Case 2: Extended CAN frame with 29-bit ID = 0x1ABCDEF0
        $display("Starting Test Case 2: Extended Frame (ID=0x1ABCDEF0)");
        sof_detected = 1;
        #20;
        sof_detected = 0;

        // Base ID = 29-bit ID[28:18] = 0x1ABCDEF0 >> 18 = 0x6AF = 11'b110_1010_1111
        send_bit(1); send_bit(1); send_bit(0);
        send_bit(1); send_bit(0); send_bit(1); send_bit(0);
        send_bit(1); send_bit(1); send_bit(1); send_bit(1);

        // SRR=1
        send_bit(1);
        // IDE=1
        send_bit(1);

        // Ext ID = 29-bit ID[17:0] = 0x0DEF0 = 18'b00_1101_1110_1111_0000
        send_bit(0); send_bit(0);
        send_bit(1); send_bit(1); send_bit(0); send_bit(1);
        send_bit(1); send_bit(1); send_bit(1); send_bit(0);
        send_bit(1); send_bit(1); send_bit(1); send_bit(1);
        send_bit(0); send_bit(0); send_bit(0); send_bit(0);

        // RTR=0
        send_bit(0);

        #100;
        if (valid_count == 2 && arb_id == 29'h1ABCDEF0 && is_extended) begin
            $display("PASS: Test Case 2");
        end else begin
            $fatal(1, "Test Case 2: ID=%h, extended=%b, pulses=%0d", arb_id, is_extended, valid_count);
        end

        #2000;
        $display("All tests completed.");
        $finish;
    end

endmodule
