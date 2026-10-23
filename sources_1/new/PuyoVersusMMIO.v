`timescale 1ns/1ps
`default_nettype none
// Two display mailboxes, NOT game engines. All rules and arbitration run on RV32I.
// CPU selects one staging bank, writes both pictures, then commits BOTH together.
module PuyoVersusMMIO(
    input wire clk,reset,valid,write,
    input wire [31:0] address,write_data,
    output wire ready, output reg [31:0] read_data,
    input wire [5:0] keys_p1,
    input wire [4:0] keys_p2,
    input wire mode_versus,
    input wire [31:0] milliseconds,keyboard_errors,
    input wire frame_tick,
    input wire [31:0] counter_cycle,counter_instret,counter_mem_stall,
    input wire [31:0] counter_load_stall,counter_flush,
    output reg [503:0] boards,
    output reg [167:0] clear_masks,
    output reg [31:0] active_words,
    output reg [11:0] next_pairs,
    output reg [63:0] scores,
    output reg [55:0] status_words,
    output reg [31:0] pending_counts,match_status
);
    reg bank;
    reg [31:0] index;
    reg [503:0] stage_boards;
    reg [167:0] stage_clear;
    reg [31:0] stage_active,stage_pending,stage_match;
    reg [11:0] stage_next;
    reg [63:0] stage_scores;
    reg [55:0] stage_status;
    reg [31:0] frame_counter,publish_counter;
    wire transfer=valid && ready;
    assign ready=!reset;
    always @* begin
        read_data=0;
        case(address)
            32'h2000: read_data={26'd0,keys_p1};
            32'h2004: read_data=milliseconds;
            32'h2008: read_data=counter_cycle;
            32'h200c: read_data=32'h50565933; // PVY3: RV32I versus platform.
            32'h2010: read_data=frame_counter;
            32'h2014: read_data=publish_counter;
            32'h2018: read_data={27'd0,keys_p2};
            32'h201c: read_data={31'd0,mode_versus};
            32'h2040: read_data=counter_cycle;
            32'h2044: read_data=counter_instret;
            32'h2048: read_data=counter_mem_stall;
            32'h204c: read_data=counter_load_stall;
            32'h2050: read_data=counter_flush;
            32'h2068: read_data=keyboard_errors;
            default: read_data=0;
        endcase
    end
    always @(posedge clk) begin
        if(reset) begin
            bank<=0; index<=0;
            stage_boards<=0; stage_clear<=0; stage_active<=0; stage_next<=0;
            stage_scores<=0; stage_status<=0; stage_pending<=0; stage_match<=0;
            boards<=0; clear_masks<=0; active_words<=0; next_pairs<=0;
            scores<=0; status_words<=0; pending_counts<=0; match_status<=0;
            frame_counter<=0; publish_counter<=0;
        end else begin
            if(frame_tick) frame_counter<=frame_counter+1'b1;
            if(transfer && write) begin
                case(address)
                    32'h2020: index<=write_data;
                    32'h2024: if(index<84) begin
                        if(bank) begin
                            stage_boards[252+index[6:0]*3 +:3]<=write_data[2:0];
                            stage_clear[84+index[6:0]]<=write_data[8];
                        end else begin
                            stage_boards[index[6:0]*3 +:3]<=write_data[2:0];
                            stage_clear[index[6:0]]<=write_data[8];
                        end
                    end
                    32'h2028: if(bank) stage_active[31:16]<=write_data[15:0];
                                 else stage_active[15:0]<=write_data[15:0];
                    32'h202c: if(bank) stage_next[11:6]<=write_data[5:0];
                                 else stage_next[5:0]<=write_data[5:0];
                    32'h2030: if(bank) stage_scores[63:32]<=write_data;
                                 else stage_scores[31:0]<=write_data;
                    32'h2034: if(bank) stage_status[55:28]<=write_data[27:0];
                                 else stage_status[27:0]<=write_data[27:0];
                    32'h2038: if(write_data[0]) begin
                        boards<=stage_boards; clear_masks<=stage_clear;
                        active_words<=stage_active; next_pairs<=stage_next;
                        scores<=stage_scores; status_words<=stage_status;
                        pending_counts<=stage_pending; match_status<=stage_match;
                        publish_counter<=publish_counter+1'b1;
                    end
                    32'h203c: bank<=write_data[0];
                    32'h2060: if(bank) stage_pending[31:16]<=write_data[15:0];
                                 else stage_pending[15:0]<=write_data[15:0];
                    32'h2064: stage_match<=write_data;
                    default: ;
                endcase
            end
        end
    end
endmodule
`default_nettype wire
