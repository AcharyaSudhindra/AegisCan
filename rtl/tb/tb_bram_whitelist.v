`timescale 1ns/1ps

module tb_bram_whitelist;

    reg clk;
    reg rst_n;
    
    reg [28:0] arb_id;
    reg        id_valid;
    reg        is_extended;
    
    reg        wr_en;
    reg [10:0] wr_addr;
    reg        wr_data;
    
    reg        wr_ext_en;
    reg [5:0]  wr_ext_addr;
    reg [28:0] wr_ext_id;
    reg        wr_ext_valid;
    
    wire       id_blocked;
    wire       lookup_done;

    bram_whitelist uut (
        .clk(clk),
        .rst_n(rst_n),
        .arb_id(arb_id),
        .id_valid(id_valid),
        .is_extended(is_extended),
        .wr_en(wr_en),
        .wr_addr(wr_addr),
        .wr_data(wr_data),
        .wr_ext_en(wr_ext_en),
        .wr_ext_addr(wr_ext_addr),
        .wr_ext_id(wr_ext_id),
        .wr_ext_valid(wr_ext_valid),
        .id_blocked(id_blocked),
        .lookup_done(lookup_done)
    );

    always #10 clk = ~clk; // 50MHz clock

    initial begin
        $dumpfile("bram_whitelist.vcd");
        $dumpvars(0, tb_bram_whitelist);
        
        clk = 0;
        rst_n = 0;
        arb_id = 0;
        id_valid = 0;
        is_extended = 0;
        wr_en = 0;
        wr_addr = 0;
        wr_data = 0;
        wr_ext_en = 0;
        wr_ext_addr = 0;
        wr_ext_id = 0;
        wr_ext_valid = 0;
        
        #100 rst_n = 1;
        
        // Write standard IDs
        @(posedge clk); wr_en = 1; wr_addr = 11'h100; wr_data = 1; // Blocked
        @(posedge clk); wr_en = 1; wr_addr = 11'h200; wr_data = 1; // Blocked
        @(posedge clk); wr_en = 1; wr_addr = 11'h3FF; wr_data = 1; // Blocked
        @(posedge clk); wr_en = 0;
        
        // Write extended IDs
        @(posedge clk); wr_ext_en = 1; wr_ext_addr = 6'd0; wr_ext_id = 29'h1ABCDEF0; wr_ext_valid = 1;
        @(posedge clk); wr_ext_en = 1; wr_ext_addr = 6'd1; wr_ext_id = 29'h0DEADBEE; wr_ext_valid = 1;
        @(posedge clk); wr_ext_en = 1; wr_ext_addr = 6'd2; wr_ext_id = 29'h1FFFFFFF; wr_ext_valid = 1;
        @(posedge clk); wr_ext_en = 0;
        
        #50;
        // Test 1: Blocked Standard ID (0x100)
        @(posedge clk); id_valid = 1; is_extended = 0; arb_id = 29'h100;
        wait(lookup_done);
        @(posedge clk);
        if (id_blocked) $display("PASS: Standard blocked ID (0x100) detected.");
        else $display("FAIL: Standard blocked ID (0x100) not detected.");
        id_valid = 0;
        wait(!lookup_done);
        
        #50;
        // Test 2: Allowed Standard ID (0x101)
        @(posedge clk); id_valid = 1; is_extended = 0; arb_id = 29'h101;
        wait(lookup_done);
        @(posedge clk);
        if (!id_blocked) $display("PASS: Standard allowed ID (0x101) passed.");
        else $display("FAIL: Standard allowed ID (0x101) blocked.");
        id_valid = 0;
        wait(!lookup_done);
        
        #50;
        // Test 3: Blocked Extended ID (0x0DEADBEE)
        @(posedge clk); id_valid = 1; is_extended = 1; arb_id = 29'h0DEADBEE;
        wait(lookup_done);
        @(posedge clk);
        if (id_blocked) $display("PASS: Extended blocked ID (0x0DEADBEE) detected.");
        else $display("FAIL: Extended blocked ID (0x0DEADBEE) not detected.");
        id_valid = 0;
        wait(!lookup_done);
        
        #50;
        // Test 4: Allowed Extended ID (0x01234567)
        @(posedge clk); id_valid = 1; is_extended = 1; arb_id = 29'h01234567;
        wait(lookup_done);
        @(posedge clk);
        if (!id_blocked) $display("PASS: Extended allowed ID (0x01234567) passed.");
        else $display("FAIL: Extended allowed ID (0x01234567) blocked.");
        id_valid = 0;
        
        #100;
        $display("Simulation complete.");
        $finish;
    end

endmodule
