`timescale 1ns/1ps
module tb_versus_mmio;
    reg clk=0,reset=1,valid=0,wr=0,frame_tick=0;
    always #5 clk=~clk;
    reg [31:0] address=0,data=0;
    wire ready;
    wire [31:0] rd,active,pending,match_state;
    wire [503:0] boards;
    wire [167:0] masks;
    wire [11:0] next_pairs;
    wire [63:0] scores;
    wire [55:0] states;
    PuyoVersusMMIO dut(.clk(clk),.reset(reset),.valid(valid),.write(wr),
        .address(address),.write_data(data),.ready(ready),.read_data(rd),
        .keys_p1(6'h25),.keys_p2(5'h13),.mode_versus(1'b1),.milliseconds(32'd200),
        .keyboard_errors(32'd7),.frame_tick(frame_tick),.counter_cycle(32'd301),
        .counter_instret(32'd302),.counter_mem_stall(32'd303),.counter_load_stall(32'd304),
        .counter_flush(32'd305),.boards(boards),.clear_masks(masks),.active_words(active),
        .next_pairs(next_pairs),.scores(scores),.status_words(states),
        .pending_counts(pending),.match_status(match_state));
    task put;
        input [31:0] a,d;
        begin
            @(negedge clk);address=a;data=d;valid=1;wr=1;
            @(negedge clk);valid=0;wr=0;
        end
    endtask
    task get;
        input [31:0] a,d;
        begin @(negedge clk);address=a;#1;if(rd!==d)$fatal(1,"read %h got%h expected%h",a,rd,d);end
    endtask
    initial begin
        repeat(3)@(negedge clk);reset=0;
        get('h2000,'h25);get('h2018,'h13);get('h201c,1);get('h2068,7);
        get('h200c,'h50565933);get('h2004,200);get('h2044,302);get('h20fc,0);
        put('h2020,0);put('h2024,'h101);put('h2028,'h4321);put('h202c,'h12);
        put('h2030,32'hffffffff);put('h2034,'h9123456);put('h2060,16'd31);
        put('h203c,1);put('h2020,83);put('h2024,'h105);put('h2028,'h8765);
        put('h202c,'h21);put('h2030,32'd123);put('h2034,'ha654321);put('h2060,65535);
        put('h2064,'h135);
        if(boards!==0 || scores!==0 || pending!==0 || match_state!==0)$fatal(1,"staging leaks");
        put('h2038,1);
        if(boards[2:0]!==1 || boards[501+:3]!==5 || boards[500:3]!==0 ||
            masks!==(168'd1 | (168'd1<<167)) || active!==32'h87654321 ||
            next_pairs!=={6'h21,6'h12} || scores!=={32'd123,32'hffffffff} ||
            states!=={28'ha654321,28'h9123456} || pending!=={16'd65535,16'd31} ||
            match_state!==32'h135 || dut.publish_counter!==1)$fatal(1,"atomic fields mismatch");
        put('h2020,84);put('h2024,4);put('h2020,128);put('h2024,4);
        put('h2020,65536);put('h2024,4);put('h2020,32'hffffffff);put('h2024,4);
        put('h2038,0);if(dut.publish_counter!==1)$fatal(1,"zero commit accepted");
        put('h2038,1);
        if(boards[501+:3]!==5 || boards[2:0]!==1 || boards[500:3]!==0)$fatal(1,"invalid index alias");
        put('h203c,0);put('h2020,0);put('h2024,3);
        if(boards[2:0]!==1)$fatal(1,"second staging leaks");
        put('h2038,1);
        if(boards[2:0]!==3 || boards[501+:3]!==5)$fatal(1,"bank isolation");
        @(negedge clk);frame_tick=1;@(negedge clk);frame_tick=0;
        get('h2010,1);get('h2014,3);
        @(negedge clk);reset=1;repeat(3)@(negedge clk);
        if(ready!==0 || boards!==0 || pending!==0 || match_state!==0)$fatal(1,"reset failed");
        $display("PASS tb_versus_mmio: independent banks, atomic two-player commit, bounds, input and counters");
        $finish;
    end
endmodule
