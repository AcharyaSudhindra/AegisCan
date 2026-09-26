`timescale 1ns/1ps

module tb_can_destuffer;
    reg clk;
    reg rst_n;
    reg sampled_bit;
    reg bit_valid;
    reg sof_detected;
    wire destuffed_bit;
    wire destuffed_valid;
    wire stuff_error;
    
    can_destuffer uut (
        .clk(clk),
        .rst_n(rst_n),
        .sampled_bit(sampled_bit),
        .bit_valid(bit_valid),
        .sof_detected(sof_detected),
        .destuffed_bit(destuffed_bit),
        .destuffed_valid(destuffed_valid),
        .stuff_error(stuff_error)
    );
    
    initial clk = 0;
    always #10 clk = ~clk;
    
    task feed_bit;
        input b;
        begin
            sampled_bit = b;
            bit_valid = 1;
            #20; // 1 clock cycle
            bit_valid = 0;
            #80;
        end
    endtask
    
    initial begin
        $dumpfile("can_destuffer.vcd");
        $dumpvars(0, tb_can_destuffer);
        
        rst_n = 0;
        sampled_bit = 1;
        bit_valid = 0;
        sof_detected = 0;
        #100;
        rst_n = 1;
        #100;
        
        // SOF
        sof_detected = 1;
        #20;
        sof_detected = 0;
        #80;
        
        // can_btl emits the SOF sample after its edge notification.
        feed_bit(0);

        // Send 5 ones
        feed_bit(1); feed_bit(1); feed_bit(1); feed_bit(1); feed_bit(1);
        
        // Send valid stuffed bit (0)
        feed_bit(0);
        
        // Send 6 ones to trigger stuff_error
        feed_bit(1); feed_bit(1); feed_bit(1); feed_bit(1); feed_bit(1); feed_bit(1);
        
        #200;
        $display("can_destuffer test finished.");
        $finish;
    end
endmodule
