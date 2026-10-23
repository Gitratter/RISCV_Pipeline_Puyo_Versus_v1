`timescale 1ns/1ps
`default_nettype none
// One CPU updates two independent game state machines and resolves both events
// from the same logical update. RTL only provides input, clocks and scanout.
module PuyoVersusSystem #(
    parameter integer CLK_HZ=25000000,
    parameter integer DEBOUNCE_MS=10,
    parameter IMEM_INIT_FILE="puyo_versus.mem",
    parameter DMEM_INIT_FILE="puyo_versus_data.mem",
    parameter integer IMEM_WORDS=4096
)(
    input wire clk,reset,
    input wire raw_left,raw_right,raw_down,raw_rot_l,raw_rot_r,raw_start,raw_mode,
    input wire ps2_clk,ps2_data,
    output wire [3:0] vga_red,vga_green,vga_blue,
    output wire hsync,vsync,output wire [15:0] led
);
    wire tick_ms;
    wire [31:0] milliseconds;
    MillisecondTick #(.CLK_HZ(CLK_HZ)) timer(.clk(clk),.reset(reset),
        .tick_ms(tick_ms),.milliseconds(milliseconds));
    wire [5:0] keys_p1;
    wire [4:0] keys_p2;
    wire mode_versus;
    wire [31:0] keyboard_errors;
    PuyoButtons #(.DEBOUNCE_MS(DEBOUNCE_MS)) controls(.clk(clk),.reset(reset),
        .tick_ms(tick_ms),.raw_left(raw_left),.raw_right(raw_right),
        .raw_down(raw_down),.raw_rot_l(raw_rot_l),.raw_rot_r(raw_rot_r),
        .raw_restart(raw_start),.stable_keys(keys_p1));
    PuyoModeSwitch #(.DEBOUNCE_MS(DEBOUNCE_MS)) mode_input(.clk(clk),.reset(reset),
        .tick_ms(tick_ms),.raw_mode(raw_mode),.stable_mode(mode_versus));
    PuyoKeyboard #(.CLK_HZ(CLK_HZ)) keyboard(.clk(clk),.reset(reset),
        .ps2_clk(ps2_clk),.ps2_data(ps2_data),.keys(keys_p2),
        .error_count(keyboard_errors),.byte_valid(),.last_byte());
    wire mmio_valid,mmio_write,mmio_ready;
    wire [31:0] mmio_addr,mmio_wdata,mmio_rdata;
    wire [31:0] cycles,retired,mem_stalls,load_stalls,flushes;
    wire fault;
    wire [7:0] legacy_output;
    reg cpu_reset=1'b1;
    always @(posedge clk) cpu_reset<=reset;
    TD8_RISCV_V31_Core #(.IMEM_WORDS(IMEM_WORDS),.IMEM_INIT_FILE(IMEM_INIT_FILE),
        .DMEM_INIT_FILE(DMEM_INIT_FILE)) cpu(.clk(clk),.reset(cpu_reset),
        .cpu_enable(1'b1),.input_port(8'd0),.debug_reg_addr(5'd0),
        .output_port(legacy_output),.debug_pc(),.debug_instruction(),
        .debug_reg_data(),.debug_mem_word0(),.debug_trap_cause(),.debug_trap_value(),
        .fault(fault),.retire_valid(),.retire_pc(),.retire_instruction(),
        .mmio_valid(mmio_valid),.mmio_write(mmio_write),.mmio_addr(mmio_addr),
        .mmio_wdata(mmio_wdata),.mmio_rdata(mmio_rdata),.mmio_ready(mmio_ready),
        .counter_cycle(cycles),.counter_instret(retired),.counter_mem_stall(mem_stalls),
        .counter_load_stall(load_stalls),.counter_flush(flushes));
    wire [503:0] boards;
    wire [167:0] clear_masks;
    wire [31:0] active_words,pending_counts,match_status;
    wire [11:0] next_pairs;
    wire [63:0] scores;
    wire [55:0] status_words;
    wire frame_tick;
    PuyoVersusMMIO peripherals(.clk(clk),.reset(reset),.valid(mmio_valid),
        .write(mmio_write),.address(mmio_addr),.write_data(mmio_wdata),
        .ready(mmio_ready),.read_data(mmio_rdata),.keys_p1(keys_p1),.keys_p2(keys_p2),
        .mode_versus(mode_versus),.milliseconds(milliseconds),.keyboard_errors(keyboard_errors),
        .frame_tick(frame_tick),.counter_cycle(cycles),.counter_instret(retired),
        .counter_mem_stall(mem_stalls),.counter_load_stall(load_stalls),.counter_flush(flushes),
        .boards(boards),.clear_masks(clear_masks),.active_words(active_words),
        .next_pairs(next_pairs),.scores(scores),.status_words(status_words),
        .pending_counts(pending_counts),.match_status(match_status));
    PuyoVersusVideo video(.clk(clk),.reset(reset),.boards(boards),
        .clear_masks(clear_masks),.active_words(active_words),.next_pairs(next_pairs),
        .scores(scores),.status_words(status_words),.pending_counts(pending_counts),
        .match_status(match_status),.vga_red(vga_red),.vga_green(vga_green),
        .vga_blue(vga_blue),.hsync(hsync),.vsync(vsync),.frame_tick(frame_tick));
    assign led={fault,(match_status[3:1]==3'd3),status_words[25],status_words[53],
        retired[19:16],legacy_output};
endmodule

module PuyoModeSwitch #(parameter integer DEBOUNCE_MS=10)(
    input wire clk,reset,tick_ms,raw_mode,output reg stable_mode
);
    (* ASYNC_REG="TRUE" *) reg meta,sync;
    reg [15:0] count;
    always @(posedge clk) begin
        if(reset) begin meta<=0;sync<=0;count<=0;stable_mode<=0;end
        else begin
            meta<=raw_mode;sync<=meta;
            if(tick_ms) begin
                if(sync==stable_mode) count<=0;
                else if(count>=DEBOUNCE_MS-1) begin stable_mode<=sync;count<=0;end
                else count<=count+1'b1;
            end
        end
    end
endmodule

module RISCV_PUYO_VERSUS_TOP(
    input wire clk,input wire [15:0] sw,
    input wire btnL,btnR,btnD,btnU,btnC,
    input wire PS2Clk,PS2Data,
    output wire [3:0] vgaRed,vgaGreen,vgaBlue,
    output wire Hsync,Vsync,output wire [15:0] led,
    output wire [6:0] seg,output wire [3:0] an,output wire dp
);
    wire clk25,locked,reset;
    Clock25 clock_generator(.clk100(clk),.reset(sw[15]),.clk25(clk25),.locked(locked));
    ResetSync reset_synchronizer(.clk(clk25),.async_reset(sw[15] || !locked),.reset(reset));
    PuyoVersusSystem system(.clk(clk25),.reset(reset),.raw_left(btnL),.raw_right(btnR),
        .raw_down(btnD),.raw_rot_l(btnC),.raw_rot_r(btnU),.raw_start(sw[14]),.raw_mode(sw[0]),
        .ps2_clk(PS2Clk),.ps2_data(PS2Data),.vga_red(vgaRed),.vga_green(vgaGreen),
        .vga_blue(vgaBlue),.hsync(Hsync),.vsync(Vsync),.led(led));
    assign seg=7'b1111111;
    assign an=4'b1111;
    assign dp=1'b1;
endmodule
`default_nettype wire
