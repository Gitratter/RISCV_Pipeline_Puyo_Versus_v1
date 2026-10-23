`timescale 1ns/1ps
`default_nettype none
// VGA owns display snapshots only; all game/garbage/match rules are RV32I software.
// The original solo renderer is unchanged and remains selected in solo PLAY.
module PuyoVersusVideo(
    input wire clk, reset,
    input wire [503:0] boards,
    input wire [167:0] clear_masks,
    input wire [31:0] active_words,
    input wire [11:0] next_pairs,
    input wire [63:0] scores,
    input wire [55:0] status_words,
    input wire [31:0] pending_counts, match_status,
    output wire [3:0] vga_red, vga_green, vga_blue,
    output wire hsync, vsync,
    output reg frame_tick
);
    wire [3:0] solo_r,solo_g,solo_b;
    wire solo_hs,solo_vs,solo_frame;
    PuyoVideo solo(
        .clk(clk),.reset(reset),.board_flat(boards[251:0]),
        .active_valid(status_words[24]),.active_x(active_words[3:0]),
        .active_y(active_words[7:4]),.orientation(active_words[9:8]),
        .color_a(active_words[12:10]),.color_b(active_words[15:13]),
        .next_a(next_pairs[2:0]),.next_b(next_pairs[5:3]),.score(scores[31:0]),
        .chain(status_words[7:0]),.last_chain(status_words[15:8]),
        .game_over(status_words[26]),.busy(status_words[25]),
        .clear_mask(clear_masks[83:0]),.vga_red(solo_r),.vga_green(solo_g),
        .vga_blue(solo_b),.hsync(solo_hs),.vsync(solo_vs),.frame_tick(solo_frame));

    reg [9:0] pixel_x,pixel_y;
    reg [503:0] board_view;
    reg [167:0] clear_view;
    reg [31:0] active_view,pending_view,match_view;
    reg [11:0] next_view;
    reg [63:0] score_view;
    reg [55:0] status_view;
    reg [31:0] score_binary0,score_binary1;
    reg [15:0] pending_binary0,pending_binary1;
    reg [39:0] score_work0,score_work1,score_bcd0,score_bcd1;
    reg [19:0] pending_work0,pending_work1,pending_bcd0,pending_bcd1;
    reg [39:0] score_adjust0,score_adjust1;
    reg [19:0] pending_adjust0,pending_adjust1;
    reg [5:0] score_steps;
    reg [4:0] pending_steps;
    integer digit;
    always @* begin
        score_adjust0=score_work0; score_adjust1=score_work1;
        pending_adjust0=pending_work0; pending_adjust1=pending_work1;
        for(digit=0;digit<10;digit=digit+1) begin
            if(score_work0[digit*4 +: 4]>=5)
                score_adjust0[digit*4 +: 4]=score_work0[digit*4 +: 4]+4'd3;
            if(score_work1[digit*4 +: 4]>=5)
                score_adjust1[digit*4 +: 4]=score_work1[digit*4 +: 4]+4'd3;
        end
        for(digit=0;digit<5;digit=digit+1) begin
            if(pending_work0[digit*4 +: 4]>=5)
                pending_adjust0[digit*4 +: 4]=pending_work0[digit*4 +: 4]+4'd3;
            if(pending_work1[digit*4 +: 4]>=5)
                pending_adjust1[digit*4 +: 4]=pending_work1[digit*4 +: 4]+4'd3;
        end
    end

    always @(posedge clk) begin
        if(reset) begin
            pixel_x<=0; pixel_y<=0; frame_tick<=0;
            board_view<=0; clear_view<=0; active_view<=0; next_view<=0;
            score_view<=0; status_view<=0; pending_view<=0; match_view<=0;
            score_binary0<=0; score_binary1<=0;
            pending_binary0<=0; pending_binary1<=0;
            score_work0<=0; score_work1<=0; score_bcd0<=0; score_bcd1<=0;
            pending_work0<=0; pending_work1<=0; pending_bcd0<=0; pending_bcd1<=0;
            score_steps<=0; pending_steps<=0;
        end else begin
            frame_tick<=0;
            if(pixel_x==799) begin
                pixel_x<=0;
                pixel_y<=pixel_y==524 ? 10'd0 : pixel_y+10'd1;
            end else pixel_x<=pixel_x+10'd1;
            // Capture both players AND mode/result on one frame boundary.
            if(pixel_x==0 && pixel_y==480) begin
                frame_tick<=1;
                board_view<=boards; clear_view<=clear_masks;
                active_view<=active_words; next_view<=next_pairs;
                score_view<=scores; status_view<=status_words;
                pending_view<=pending_counts; match_view<=match_status;
                score_binary0<=scores[31:0]; score_binary1<=scores[63:32];
                pending_binary0<=pending_counts[15:0]; pending_binary1<=pending_counts[31:16];
                score_work0<=0; score_work1<=0; score_steps<=32;
                pending_work0<=0; pending_work1<=0; pending_steps<=16;
            end else begin
                if(score_steps!=0) begin
                    score_work0<={score_adjust0[38:0],score_binary0[31]};
                    score_work1<={score_adjust1[38:0],score_binary1[31]};
                    score_binary0<={score_binary0[30:0],1'b0};
                    score_binary1<={score_binary1[30:0],1'b0};
                    score_steps<=score_steps-1'b1;
                    if(score_steps==1) begin
                        score_bcd0<={score_adjust0[38:0],score_binary0[31]};
                        score_bcd1<={score_adjust1[38:0],score_binary1[31]};
                    end
                end
                if(pending_steps!=0) begin
                    pending_work0<={pending_adjust0[18:0],pending_binary0[15]};
                    pending_work1<={pending_adjust1[18:0],pending_binary1[15]};
                    pending_binary0<={pending_binary0[14:0],1'b0};
                    pending_binary1<={pending_binary1[14:0],1'b0};
                    pending_steps<=pending_steps-1'b1;
                    if(pending_steps==1) begin
                        pending_bcd0<={pending_adjust0[18:0],pending_binary0[15]};
                        pending_bcd1<={pending_adjust1[18:0],pending_binary1[15]};
                    end
                end
            end
        end
    end

    function [11:0] palette;
        input [2:0] color_id;
        begin
            case(color_id)
                1: palette=12'hF45;
                2: palette=12'h4E6;
                3: palette=12'h49F;
                4: palette=12'hFD4;
                5: palette=12'hAAA; // neutral garbage, never a fifth matching color
                default: palette=12'h122;
            endcase
        end
    endfunction
    function [11:0] puyo_pixel;
        input [2:0] color_id;
        input [4:0] sx,sy;
        input marked;
        reg body;
        begin
            body=sx>=3 && sx<=28 && sy>=3 && sy<=28 &&
                 !(sx<7 && sy<7) && !(sx>24 && sy<7) &&
                 !(sx<7 && sy>24) && !(sx>24 && sy>24);
            puyo_pixel=12'h122;
            if(color_id!=0 && body) begin
                puyo_pixel=marked ? 12'hFFF : palette(color_id);
                if(sy>=10 && sy<=17 && ((sx>=9 && sx<=12) || (sx>=19 && sx<=22)))
                    puyo_pixel=12'hFFF;
                if(sy>=13 && sy<=17 && (sx==11 || sx==21)) puyo_pixel=12'h112;
            end
        end
    endfunction

    function [34:0] glyph;
        input [7:0] ch;
        begin
            case (ch)
                "0": glyph={5'b01110,5'b10001,5'b10011,5'b10101,5'b11001,5'b10001,5'b01110};
                "1": glyph={5'b00100,5'b01100,5'b00100,5'b00100,5'b00100,5'b00100,5'b01110};
                "2": glyph={5'b01110,5'b10001,5'b00001,5'b00010,5'b00100,5'b01000,5'b11111};
                "3": glyph={5'b11110,5'b00001,5'b00001,5'b01110,5'b00001,5'b00001,5'b11110};
                "4": glyph={5'b00010,5'b00110,5'b01010,5'b10010,5'b11111,5'b00010,5'b00010};
                "5": glyph={5'b11111,5'b10000,5'b10000,5'b11110,5'b00001,5'b00001,5'b11110};
                "6": glyph={5'b01110,5'b10000,5'b10000,5'b11110,5'b10001,5'b10001,5'b01110};
                "7": glyph={5'b11111,5'b00001,5'b00010,5'b00100,5'b01000,5'b01000,5'b01000};
                "8": glyph={5'b01110,5'b10001,5'b10001,5'b01110,5'b10001,5'b10001,5'b01110};
                "9": glyph={5'b01110,5'b10001,5'b10001,5'b01111,5'b00001,5'b00001,5'b01110};
                "A": glyph={5'b01110,5'b10001,5'b10001,5'b11111,5'b10001,5'b10001,5'b10001};
                "B": glyph={5'b11110,5'b10001,5'b10001,5'b11110,5'b10001,5'b10001,5'b11110};
                "C": glyph={5'b01111,5'b10000,5'b10000,5'b10000,5'b10000,5'b10000,5'b01111};
                "D": glyph={5'b11110,5'b10001,5'b10001,5'b10001,5'b10001,5'b10001,5'b11110};
                "E": glyph={5'b11111,5'b10000,5'b10000,5'b11110,5'b10000,5'b10000,5'b11111};
                "F": glyph={5'b11111,5'b10000,5'b10000,5'b11110,5'b10000,5'b10000,5'b10000};
                "G": glyph={5'b01111,5'b10000,5'b10000,5'b10111,5'b10001,5'b10001,5'b01111};
                "H": glyph={5'b10001,5'b10001,5'b10001,5'b11111,5'b10001,5'b10001,5'b10001};
                "I": glyph={5'b01110,5'b00100,5'b00100,5'b00100,5'b00100,5'b00100,5'b01110};
                "J": glyph={5'b00111,5'b00010,5'b00010,5'b00010,5'b10010,5'b10010,5'b01100};
                "L": glyph={5'b10000,5'b10000,5'b10000,5'b10000,5'b10000,5'b10000,5'b11111};
                "M": glyph={5'b10001,5'b11011,5'b10101,5'b10101,5'b10001,5'b10001,5'b10001};
                "N": glyph={5'b10001,5'b11001,5'b10101,5'b10011,5'b10001,5'b10001,5'b10001};
                "O": glyph={5'b01110,5'b10001,5'b10001,5'b10001,5'b10001,5'b10001,5'b01110};
                "P": glyph={5'b11110,5'b10001,5'b10001,5'b11110,5'b10000,5'b10000,5'b10000};
                "R": glyph={5'b11110,5'b10001,5'b10001,5'b11110,5'b10100,5'b10010,5'b10001};
                "S": glyph={5'b01111,5'b10000,5'b10000,5'b01110,5'b00001,5'b00001,5'b11110};
                "T": glyph={5'b11111,5'b00100,5'b00100,5'b00100,5'b00100,5'b00100,5'b00100};
                "U": glyph={5'b10001,5'b10001,5'b10001,5'b10001,5'b10001,5'b10001,5'b01110};
                "V": glyph={5'b10001,5'b10001,5'b10001,5'b10001,5'b10001,5'b01010,5'b00100};
                "W": glyph={5'b10001,5'b10001,5'b10001,5'b10101,5'b10101,5'b10101,5'b01010};
                "X": glyph={5'b10001,5'b10001,5'b01010,5'b00100,5'b01010,5'b10001,5'b10001};
                "Y": glyph={5'b10001,5'b10001,5'b01010,5'b00100,5'b00100,5'b00100,5'b00100};
                ":": glyph={5'b00000,5'b00100,5'b00100,5'b00000,5'b00100,5'b00100,5'b00000};
                "/": glyph={5'b00001,5'b00001,5'b00010,5'b00100,5'b01000,5'b10000,5'b10000};
                default: glyph=0;
            endcase
        end
    endfunction

    function [11:0] bcd8;
        input [7:0] value;
        reg [19:0] work;
        integer k, d;
        begin
            work={12'b0,value};
            for(k=0;k<8;k=k+1) begin
                for(d=0;d<3;d=d+1)
                    if(work[8+d*4 +: 4]>=5)
                        work[8+d*4 +: 4]=work[8+d*4 +: 4]+4'd3;
                work=work<<1;
            end
            bcd8=work[19:8];
        end
    endfunction

    reg [11:0] rgb,versus_rgb_q,overlay_rgb_q;
    reg overlay_enable,overlay_q;
    reg hs_q,vs_q;
    reg [251:0] player_board;
    reg [83:0] player_clear;
    reg [15:0] player_active;
    reg [5:0] player_next;
    reg [27:0] player_status;
    reg [39:0] player_score_bcd;
    reg [19:0] player_pending_bcd;
    reg [2:0] tile_color;
    reg [4:0] tile_sx,tile_sy;
    reg tile_mark;
    integer panel,local_x,board_x,board_y,board_index,satellite_x,satellite_y;
    reg text_enable;
    reg [127:0] text_line;
    reg [1:0] number_kind;
    reg [11:0] short_bcd;
    integer tx,ty,text_length,text_col,font_x,font_y;
    reg [7:0] character;
    reg [34:0] character_bits;
    reg [11:0] text_color;
    always @* begin
        panel=pixel_x>=320 ? 1 : 0;
        local_x=panel ? pixel_x-320 : pixel_x;
        player_board=panel ? board_view[503:252] : board_view[251:0];
        player_clear=panel ? clear_view[167:84] : clear_view[83:0];
        player_active=panel ? active_view[31:16] : active_view[15:0];
        player_next=panel ? next_view[11:6] : next_view[5:0];
        player_status=panel ? status_view[55:28] : status_view[27:0];
        player_score_bcd=panel ? score_bcd1 : score_bcd0;
        player_pending_bcd=panel ? pending_bcd1 : pending_bcd0;
        rgb=12'h123;
        tile_color=0; tile_sx=0; tile_sy=0; tile_mark=0;
        board_x=0; board_y=0; board_index=0;
        satellite_x={1'b0,player_active[3:0]};
        satellite_y={1'b0,player_active[7:4]};
        case(player_active[9:8])
            0: satellite_y=satellite_y-1;
            1: satellite_x=satellite_x+1;
            2: satellite_y=satellite_y+1;
            3: satellite_x=satellite_x-1;
        endcase
        // Each field keeps the original 32 x 32 pixel cells: no shrink.
        if(local_x>=12 && local_x<212 && pixel_y>=44 && pixel_y<436) rgb=12'h789;
        if(local_x>=16 && local_x<208 && pixel_y>=48 && pixel_y<432) begin
            board_x=(local_x-16)>>5;
            board_y=((pixel_y-48)>>5)+2;
            board_index=board_y*6+board_x;
            tile_sx=(local_x-16)&31;
            tile_sy=(pixel_y-48)&31;
            tile_color=player_board[board_index*3 +: 3];
            tile_mark=player_clear[board_index];
            if(player_status[24] && board_x==player_active[3:0] && board_y==player_active[7:4]) begin
                tile_color=player_active[12:10]; tile_mark=0;
            end
            if(player_status[24] && board_x==satellite_x && board_y==satellite_y) begin
                tile_color=player_active[15:13]; tile_mark=0;
            end
            rgb=puyo_pixel(tile_color,tile_sx,tile_sy,tile_mark);
            if(tile_sx==0 || tile_sy==0) rgb=12'h234;
        end
        if(local_x>=232 && local_x<264 && pixel_y>=288 && pixel_y<352) begin
            tile_sx=local_x-232; tile_sy=(pixel_y-288)&31;
            tile_color=pixel_y<320 ? player_next[5:3] : player_next[2:0];
            rgb=puyo_pixel(tile_color,tile_sx,tile_sy,1'b0);
        end
        // Small sidebar text: 5 x 7 glyph in an 8 px cell; decimal digits are
        // preconverted during blanking, and positioning uses shifts only.
        text_enable=0; tx=216; ty=0; text_length=0; text_line=0;
        number_kind=0; short_bcd=0; text_color=12'hDEF;
        if(pixel_y>=28 && pixel_y<35) begin
            text_enable=1; tx=16; ty=28; text_length=2;
            text_line=panel ? "P2              " : "P1              ";
        end else if(pixel_y>=56 && pixel_y<63) begin
            text_enable=1; ty=56; text_length=player_status[25] ? 9 : 4;
            text_line=player_status[25] ? "RESOLVING       " : "PLAY            ";
        end else if(pixel_y>=88 && pixel_y<95) begin
            text_enable=1; ty=88; text_length=5; text_line="SCORE           ";
        end else if(pixel_y>=104 && pixel_y<111) begin
            text_enable=1; ty=104; text_length=10; number_kind=1;
        end else if(pixel_y>=136 && pixel_y<143) begin
            text_enable=1; ty=136; text_length=5; text_line="CHAIN           ";
        end else if(pixel_y>=152 && pixel_y<159) begin
            text_enable=1; ty=152; text_length=3; number_kind=2;
            short_bcd=bcd8(player_status[7:0]);
        end else if(pixel_y>=176 && pixel_y<183) begin
            text_enable=1; ty=176; text_length=4; text_line="LAST            ";
        end else if(pixel_y>=192 && pixel_y<199) begin
            text_enable=1; ty=192; text_length=3; number_kind=2;
            short_bcd=bcd8(player_status[15:8]);
        end else if(pixel_y>=224 && pixel_y<231) begin
            text_enable=1; ty=224; text_length=8; text_line="INCOMING        ";
        end else if(pixel_y>=240 && pixel_y<247) begin
            text_enable=1; ty=240; text_length=5; number_kind=3; text_color=12'hFD4;
        end else if(pixel_y>=272 && pixel_y<279) begin
            text_enable=1; ty=272; text_length=4; text_line="NEXT            ";
        end else if(pixel_y>=376 && pixel_y<383 && match_view[3:1]==3) begin
            text_enable=1; ty=376; text_color=12'hFD4;
            if(match_view[6:4]==3) begin text_length=4; text_line="DRAW            "; end
            else if((!panel && match_view[6:4]==1) || (panel && match_view[6:4]==2)) begin
                text_length=3; text_line="WIN             ";
            end else begin text_length=4; text_line="LOSE            "; text_color=12'hF67; end
        end else if(pixel_y>=440 && pixel_y<447) begin
            text_enable=1; tx=16; ty=440; text_length=16;
            text_line=panel ? "ARROWS MOVE/DROP" : "L/R MOVE D DROP ";
        end else if(pixel_y>=456 && pixel_y<463) begin
            text_enable=1; tx=16; ty=456; text_length=10;
            text_line=panel ? "Z/X ROTATE      " : "U/C ROTATE      ";
        end

        // Shared top strip is also overlaid on the legacy SOLO display, except
        // during PLAY where every output pixel is exactly the old renderer.
        overlay_enable=match_view[3:1]!=2 && pixel_y<24 && pixel_x<640;
        if(pixel_y<24) begin
            rgb=12'h123;
            text_enable=0;
            if(pixel_y>=8 && pixel_y<15) begin
                text_enable=1; tx=16; ty=8; text_length=16; number_kind=0;
                if(!panel) begin
                    text_line=match_view[0] ? "SW0: VERSUS     " : "SW0: SOLO       ";
                end else if(match_view[3:1]==0) begin
                    text_line="SW14 START      ";
                end else if(match_view[3:1]==1) begin
                    text_line="READY 0         "; text_length=7;
                    text_line[79:72]=8'h30+{6'd0,match_view[9:8]};
                end else if(match_view[3:1]==3) begin
                    if(match_view[6:4]==3) text_line="DRAW SW14 AGAIN ";
                    else if(match_view[6:4]==4) text_line="OVER SW14 AGAIN ";
                    else if(match_view[6:4]==1) text_line="P1 WINS SW14    ";
                    else text_line="P2 WINS SW14    ";
                end else begin
                    text_line="VERSUS          "; text_length=6;
                end
            end
        end
        text_col=0; font_x=0; font_y=0; character=0; character_bits=0;
        if(text_enable && local_x>=tx && local_x<tx+text_length*8 &&
           pixel_y>=ty && pixel_y<ty+7) begin
            text_col=(local_x-tx)>>3; font_x=(local_x-tx)&7; font_y=pixel_y-ty;
            case(number_kind)
                1: character=8'h30+player_score_bcd[(9-text_col)*4 +: 4];
                2: character=8'h30+short_bcd[(2-text_col)*4 +: 4];
                3: character=8'h30+player_pending_bcd[(4-text_col)*4 +: 4];
                default: character=text_line[(15-text_col)*8 +: 8];
            endcase
            character_bits=glyph(character);
            if(font_x<5 && character_bits[34-font_y*5-font_x]) rgb=text_color;
        end
        if(pixel_x>=640 || pixel_y>=480) begin rgb=0; overlay_enable=0; end
    end

    always @(posedge clk) begin
        if(reset) begin
            versus_rgb_q<=0; overlay_rgb_q<=0; overlay_q<=0; hs_q<=1; vs_q<=1;
        end else begin
            versus_rgb_q<=rgb; overlay_rgb_q<=rgb; overlay_q<=overlay_enable;
            hs_q<=!(pixel_x>=656 && pixel_x<752);
            vs_q<=!(pixel_y>=490 && pixel_y<492);
        end
    end
    wire [11:0] selected_rgb=match_view[0] ? versus_rgb_q :
                             overlay_q ? overlay_rgb_q : {solo_r,solo_g,solo_b};
    assign {vga_red,vga_green,vga_blue}=selected_rgb;
    assign hsync=hs_q;
    assign vsync=vs_q;
endmodule
`default_nettype wire
