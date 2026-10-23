`timescale 1ns/1ps
`default_nettype none
// Basys 3 J2 USB HID -> PIC24 PS/2 bridge, receive only.
// Keys are HELD LEVELS: [0]left [1]right [2]down [3]Z [4]X.
// Edge detection, DAS/ARR and new-match held-key suppression belong to software.
module PuyoKeyboard #(
    parameter integer CLK_HZ=25000000,
    parameter integer FILTER_CYCLES=8,
    parameter integer FRAME_TIMEOUT_US=2000
)(
    input wire clk,reset,ps2_clk,ps2_data,
    output reg [4:0] keys,
    output reg [31:0] error_count,
    output reg byte_valid,
    output reg [7:0] last_byte
);
    localparam integer FILTER=(FILTER_CYCLES<1) ? 1 : FILTER_CYCLES;
    localparam integer FILTER_WIDTH=(FILTER<2) ? 1 : $clog2(FILTER);
    localparam integer TIMEOUT_RAW=((CLK_HZ/1000)*FRAME_TIMEOUT_US)/1000;
    localparam integer TIMEOUT=(TIMEOUT_RAW<1) ? 1 : TIMEOUT_RAW;
    localparam integer TIMEOUT_WIDTH=(TIMEOUT<2) ? 1 : $clog2(TIMEOUT);

    // The XDC constrains asynchronous inputs only to sync_meta/D.
    // bit1 = clock, bit0 = data. Data is stable around the delayed clock edge.
    (* ASYNC_REG="TRUE" *) reg [1:0] sync_meta,sync_clean;
    reg filtered_clk,previous_clk;
    reg [FILTER_WIDTH-1:0] filter_count;
    wire falling_edge=previous_clk && !filtered_clk;
    reg receiving;
    reg [3:0] bit_index;
    reg [7:0] data_byte;
    reg parity_bit;
    reg extended_code,break_code;
    reg [2:0] pause_remaining;
    reg [TIMEOUT_WIDTH-1:0] timeout_count;
    wire waiting=receiving || extended_code || break_code || (pause_remaining!=0);

    // No PS/2-derived clock is used by any state register.
    always @(posedge clk) begin
        if(reset) begin
            sync_meta<=2'b11;
            sync_clean<=2'b11;
            filtered_clk<=1'b1;
            previous_clk<=1'b1;
            filter_count<=0;
        end else begin
            sync_meta<={ps2_clk,ps2_data};
            sync_clean<=sync_meta;
            previous_clk<=filtered_clk;
            if(sync_clean[1]==filtered_clk) filter_count<=0;
            else if(filter_count==FILTER-1) begin
                filtered_clk<=sync_clean[1];
                filter_count<=0;
            end else filter_count<=filter_count+1'b1;
        end
    end

    always @(posedge clk) begin
        if(reset) begin
            keys<=0;
            error_count<=0;
            byte_valid<=0;
            last_byte<=0;
            receiving<=0;
            bit_index<=0;
            data_byte<=0;
            parity_bit<=0;
            extended_code<=0;
            break_code<=0;
            pause_remaining<=0;
            timeout_count<=0;
        end else begin
            byte_valid<=0;
            if(falling_edge) begin
                timeout_count<=0;
                if(!receiving) begin
                    if(!sync_clean[0]) begin
                        receiving<=1;
                        bit_index<=0;
                        data_byte<=0;
                    end else begin
                        // A start bit must be zero. Fail closed on bad framing.
                        keys<=0;
                        extended_code<=0;
                        break_code<=0;
                        pause_remaining<=0;
                        if(error_count!=32'hffffffff) error_count<=error_count+1'b1;
                    end
                end else if(bit_index<8) begin
                    data_byte[bit_index]<=sync_clean[0];
                    bit_index<=bit_index+1'b1;
                end else if(bit_index==8) begin
                    parity_bit<=sync_clean[0];
                    bit_index<=9;
                end else begin
                    receiving<=0;
                    bit_index<=0;
                    if(!sync_clean[0] || !((^data_byte)^parity_bit)) begin
                        // Invalid byte is NOT reported and cannot become a key.
                        keys<=0;
                        extended_code<=0;
                        break_code<=0;
                        pause_remaining<=0;
                        if(error_count!=32'hffffffff) error_count<=error_count+1'b1;
                    end else begin
                        byte_valid<=1;
                        last_byte<=data_byte;
                        if(pause_remaining!=0) begin
                            // Pause: E1 14 77 E1 F0 14 F0 77. Swallow seven
                            // bytes after E1, including embedded E1/F0 bytes.
                            pause_remaining<=pause_remaining-1'b1;
                        end else if(data_byte==8'he1) begin
                            extended_code<=0;
                            break_code<=0;
                            pause_remaining<=7;
                        end else if(data_byte==8'haa || data_byte==8'hfc ||
                                    data_byte==8'hfd || data_byte==8'h00 || data_byte==8'hff) begin
                            // BAT, BAT failure or keyboard overrun: stale held
                            // states cannot be trusted. AA is not an error.
                            keys<=0;
                            extended_code<=0;
                            break_code<=0;
                            if(data_byte!=8'haa && error_count!=32'hffffffff)
                                error_count<=error_count+1'b1;
                        end else if(data_byte==8'he0) begin
                            if(extended_code || break_code) begin
                                keys<=0;
                                extended_code<=0;
                                break_code<=0;
                                if(error_count!=32'hffffffff) error_count<=error_count+1'b1;
                            end else extended_code<=1;
                        end else if(data_byte==8'hf0) begin
                            if(break_code) begin
                                keys<=0;
                                extended_code<=0;
                                break_code<=0;
                                if(error_count!=32'hffffffff) error_count<=error_count+1'b1;
                            end else break_code<=1;
                        end else begin
                            if(extended_code) begin
                                case(data_byte)
                                    8'h6b: keys[0]<=!break_code;
                                    8'h74: keys[1]<=!break_code;
                                    8'h72: keys[2]<=!break_code;
                                    default: ;
                                endcase
                            end else begin
                                case(data_byte)
                                    8'h1a: keys[3]<=!break_code;
                                    8'h22: keys[4]<=!break_code;
                                    default: ;
                                endcase
                            end
                            // Unknown scan codes consume any prefix as well.
                            // Repeated make merely writes an already-high bit.
                            extended_code<=0;
                            break_code<=0;
                        end
                    end
                end
            end else if(waiting) begin
                if(timeout_count==TIMEOUT-1) begin
                    receiving<=0;
                    bit_index<=0;
                    extended_code<=0;
                    break_code<=0;
                    pause_remaining<=0;
                    timeout_count<=0;
                    keys<=0;
                    if(error_count!=32'hffffffff) error_count<=error_count+1'b1;
                end else timeout_count<=timeout_count+1'b1;
            end else timeout_count<=0;
            // Deliberately no held-key timeout: a silent held key is legal,
            // and keyboard typematic reports only one of several held keys.
        end
    end
endmodule
`default_nettype wire
