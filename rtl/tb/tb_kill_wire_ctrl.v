`timescale 1ns/1ps

module tb_kill_wire_ctrl;

    reg clk;
    reg rst_n;
    reg id_blocked;
    reg lookup_done;
    reg bit_valid;
    
    wire can_tx_override;
    wire can_tx_oe;
    wire kill_active;

    kill_wire_ctrl uut (
        .clk(clk),
        .rst_n(rst_n),
        .id_blocked(id_blocked),
        .lookup_done(lookup_done),
        .bit_valid(bit_valid),
        .can_tx_override(can_tx_override),
        .can_tx_oe(can_tx_oe),
        .kill_active(kill_active)
    );

    always #10 clk = ~clk; // 50MHz clock

    initial begin
        $dumpfile("kill_wire_ctrl.vcd");
        $dumpvars(0, tb_kill_wire_ctrl);
        
        clk = 0;
        rst_n = 0;
        id_blocked = 0;
        lookup_done = 0;
        bit_valid = 0;
        
        #100 rst_n = 1;
        
        // Wait some time
        #50;
        
        // Assert lookup_done and id_blocked
        @(posedge clk);
        lookup_done = 1;
        id_blocked = 1;
        @(posedge clk);
        lookup_done = 0;
        id_blocked = 0;
        
        $display("Triggered kill state...");
        
        // Wait for KILLING to finish (600 cycles) + COOLDOWN (100 cycles)
        #(750 * 20); // 750 cycles roughly
        
        $display("Testing rapid back-to-back kills...");
        // Trigger again rapidly
        @(posedge clk);
        lookup_done = 1;
        id_blocked = 1;
        @(posedge clk);
        lookup_done = 0;
        id_blocked = 0;
        
        #(750 * 20);
        
        $display("Simulation complete.");
        $finish;
    end

    // Monitors for states and output checks
    initial begin
        // Monitor for kill_active
        forever @(posedge clk) begin
            if (uut.state == 2'd1) begin // KILLING
                if (can_tx_override !== 0 || can_tx_oe !== 1 || kill_active !== 1) begin
                    $display("FAIL: Outputs incorrect during KILLING at time %t", $time);
                end
            end else begin
                if (can_tx_oe !== 0 || kill_active !== 0) begin
                    $display("FAIL: Outputs incorrect during non-KILLING at time %t", $time);
                end
            end
        end
    end

endmodule
