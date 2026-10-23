`timescale 1ns/1ps
`default_nettype none
// Real RV32I executable, BRAM, board debouncer, PS/2 wire receiver, MMIO and VGA.
// Only countdown is shortened in this image; normal game timings are unchanged.
module tb_versus_system;
    reg clk=0,reset=1;
    always #20 clk=~clk;
    reg left_button=0,right_button=0,down_button=0,rot_l=0,rot_r=0,start=0,mode=1;
    reg psclk=1,psdata=1;
    wire [3:0] red,green,blue;
    wire hs,vs;wire [15:0] led;
    PuyoVersusSystem #(.CLK_HZ(1000000),.DEBOUNCE_MS(2),
        .IMEM_INIT_FILE("puyo_versus_integration.mem"),
        .DMEM_INIT_FILE("puyo_versus_integration_data.mem")) dut(
        .clk(clk),.reset(reset),.raw_left(left_button),.raw_right(right_button),
        .raw_down(down_button),.raw_rot_l(rot_l),.raw_rot_r(rot_r),.raw_start(start),
        .raw_mode(mode),.ps2_clk(psclk),.ps2_data(psdata),.vga_red(red),
        .vga_green(green),.vga_blue(blue),.hsync(hs),.vsync(vs),.led(led));
    integer cycles=0;
    reg [899:0] frozen;
    reg [31:0] retired,published;
    reg [3:0] y1,y2;
    wire [899:0] picture={dut.boards,dut.clear_masks,dut.active_words,dut.next_pairs,
        dut.scores,dut.status_words,dut.pending_counts,dut.match_status};
    always @(posedge clk) begin
        cycles=cycles+1;
        if(cycles>8000000)$fatal(1,"system watchdog pc=%h match=%h",dut.cpu.debug_pc,dut.match_status);
        if(!reset && dut.fault)$fatal(1,"CPU trap cause=%d value=%h pc=%h",
            dut.cpu.debug_trap_cause,dut.cpu.debug_trap_value,dut.cpu.debug_pc);
    end
    task wait_ms;
        input [31:0] duration;
        reg [31:0] began;
        begin
            began=dut.milliseconds;
            while(dut.milliseconds-began<duration)@(negedge clk);
        end
    endtask
    task serial_bit;
        input value;
        begin
            psclk=1;psdata=value;repeat(32)@(negedge clk);
            psclk=0;repeat(32)@(negedge clk);
            psclk=1;
        end
    endtask
    task send;
        input [7:0] byte_value;
        integer bitn;
        begin
            serial_bit(0);
            for(bitn=0;bitn<8;bitn=bitn+1)serial_bit(byte_value[bitn]);
            serial_bit(~^byte_value);serial_bit(1);
            psdata=1;repeat(64)@(negedge clk);
        end
    endtask
    task arrow_make;input[7:0]code;begin send('he0);send(code);end endtask
    task arrow_break;input[7:0]code;begin send('he0);send('hf0);send(code);end endtask
    task start_match;
        begin
            start=1;wait_ms(20);start=0;
            wait_ms(100);
            if(dut.match_status[3:1]!==2)$fatal(1,"no PLAY after countdown: %h",dut.match_status);
        end
    endtask
    initial begin
        repeat(8)@(negedge clk);reset=0;
        wait_ms(50);
        if(dut.match_status[0]!==1 || dut.match_status[3:1]!==0)$fatal(1,"menu/mode");
        // Hold rotation through countdown. It must be released before acting.
        send('h22);start_match();wait_ms(20);
        if(dut.active_words[25:24]!==0)$fatal(1,"held rotation leaked at start");
        send('hf0);send('h22);wait_ms(20);
        if(dut.active_words[15:10]!==dut.active_words[31:26] ||
           dut.next_pairs[5:0]!==dut.next_pairs[11:6])$fatal(1,"initial shared piece sequence");
        left_button=1;arrow_make('h74);wait_ms(30);
        if(dut.active_words[3:0]!==1 || dut.active_words[19:16]!==3)$fatal(1,"independent moves %h",dut.active_words);
        left_button=0;arrow_break('h74);wait_ms(20);
        send('h22);wait_ms(30);
        if(dut.active_words[25:24]!==1)$fatal(1,"P2 X make did not rotate");
        send('h22);send('h22);wait_ms(30);
        if(dut.active_words[25:24]!==1)$fatal(1,"typematic repeated rotation");
        send('hf0);send('h22);wait_ms(20);send('h22);wait_ms(20);
        if(dut.active_words[25:24]!==2)$fatal(1,"release/repress not detected");
        send('hf0);send('h22);wait_ms(20);
        y1=dut.active_words[7:4];y2=dut.active_words[23:20];
        left_button=1;down_button=1;arrow_make('h6b);arrow_make('h72);wait_ms(70);
        if(dut.active_words[7:4]<=y1 || dut.active_words[23:20]<=y2)$fatal(1,"simultaneous softdrops");
        if(dut.active_words[3:0]!==0 || dut.active_words[19:16]!==2)$fatal(1,"move+drop multikey");
        left_button=0;down_button=0;arrow_break('h6b);arrow_break('h72);wait_ms(20);
        start=1;mode=0;wait_ms(30);start=0;wait_ms(20);
        if(dut.match_status[0]!==1 || dut.match_status[3:1]!==2)$fatal(1,"midmatch admin input changed mode/reset");
        while(dut.mmio_valid)@(negedge clk);
        force dut.cpu.cpu_enable=1'b0;
        repeat(4)@(negedge clk);
        retired=dut.retired;published=dut.peripherals.publish_counter;frozen=picture;
        wait_ms(240);
        if(dut.retired!==retired || dut.peripherals.publish_counter!==published || picture!==frozen)
            $fatal(1,"game advanced without CPU");
        release dut.cpu.cpu_enable;wait_ms(30);
        if(dut.retired==retired || dut.peripherals.publish_counter==published)$fatal(1,"CPU resume");
        if(dut.keyboard_errors!==0)$fatal(1,"valid serial input reported error");

        // Full reset returns to menu. Solo path uses same CPU program and old UI.
        reset=1;repeat(8)@(negedge clk);reset=0;wait_ms(50);start_match();
        if(dut.match_status[0]!==0)$fatal(1,"solo selection");
        right_button=1;arrow_make('h74);wait_ms(30);
        if(dut.active_words[3:0]!==3 || dut.active_words[19:16]!==2)$fatal(1,"solo input isolation");
        right_button=0;arrow_break('h74);wait_ms(20);
        start=1;wait_ms(20);
        if(dut.match_status[3:1]!==1)$fatal(1,"solo SW14 restart not retained");
        start=0;wait_ms(100);
        if(dut.match_status[3:1]!==2 || dut.scores!==0 || dut.boards!==0)$fatal(1,"solo restart init");
        $display("PASS tb_versus_system: actual RV32I, PS/2+board independent/multikey, held/repeat gating, same NEXT, mode/start, CPU halt, solo regression; cycles=%0d",cycles);
        $finish;
    end
endmodule
`default_nettype wire
