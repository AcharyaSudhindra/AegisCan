`timescale 1ns/1ps
// Exercises the real BTL -> destuffer -> ID lookup -> kill path.
// Sends stuffed arbitration/control prefixes, not complete CRC-checked frames.
module tb_can_attack_regression;
    reg clk = 0;
    always #10 clk = ~clk;
    reg rst_n = 0, can_rx = 1;
    wire can_tx, led;
    aegis_top uut(.clk(clk), .rst_n(rst_n), .can_rx(can_rx),
                  .can_tx(can_tx), .led_kill_active(led));
    integer run_length, id_count = 0, kill_count = 0;
    reg last_bit;
    reg [10:0] expected_id;
    always @(posedge clk) begin
        if (rst_n && uut.deser_id_valid) begin
            if (uut.deser_arb_id !== {18'd0, expected_id} ||
                uut.deser_is_extended !== 0)
                $fatal(1, "Wrong ID: expected %h got %h", expected_id, uut.deser_arb_id);
            id_count = id_count + 1;
        end
    end
    always @(negedge can_tx) if (rst_n) kill_count = kill_count + 1;

    task send_bit;
        input b;
        begin
            can_rx = b; #2000;
            if (b == last_bit) run_length = run_length + 1;
            else begin last_bit = b; run_length = 1; end
            if (run_length == 5) begin
                can_rx = ~b; #2000;
                last_bit = ~b; run_length = 1;
            end
        end
    endtask
    task check_id;
        input [10:0] value;
        input blocked;
        integer i, old_ids, old_kills;
        begin
            expected_id = value;
            old_ids = id_count; old_kills = kill_count;
            run_length = 0; last_bit = 1;
            send_bit(0);
            for (i = 10; i >= 0; i = i - 1) send_bit(value[i]);
            send_bit(0); // RTR
            send_bit(0); // IDE
            send_bit(0); // r0
            // Let the kill controller finish its pulse and the bus recover.
            can_rx = 1; #60000;
            if (id_count != old_ids + 1 || kill_count != old_kills + blocked)
                $fatal(1, "ID %h: decode/kill count mismatch", value);
            if (blocked && led !== 1) $fatal(1, "Kill LED did not remain visible");
            if (can_tx !== 1) $fatal(1, "CAN TX did not release");
            $display("PASS: ID %h blocked=%b", value, blocked);
        end
    endtask
    initial begin
        #200; rst_n = 1; #25000;
        if (led !== 0) $fatal(1, "LED did not reset");
        check_id(11'h050, 0);
        check_id(11'h100, 1);
        check_id(11'h001, 1);
        check_id(11'h010, 1);
        check_id(11'h1AA, 1);
        check_id(11'h555, 0); // Exercises dominant/recessive ID transitions.
        check_id(11'h650, 0); // Exercises ID[10]=1.
        check_id(11'h050, 0);
        // Verify the complete hold interval without forcing DUT registers.
        wait (uut.led_stretch_cnt == 1);
        @(negedge clk);
        if (led !== 1) $fatal(1, "LED expired early");
        @(posedge clk); #1;
        if (led !== 0) $fatal(1, "LED did not expire");
        check_id(11'h001, 1);
        rst_n = 0; #1;
        if (led !== 0) $fatal(1, "Reset did not clear active LED");
        $display("PASS: attack regression and 200 ms LED hold/reset");
        $finish;
    end
    initial begin #250000000; $fatal(1, "Regression timed out"); end
endmodule
