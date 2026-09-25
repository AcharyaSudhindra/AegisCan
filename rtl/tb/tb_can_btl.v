`timescale 1ns/1ps

module tb_can_btl;
    reg clk;
    reg rst_n;
    reg can_rx;
    wire sampled_bit;
    wire bit_valid;
    wire sof_detected;
    wire bus_idle;
    
    can_btl uut (
        .clk(clk),
        .rst_n(rst_n),
        .can_rx(can_rx),
        .sampled_bit(sampled_bit),
        .bit_valid(bit_valid),
        .sof_detected(sof_detected),
        .bus_idle(bus_idle)
    );
    
    // 50 MHz Clock -> 20ns period (10ns high, 10ns low)
    initial clk = 0;
    always #10 clk = ~clk;
    
    // CAN bit task, 500 kbps -> 2000 ns per bit
    task send_bit;
        input b;
        begin
            can_rx = b;
            #2000;
        end
    endtask
    
    initial begin
        $dumpfile("can_btl.vcd");
        $dumpvars(0, tb_can_btl);
        
        rst_n = 0;
        can_rx = 1;
        #100;
        rst_n = 1;
        
        // Wait for bus idle (11 bits = 22000 ns)
        #22000;
        
        // Send SOF (0)
        send_bit(0);
        
        // ID: 0x7FF (11 bits of 1s)
        send_bit(1); send_bit(1); send_bit(1); send_bit(1); send_bit(1);
        // Stuffed bit (0)
        send_bit(0);
        send_bit(1); send_bit(1); send_bit(1); send_bit(1); send_bit(1);
        // Stuffed bit (0)
        send_bit(0);
        send_bit(1); // 11th bit
        
        // RTR (0)
        send_bit(0);
        
        // IDE (0)
        send_bit(0);
        
        // r0 (0)
        send_bit(0);
        
        // DLC: 0x0
        send_bit(0); send_bit(0); send_bit(0); send_bit(0);
        
        // Finish
        #5000;
        $display("can_btl test finished.");
        $finish;
    end
endmodule
