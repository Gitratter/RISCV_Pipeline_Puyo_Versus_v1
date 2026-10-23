`timescale 1ns/1ps
`default_nettype none
module tb_keyboard;
    reg clk=0;
    always #500 clk=~clk; // 1 MHz test clock, real PS/2 bit durations.
    reg reset=1,ps2_clk=1,ps2_data=1;
    wire [4:0] keys;
    wire [31:0] error_count;
    wire byte_valid;
    wire [7:0] last_byte;
    PuyoKeyboard #(.CLK_HZ(1000000),.FILTER_CYCLES(8),.FRAME_TIMEOUT_US(2000)) dut(
        .clk(clk),.reset(reset),.ps2_clk(ps2_clk),.ps2_data(ps2_data),
        .keys(keys),.error_count(error_count),.byte_valid(byte_valid),.last_byte(last_byte));
    integer valid_bytes=0,z_edges=0,x_edges=0;
    reg [4:0] previous_keys=0;
    reg previous_valid=0;
    always @(posedge clk) begin
        if(!reset) begin
            if(byte_valid) valid_bytes=valid_bytes+1;
            if(byte_valid && previous_valid) $fatal(1,"byte_valid wider than one clock");
            if(keys[3] && !previous_keys[3]) z_edges=z_edges+1;
            if(keys[4] && !previous_keys[4]) x_edges=x_edges+1;
        end
        previous_keys=keys;
        previous_valid=byte_valid;
    end
    task bit_send;
        input value;
        begin
            ps2_data=value;
            #20000;
            ps2_clk=0;
            #40000;
            ps2_clk=1;
            #20000;
        end
    endtask
    task frame;
        input [7:0] value;
        input bad_parity,bad_stop;
        integer i;
        reg parity;
        begin
            parity=~(^value);
            bit_send(0);
            for(i=0;i<8;i=i+1) bit_send(value[i]);
            bit_send(parity^bad_parity);
            bit_send(!bad_stop);
            ps2_data=1;
            #40000;
        end
    endtask
    task send;
        input [7:0] value;
        begin frame(value,0,0); end
    endtask
    task check_keys;
        input [4:0] expected;
        begin
            #4000;
            if(keys!==expected) $fatal(1,"keys expected=%b got=%b last=%h",expected,keys,last_byte);
        end
    endtask
    task arrow_make;
        input [7:0] value;
        begin send(8'he0);send(value);end
    endtask
    task arrow_break;
        input [7:0] value;
        begin send(8'he0);send(8'hf0);send(value);end
    endtask
    integer n,e,z,x;
    initial begin
        #10000;reset=0;#30000;
        // Three clock cycles of noise must not create a falling-edge event.
        ps2_clk=0;#3000;ps2_clk=1;#30000;
        if(valid_bytes!=0 || error_count!=0) $fatal(1,"clock glitch not rejected");
        send(8'h1a);check_keys(5'b01000);
        z=z_edges;
        send(8'h1a);send(8'h1a);send(8'h1a);
        check_keys(5'b01000);
        if(z_edges!=z) $fatal(1,"Z typematic created a new edge");
        send(8'h22);check_keys(5'b11000);
        x=x_edges;send(8'h22);check_keys(5'b11000);
        if(x_edges!=x) $fatal(1,"X typematic created a new edge");
        arrow_make(8'h6b);check_keys(5'b11001);
        arrow_make(8'h72);check_keys(5'b11101);
        arrow_break(8'h6b);check_keys(5'b11100);
        send(8'hf0);send(8'h1a);check_keys(5'b10100);
        arrow_make(8'h74);check_keys(5'b10110);
        arrow_break(8'h74);arrow_break(8'h72);
        send(8'hf0);send(8'h22);check_keys(0);
        if(error_count!=0) $fatal(1,"valid simultaneous key sequence produced errors");

        // Numeric keypad / unrelated / extended Z codes are not these keys.
        send(8'h6b);send(8'he0);send(8'h1a);
        send(8'h12);send(8'hf0);send(8'h12);
        send(8'he0);send(8'hf0);send(8'h75);check_keys(0);
        send(8'h1a);check_keys(5'b01000);
        if(z_edges!=z+1) $fatal(1,"release/new Z press did not rearm");
        send(8'hf0);send(8'h1a);check_keys(0);

        // Silent held keys must not expire, even after frame timeout duration.
        arrow_make(8'h6b);e=error_count;#10000000;check_keys(5'b00001);
        if(error_count!=e) $fatal(1,"held key wrongly treated as missing frame");
        arrow_make(8'h74);check_keys(5'b00011); // software cancels left+right
        arrow_break(8'h6b);arrow_break(8'h74);check_keys(0);

        // Real Pause sequence plus adversarial payload with mapped key bytes.
        send(8'h22);x=x_edges;
        send(8'he1);send(8'h14);send(8'h77);send(8'he1);
        send(8'hf0);send(8'h14);send(8'hf0);send(8'h77);check_keys(5'b10000);
        send(8'he1);send(8'h1a);send(8'h22);send(8'he0);
        send(8'h6b);send(8'hf0);send(8'h1a);send(8'h22);check_keys(5'b10000);
        if(x_edges!=x) $fatal(1,"Pause payload created rotation edge");
        send(8'hf0);send(8'h22);check_keys(0);

        // Bad parity, stop and start bits: discard byte, clear stale levels.
        arrow_make(8'h72);e=error_count;n=valid_bytes;
        frame(8'h1a,1,0);check_keys(0);
        if(error_count!=e+1 || valid_bytes!=n) $fatal(1,"bad parity handling");
        send(8'h22);check_keys(5'b10000);
        e=error_count;n=valid_bytes;frame(8'h1a,0,1);check_keys(0);
        if(error_count!=e+1 || valid_bytes!=n) $fatal(1,"bad stop handling");
        e=error_count;bit_send(1);#40000;check_keys(0);
        if(error_count!=e+1) $fatal(1,"bad start handling");
        send(8'h1a);check_keys(5'b01000);

        // Broken frame and prefix timeout cannot leave a latent E0 or F0.
        e=error_count;bit_send(0);bit_send(1);bit_send(0);
        ps2_data=1;#2100000;check_keys(0);
        if(error_count!=e+1) $fatal(1,"partial frame timeout");
        send(8'he0);e=error_count;#2100000;send(8'h6b);check_keys(0);
        if(error_count!=e+1) $fatal(1,"E0 timeout");
        send(8'hf0);e=error_count;#2100000;send(8'h1a);check_keys(5'b01000);
        if(error_count!=e+1) $fatal(1,"F0 timeout/recovery");
        send(8'he1);send(8'h14);e=error_count;#2100000;check_keys(0);
        if(error_count!=e+1) $fatal(1,"incomplete Pause timeout");
        send(8'h22);check_keys(5'b10000);

        // Device reset/failures/overrun clear all held levels and prefixes.
        e=error_count;send(8'haa);check_keys(0);
        if(error_count!=e) $fatal(1,"BAT success should not count as error");
        send(8'h1a);send(8'hfc);check_keys(0);
        send(8'h22);send(8'hfd);check_keys(0);
        arrow_make(8'h6b);send(8'h00);check_keys(0);
        arrow_make(8'h74);send(8'hff);check_keys(0);
        if(error_count!=e+4) $fatal(1,"BAT failure/overrun counts");
        e=error_count;send(8'hf0);send(8'he0);send(8'h6b);check_keys(0);
        send(8'he0);send(8'he0);send(8'h74);check_keys(0);
        send(8'hf0);send(8'hf0);send(8'h72);check_keys(0);
        if(error_count!=e+3) $fatal(1,"malformed prefix counts");

        // Preload only the diagnostic counter to exercise the 32-bit limit.
        dut.error_count=32'hfffffffe;
        send(8'h00);
        if(error_count!=32'hffffffff) $fatal(1,"error count did not reach maximum");
        send(8'hff);
        if(error_count!=32'hffffffff) $fatal(1,"error count wrapped instead of saturating");

        // Receiver reset clears partial decoding, counters and all key levels.
        send(8'h22);send(8'he0);
        reset=1;#10000;reset=0;#30000;
        check_keys(0);
        if(error_count!=0 || last_byte!=0 || byte_valid) $fatal(1,"reset state");
        send(8'h6b);check_keys(0);
        arrow_make(8'h6b);check_keys(1);arrow_break(8'h6b);check_keys(0);
        if(last_byte!=8'h6b || error_count!=0) $fatal(1,"final valid frame");
        $display("PASS tb_keyboard: frames/filter/parity/stop/start/timeouts/E0/F0/Pause/BAT/held-repeat/multikey/reset/saturation (%0d valid bytes)",valid_bytes);
        $finish;
    end
    initial begin #300000000;$fatal(1,"tb_keyboard watchdog");end
endmodule
`default_nettype wire
