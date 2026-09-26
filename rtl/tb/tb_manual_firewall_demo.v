`timescale 1ns/1ps
module tb_manual_firewall_demo;
    reg clk = 0;
    always #10 clk = ~clk;
    reg [1:0] key = 2'b11;
    reg [10:0] sw = 0;
    wire [17:0] ledr;
    wire [8:0] ledg;
    wire [6:0] hex0, hex1, hex2;
    manual_firewall_demo #(.DEBOUNCE_CYCLES(2)) dut(
        .CLOCK_50(clk), .KEY(key), .SW(sw), .LEDR(ledr), .LEDG(ledg),
        .HEX0(hex0), .HEX1(hex1), .HEX2(hex2));
    task press_check;
        input [10:0] id;
        input expect_block;
        begin
            sw = id;
            @(negedge clk); key[0] = 0;
            repeat (12) @(posedge clk);
            if (ledr[10:0] !== id || ledg[1] !== expect_block ||
                ledg[0] !== ~expect_block || ledg[2] !== expect_block || ledg[3] !== 1)
                $fatal(1, "ID %h gave incorrect manual-demo result", id);
            @(negedge clk); key[0] = 1;
            repeat (4) @(posedge clk);
        end
    endtask
    initial begin
        key[1] = 0; repeat (3) @(posedge clk); key[1] = 1;
        repeat (4) @(posedge clk);
        // Short bounce must not cause a check.
        sw = 11'h100; @(negedge clk); key[0] = 0; @(negedge clk); key[0] = 1;
        repeat (5) @(posedge clk);
        if (ledg[3] !== 0) $fatal(1, "Key bounce triggered a check");
        press_check(11'h050, 0);
        press_check(11'h100, 1);
        press_check(11'h001, 1);
        press_check(11'h010, 1);
        press_check(11'h1AA, 1);
        press_check(11'h555, 0);
        $display("PASS: manual firewall demo checks the shared blocklist");
        $finish;
    end
endmodule
