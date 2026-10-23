`timescale 1ns/1ps
module tb_versus_video;
    reg clk=0,reset=1;
    always #20 clk=~clk;
    reg [503:0] boards=0;
    reg [167:0] masks=0;
    reg [31:0] active=0;
    reg [11:0] next_pairs=0;
    reg [63:0] scores=0;
    reg [55:0] statuses=0;
    reg [31:0] pending=0;
    reg [31:0] match_status=5; // versus, PLAY
    wire [3:0] r,g,b,rr,rg,rb;
    wire hs,vs,frame,rhs,rvs,rframe;
    PuyoVersusVideo dut(.clk(clk),.reset(reset),.boards(boards),.clear_masks(masks),
        .active_words(active),.next_pairs(next_pairs),.scores(scores),
        .status_words(statuses),.pending_counts(pending),.match_status(match_status),
        .vga_red(r),.vga_green(g),.vga_blue(b),.hsync(hs),.vsync(vs),.frame_tick(frame));
    PuyoVideo reference_solo(.clk(clk),.reset(reset),.board_flat(boards[251:0]),
        .active_valid(statuses[24]),.active_x(active[3:0]),.active_y(active[7:4]),
        .orientation(active[9:8]),.color_a(active[12:10]),.color_b(active[15:13]),
        .next_a(next_pairs[2:0]),.next_b(next_pairs[5:3]),.score(scores[31:0]),
        .chain(statuses[7:0]),.last_chain(statuses[15:8]),
        .game_over(statuses[26]),.busy(statuses[25]),.clear_mask(masks[83:0]),
        .vga_red(rr),.vga_green(rg),.vga_blue(rb),.hsync(rhs),.vsync(rvs),.frame_tick(rframe));
    integer n,x,y,hs_low,vs_low,visible,ticks,checks,d;
    reg [11:0] rgb,decimal_expected;
    reg [899:0] frozen;
    reg changed;
    integer ppm;

    task expect_pixel;
        input integer at_x,at_y;
        input [11:0] expected;
        begin
            if(x==at_x && y==at_y) begin
                if(rgb!==expected) $fatal(1,"versus pixel %0d,%0d got=%h expected=%h",x,y,rgb,expected);
                checks=checks+1;
            end
        end
    endtask
    task await_frame_start;
        begin
            while(!(dut.pixel_x==0 && dut.pixel_y==0)) @(negedge clk);
        end
    endtask
    task await_snapshot;
        begin
            while(!(dut.pixel_x==0 && dut.pixel_y==480)) @(negedge clk);
            @(posedge clk); #1;
            if(!frame || !rframe) $fatal(1,"frame tick mismatch");
            @(negedge clk);
            repeat(35) @(negedge clk);
        end
    endtask
    initial begin
        for(d=0;d<256;d=d+1) begin
            decimal_expected=((d/100)<<8)|(((d/10)%10)<<4)|(d%10);
            if(dut.bcd8(d[7:0])!==decimal_expected) $fatal(1,"BCD8 failed %0d",d);
        end
        boards[(12+0)*3 +:3]=1;
        boards[(12+1)*3 +:3]=5;
        boards[(12+2)*3 +:3]=4;
        masks[14]=1;
        boards[0 +:3]=3; // hidden
        boards[252+(12+0)*3 +:3]=2;
        boards[252+(12+1)*3 +:3]=5;
        boards[252 +:3]=4; // hidden
        active[15:0]={3'd4,3'd3,2'd1,4'd3,4'd2};
        active[31:16]={3'd1,3'd4,2'd3,4'd4,4'd4};
        next_pairs={3'd4,3'd3,3'd2,3'd1};
        scores={32'd1234567890,32'hffffffff};
        statuses[27:0]=(1<<27)|(1<<24)|(2<<16)|(123<<8)|12;
        statuses[55:28]=(1<<27)|(1<<24)|(2<<16)|(3<<8)|2;
        pending={16'd31,16'hffff};
        repeat(5) @(negedge clk); reset=0;
        await_snapshot();
        if(dut.score_bcd0!==40'h4294967295 || dut.score_bcd1!==40'h1234567890)
            $fatal(1,"parallel full-width score BCD failed");
        if(dut.pending_bcd0!==20'h65535 || dut.pending_bcd1!==20'h00031)
            $fatal(1,"pending BCD failed");
        await_frame_start();
        frozen={dut.board_view,dut.clear_view,dut.active_view,dut.next_view,
                dut.score_view,dut.status_view,dut.pending_view,dut.match_view};
        hs_low=0;vs_low=0;visible=0;ticks=0;checks=0;changed=0;
        ppm=0;
        if($test$plusargs("PPM")) begin
            ppm=$fopen("puyo_versus_video.ppm","w");
            if(!ppm) $fatal(1,"cannot open PPM");
            // Use numeric LF, not a string escape, so Windows VVP path
            // normalization cannot rewrite octal newline escapes in strings.
            $fwrite(ppm,"P3%c640 480%c255%c",8'd10,8'd10,8'd10);
        end
        for(n=0;n<800*525;n=n+1) begin
            x=dut.pixel_x;y=dut.pixel_y;
            if(x==0 && y==100) begin
                boards[(12+0)*3 +:3]=4;
                boards[252+(12+0)*3 +:3]=3;
                pending={16'd30,16'd8};
                scores={32'd0,32'd1};
                statuses[7:0]=9;
                next_pairs[2:0]=4;
                active[3:0]=3;
                match_status=32'h203; // versus countdown2; latch only next blank
                changed=1;
            end
            // Assert character routing before the output-register edge.
            if(x==216 && y==104 && dut.character!=="4") $fatal(1,"P1 score digits route");
            if(x==536 && y==104 && dut.character!=="1") $fatal(1,"P2 score digits route");
            if(x==216 && y==240 && dut.character!=="6") $fatal(1,"P1 incoming digits route");
            if(x==568 && y==240 && dut.character!=="1") $fatal(1,"P2 incoming digits route");
            @(posedge clk);#1;
            rgb={r,g,b};
            if(hs!==!(x>=656 && x<752) || vs!==!(y>=490 && y<492)) $fatal(1,"sync timing");
            if(frame!==rframe) $fatal(1,"in-phase solo and versus counters diverged");
            if(!hs) hs_low=hs_low+1;
            if(!vs) vs_low=vs_low+1;
            if(frame) ticks=ticks+1;
            if(x<640 && y<480) begin
                visible=visible+1;
                if(ppm) $fwrite(ppm,"%0d %0d %0d%c",r*17,g*17,b*17,8'd10);
            end else if(rgb!==0) $fatal(1,"blanking nonblack");
            if(changed && y<480 &&
               {dut.board_view,dut.clear_view,dut.active_view,dut.next_view,
                dut.score_view,dut.status_view,dut.pending_view,dut.match_view}!==frozen)
                $fatal(1,"mixed frame snapshot");
            expect_pixel(32,64,12'hF45);
            expect_pixel(64,64,12'hAAA);
            expect_pixel(96,64,12'hFFF);
            expect_pixel(352,64,12'h4E6);
            expect_pixel(384,64,12'hAAA);
            expect_pixel(96,96,12'h49F);
            expect_pixel(128,96,12'hFD4);
            expect_pixel(480,128,12'hFD4);
            expect_pixel(448,128,12'hF45);
            expect_pixel(248,304,12'h4E6);
            expect_pixel(248,336,12'hF45);
            expect_pixel(568,304,12'hFD4);
            expect_pixel(568,336,12'h49F);
            @(negedge clk);
        end
        if(ppm) $fclose(ppm);
        if(hs_low!=50400 || vs_low!=1600 || visible!=307200 || ticks!=1 || checks!=13)
            $fatal(1,"frame counts hs=%0d vs=%0d visible=%0d ticks=%0d checks=%0d",hs_low,vs_low,visible,ticks,checks);
        if(dut.board_view!==boards || dut.clear_view!==masks || dut.active_view!==active ||
           dut.next_view!==next_pairs || dut.score_view!==scores || dut.status_view!==statuses ||
           dut.pending_view!==pending || dut.match_view!==match_status)
            $fatal(1,"updated atomic frame missing");
        if(dut.pending_bcd0!==20'h00008 || dut.pending_bcd1!==20'h00030) $fatal(1,"pending updated BCD");
        // Inspect countdown's digit; each frame's mode/status is immutable.
        while(!(dut.pixel_x==384 && dut.pixel_y==8)) @(negedge clk);
        if(dut.character!=="2") $fatal(1,"countdown label expected2 got%h",dut.character);
        // Switch to solo PLAY; all visible/blanking RGB is old PuyoVideo exactly.
        match_status=4;
        await_snapshot();
        await_frame_start();
        for(n=0;n<800*525;n=n+1) begin
            @(posedge clk);#1;
            if({r,g,b,hs,vs,frame}!=={rr,rg,rb,rhs,rvs,rframe})
                $fatal(1,"solo renderer compatibility");
            @(negedge clk);
        end
        // Direct glyph/multiplexer checks at actual positions in a result frame.
        match_status=1|(3<<1)|(1<<4); // P1 wins
        await_snapshot();
        while(!(dut.pixel_x==216 && dut.pixel_y==376)) @(negedge clk);
        if(dut.character!=="W") $fatal(1,"winner label");
        while(!(dut.pixel_x==536 && dut.pixel_y==376)) @(negedge clk);
        if(dut.character!=="L") $fatal(1,"loser label");
        $display("PASS tb_versus_video: dual 32px fields, garbage/palette/active/NEXT, snapshot, 800x525 sync, full-score/pending BCD, countdown/result, exact solo frame");
        $finish;
    end
    initial begin #180000000; $fatal(1,"tb_versus_video timeout"); end
endmodule
