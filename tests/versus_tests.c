#include "../firmware/match.h"
#ifdef HOST_TEST
#include <stdio.h>
#endif

/* Same tests are linked into an RV32I executable: no host-only game model. */
static PuyoMatch m;
static uint32_t now;
static uint16_t pending_test[2], sent_test[2], clipped_test[2];
static uint8_t first_pairs[32];
unsigned versus_test_passed;
const unsigned versus_test_count = 32;
#define A (&m.game[0])
#define B (&m.game[1])
#define CHECK(c,n) do { if (!(c)) return (n); } while (0)
#define PASS() do { ++versus_test_passed; } while (0)

static void fresh(void)
{
    unsigned i;
    now=0;
    match_init(&m,0x12345678u,now);
    m.countdown_ms=0;
    match_start(&m,0x12345678u,1,now,0,0);
    match_step(&m,++now,0,0,1,0);
    for(i=0;i<2;++i) {
        m.game[i].config.auto_drop_ms=100000;
        m.game[i].config.fall_ms=1;
        m.game[i].config.clear_ms=2;
    }
}
static void step(uint32_t a,uint32_t b)
{ match_step(&m,++now,a,b,m.mode_versus,0); }
static void put(PuyoGame *g,unsigned x,unsigned y,unsigned c)
{ g->board[y*6+x]=(uint8_t)c; }
static unsigned count(const PuyoGame *g,unsigned color)
{
    unsigned i,n=0;
    for(i=0;i<84;++i) if(g->board[i]==color) ++n;
    return n;
}
static unsigned population(const PuyoGame *g)
{
    unsigned i,n=0;
    for(i=0;i<84;++i) if(g->board[i]) ++n;
    return n;
}
static unsigned marks(const PuyoGame *g)
{
    unsigned i,n=0;
    for(i=0;i<84;++i) if(g->clear[i]) ++n;
    return n;
}
static int run_until_active(PuyoGame *g)
{
    unsigned i;
    for(i=0;i<4000;++i) {
        if(g->phase==GAME_ACTIVE || g->game_over) return 1;
        step(0,0);
    }
    return 0;
}
static void boundary(PuyoGame *g,unsigned attack)
{
    g->active_valid=0; g->busy=1; g->phase=GAME_BOUNDARY;
    g->turn_ready=1; g->round_attack=(uint16_t)attack;
}
static void fixture_two(PuyoGame *g)
{
    unsigned y;
    for(y=10;y<14;++y) put(g,0,y,1);
    put(g,0,9,2); put(g,1,13,2); put(g,2,13,2); put(g,3,13,2);
}
static void fixture_three(PuyoGame *g)
{
    unsigned x,y;
    for(y=10;y<14;++y) put(g,0,y,1);
    put(g,0,9,2); put(g,0,8,2);
    for(x=1;x<4;++x) {put(g,x,13,2);put(g,x,12,3);}
    put(g,0,7,3);
}
static void settle(unsigned p0,unsigned p1,unsigned a0,unsigned a1)
{
    pending_test[0]=(uint16_t)p0; pending_test[1]=(uint16_t)p1;
    match_settle(pending_test,(uint16_t)a0,(uint16_t)a1,sent_test,clipped_test);
}
static int distribution(unsigned number)
{
    unsigned x,total=0,extra=0,q=number/6,r=number%6;
    fresh(); boundary(A,0); m.pending[0]=(uint16_t)number; step(0,0);
    for(x=0;x<6;++x) {
        if(A->garbage_columns[x]!=q && A->garbage_columns[x]!=q+1) return 0;
        if(A->garbage_columns[x]==q+1) ++extra;
        total+=A->garbage_columns[x];
    }
    if(total!=number || extra!=r || A->garbage_inflight!=number || m.pending[0]) return 0;
    if(!run_until_active(A) || A->game_over || count(A,5)!=number) return 0;
    for(x=0;x<6;++x) {
        unsigned y,n=0;
        for(y=0;y<14;++y) if(A->board[y*6+x]) ++n;
        if(n!=q && n!=q+1) return 0;
        for(y=14-n;y<14;++y) if(A->board[y*6+x]!=5) return 0;
    }
    return 1;
}

int versus_run_tests(void)
{
    unsigned i,x,y,n,a,b,p,q;
    uint32_t rng,saved,updates;
    uint8_t before_x,ori;
    versus_test_passed=0;

    /* 1: garbage never forms a normal component, including 5+ pieces. */
    fresh(); for(x=0;x<6;++x) put(A,x,13,5);
    game_begin_resolve(A,now); CHECK(run_until_active(A),101);
    CHECK(count(A,5)==6 && !A->last_chain && !A->score && !m.attack_total[0],102); PASS();

    /* 2: direct neighbor only; diagonal and garbage-to-garbage do not spread. */
    fresh(); for(x=0;x<4;++x)put(A,x,13,1);
    put(A,4,13,5);put(A,5,13,5);put(A,4,12,5);
    game_begin_resolve(A,now);
    for(i=0;i<20 && A->phase!=GAME_CLEAR_WAIT;++i)step(0,0);
    CHECK(A->phase==GAME_CLEAR_WAIT && A->removed==4 && marks(A)==5,201);
    CHECK(A->clear[82] && !A->clear[83] && !A->clear[76],202);
    CHECK(run_until_active(A) && count(A,5)==2 && A->score==40 && A->last_chain==1,203);
    CHECK(m.attack_total[0]==1 && m.pending[1]==1,204); PASS();

    /* 3: a shared garbage neighbor is marked only once. */
    fresh(); put(A,1,13,5);
    put(A,0,13,1);put(A,0,12,1);put(A,1,12,1);put(A,2,12,1);put(A,2,13,1);
    game_begin_resolve(A,now);
    for(i=0;i<20 && A->phase!=GAME_CLEAR_WAIT;++i)step(0,0);
    CHECK(A->removed==5 && marks(A)==6,301);
    CHECK(run_until_active(A) && population(A)==0 && A->score==50 && m.attack_total[0]==1,302);PASS();

    /* 4: two separated groups in ONE stage: total8/4, not two chains. */
    fresh();for(x=0;x<2;++x){put(A,x,12,1);put(A,x,13,1);put(A,x+4,12,2);put(A,x+4,13,2);}
    game_begin_resolve(A,now);CHECK(run_until_active(A),401);
    CHECK(m.attack_total[0]==2 && m.pending[1]==2 && A->last_chain==1 && A->score==80,402);PASS();

    /* 5: 4+4 two-chain sends only at END, attack1+2=3. */
    fresh();fixture_two(A);game_begin_resolve(A,now);n=0;
    for(i=0;i<4000 && A->phase!=GAME_ACTIVE;++i){step(0,0);if(A->last_chain==1){++n;CHECK(m.pending[1]==0,501);}}
    CHECK(n && A->phase==GAME_ACTIVE && m.attack_total[0]==3 && m.pending[1]==3 && A->score==120,502);PASS();

    /* 6: 4+5+4 three-chain has floor(5/4)=1, attack1+2+4=7. */
    fresh();fixture_three(A);game_begin_resolve(A,now);CHECK(run_until_active(A),601);
    CHECK(A->last_chain==3 && A->score==260 && m.attack_total[0]==7 && m.pending[1]==7,602);
    CHECK(game_attack_factor(0)==0 && game_attack_factor(4)==8 && game_attack_factor(5)==12 && game_attack_factor(6)==18 && game_attack_factor(7)==18 && game_attack_factor(21)==18,603);PASS();

    /* 7: P2 remains controllable while P1 resolves and waits for animation. */
    fresh();fixture_three(A);game_begin_resolve(A,now);before_x=B->active_x;
    step(0,BUTTON_LEFT|BUTTON_ROTATE_RIGHT);
    CHECK(B->active_x==before_x-1 && B->orientation==1 && A->busy,701);
    step(0,0); B->config.soft_drop_ms=1;before_x=B->active_y;
    step(0,BUTTON_DOWN);CHECK(B->active_y==before_x+1 && A->busy,702);PASS();

    /* 8: both independent generators agree at ordinal n despite timing. */
    fresh();for(i=0;i<16;++i){first_pairs[i*2]=A->color_a;first_pairs[i*2+1]=A->color_b;boundary(A,0);step(0,0);step(0,0);}
    CHECK(B->pair_number==1 && A->pair_number==17,801);
    for(i=0;i<16;++i){CHECK(B->color_a==first_pairs[i*2]&&B->color_b==first_pairs[i*2+1],802);boundary(B,0);step(0,0);step(0,0);}
    CHECK(A->rng==B->rng && A->next_a==B->next_a && A->next_b==B->next_b,803);PASS();

    /* 9: garbage column RNG consumes no normal pair RNG samples. */
    fresh();rng=A->rng;saved=A->garbage_rng;boundary(A,0);m.pending[0]=8;step(0,0);
    CHECK(A->rng==rng && A->garbage_rng!=saved,901);CHECK(run_until_active(A),902);
    boundary(B,0);step(0,0);step(0,0);CHECK(A->rng==B->rng&&A->color_a==B->color_a&&A->next_b==B->next_b,903);PASS();

    /* 10..12: insufficient/exact/excess self cancellation. */
    settle(10,0,6,0);CHECK(pending_test[0]==4&&pending_test[1]==0&&sent_test[0]==0,1001);PASS();
    settle(10,0,10,0);CHECK(pending_test[0]==0&&pending_test[1]==0&&sent_test[0]==0,1101);PASS();
    settle(10,0,14,0);CHECK(pending_test[0]==0&&pending_test[1]==4&&sent_test[0]==4,1201);PASS();

    /* 13: simultaneous outgoing cross cancellation, mirror invariance. */
    settle(0,0,10,6);CHECK(pending_test[0]==0&&pending_test[1]==4&&sent_test[0]==4,1301);
    settle(0,0,6,10);CHECK(pending_test[0]==4&&pending_test[1]==0&&sent_test[1]==4,1302);
    for(p=0;p<4;++p)for(q=0;q<4;++q)for(a=0;a<5;++a)for(b=0;b<5;++b){
        uint16_t p0,p1,s0,s1;settle(p,q,a,b);p0=pending_test[0];p1=pending_test[1];s0=sent_test[0];s1=sent_test[1];
        settle(q,p,b,a);CHECK(pending_test[0]==p1&&pending_test[1]==p0&&sent_test[0]==s1&&sent_test[1]==s0,1303);
    }PASS();

    /* 14: simultaneous self-cancellation must use OLD pending for both. */
    settle(10,5,14,8);CHECK(pending_test[0]==0&&pending_test[1]==1&&sent_test[0]==1&&sent_test[1]==0,1401);
    settle(10,20,6,25);CHECK(pending_test[0]==9&&pending_test[1]==0&&sent_test[1]==5,1402);PASS();

    /* 15: unsigned saturation, clipping is accounted for rather than wrap. */
    settle(0,65534,378,0);CHECK(pending_test[1]==65535&&clipped_test[1]==377&&sent_test[0]==378,1501);
    settle(65535,65535,65535,65535);CHECK(!pending_test[0]&&!pending_test[1],1502);PASS();

    /* 16: held completion tokens are consumed once; same-ms call idempotent. */
    fresh();boundary(A,10);step(0,0);CHECK(m.pending[1]==10&&m.sent_total[0]==10&&!A->turn_ready,1601);
    updates=m.updates;for(i=0;i<20;++i)match_step(&m,now,0,0,1,0);
    CHECK(m.updates==updates&&m.pending[1]==10,1602);
    for(i=0;i<20;++i) { step(0,0); }
    CHECK(m.pending[1]==10&&m.sent_total[0]==10,1603);PASS();

    /* 17..20: exact distribution for remainder, full row, 8, limit30. */
    CHECK(distribution(5),1701);PASS();
    CHECK(distribution(6),1801);PASS();
    CHECK(distribution(8),1901);PASS();
    CHECK(distribution(30),2001);PASS();

    /* 21:31 reserves30 once and allows a normal pair before remaining1. */
    fresh();boundary(A,0);m.pending[0]=31;step(0,0);
    CHECK(m.pending[0]==1&&A->garbage_inflight==30&&m.drop_total[0]==30,2101);
    CHECK(run_until_active(A)&&count(A,5)==30&&m.pending[0]==1,2102);
    for(i=0;i<20;++i) { step(0,0); }
    CHECK(count(A,5)==30&&m.pending[0]==1&&A->active_valid,2103);
    boundary(A,0);step(0,0);CHECK(m.pending[0]==0&&A->garbage_inflight==1&&m.drop_total[0]==31,2104);PASS();

    /* 22: cancellation/receiving is before same-update drop reservation. */
    fresh();m.pending[0]=10;boundary(A,6);boundary(B,9);step(0,0);
    CHECK(A->garbage_inflight==13&&B->garbage_inflight==0&&m.pending[0]==0&&m.pending[1]==0,2201);
    CHECK(m.attack_total[0]==6&&m.attack_total[1]==9&&m.drop_total[0]==13,2202);PASS();

    /* 23: receiving during fall goes to NEXT turn, not current in-flight. */
    fresh();m.pending[0]=8;boundary(A,0);step(0,0);boundary(B,5);step(0,0);
    CHECK(m.pending[0]==5&&m.drop_total[0]==8&&A->busy,2301);
    CHECK(run_until_active(A)&&count(A,5)==8&&m.pending[0]==5,2302);
    CHECK(m.received_total[0]==5&&m.sent_total[1]==5,2303);PASS();

    /* 24: insertion from top cannot fill holes below a blocked top. */
    fresh();put(A,0,0,1);boundary(A,0);m.pending[0]=6;step(0,0);step(0,0);
    CHECK(A->garbage_overflow&&A->board[0]==1&&A->garbage_columns[0]==1,2401);
    CHECK(run_until_active(A)&&A->game_over&&m.result==RESULT_P2_WIN,2402);
    CHECK(count(A,1)==1&&count(A,5)==5,2403);PASS();

    /* 25: normal spawn loss, DRAW for same-update defeat, frozen result. */
    fresh();put(A,2,1,1);put(B,2,0,2);boundary(A,0);boundary(B,0);step(0,0);step(0,0);
    CHECK(m.state==MATCH_RESULT&&m.result==RESULT_DRAW,2501);
    saved=now;updates=m.updates;boundary(A,25);m.pending[1]=7;
    for(i=0;i<20;++i)step(31,31);
    CHECK(m.updates==updates&&m.pending[1]==7&&m.result==RESULT_DRAW&&now>saved,2502);PASS();

    /* 26: earlier defeat wins immediately, later potential loss not a draw. */
    fresh();put(A,2,1,1);boundary(A,0);step(0,0);step(0,0);
    CHECK(m.result==RESULT_P2_WIN,2601);B->game_over=1;step(0,0);CHECK(m.result==RESULT_P2_WIN,2602);PASS();

    /* 27: countdown, symmetric start, per-key release gating and new seed. */
    fresh();m.state=MATCH_RESULT;m.result=RESULT_P2_WIN;saved=m.match_seed;
    m.countdown_ms=3;match_step(&m,++now,16,8,1,1);
    CHECK(m.state==MATCH_COUNTDOWN&&m.match_seed!=saved&&m.pending[0]==0&&m.drop_total[0]==0,2701);
    match_step(&m,++now,16,8,1,1);match_step(&m,++now,16,8,1,1);match_step(&m,++now,16,8,1,1);
    CHECK(m.state==MATCH_PLAY&&A->active_y==1&&B->active_y==1,2702);
    step(16,8);CHECK(A->orientation==0&&B->orientation==0,2703);
    step(0,0);step(16,8);CHECK(A->orientation==1&&B->orientation==3,2704);PASS();

    /* 28: independent held keys, no rotate repeats, equal DAS/ARR. */
    fresh();step(BUTTON_LEFT|BUTTON_ROTATE_RIGHT,BUTTON_LEFT|BUTTON_ROTATE_RIGHT);
    CHECK(A->active_x==1&&B->active_x==1&&A->orientation==1&&B->orientation==1,2801);
    for(i=0;i<179;++i) { step(17,17); }
    CHECK(A->active_x==1&&B->active_x==1,2802);
    step(17,17);CHECK(A->active_x==0&&B->active_x==0&&A->orientation==1&&B->orientation==1,2803);
    step(3,3);CHECK(A->active_x==0&&B->active_x==0,2804);
    step(0,0);step(2,2);CHECK(A->active_x==1&&B->active_x==1,2805);PASS();

    /* 29: SOLO ignores opponent, no garbage; active SW14 restarts safely. */
    fresh();m.mode_versus=0;B->game_over=1;boundary(A,10);step(0,31);step(0,31);
    CHECK(m.state==MATCH_PLAY&&!m.pending[1]&&m.sent_total[0]==0&&A->active_valid,2901);
    A->score=55;m.previous_start=0;match_step(&m,++now,16,0,0,1);
    CHECK(m.state==MATCH_COUNTDOWN&&A->score==0&&m.result==RESULT_NONE,2902);PASS();

    /* 30: versus start ignored midmatch; shared timing wrap and restartreset. */
    fresh();saved=m.match_seed;match_step(&m,++now,0,0,1,1);
    CHECK(m.state==MATCH_PLAY&&m.match_seed==saved,3001);
    now=0xfffffff0u; m.last_update_ms=now-1; A->drop_started_ms=B->drop_started_ms=now;
    A->config.auto_drop_ms=B->config.auto_drop_ms=20;
    match_step(&m,now,0,0,1,0);now+=20;match_step(&m,now,0,0,1,0);
    CHECK(A->active_y==2&&B->active_y==2,3002);
    m.pending[0]=25;m.game[0].clear[2]=1;m.game[0].garbage_inflight=8;
    match_start(&m,9,1,++now,0,0);
    CHECK(!m.pending[0]&&!m.game[0].clear[2]&&!m.game[0].garbage_inflight&&!m.attack_total[0]&&!m.updates,3003);
    for(i=0;i<2;++i)for(y=0;y<14;++y)for(x=0;x<6;++x)CHECK(m.game[i].board[y*6+x]==0,3004);
    ori=A->orientation;CHECK(ori==0&&A->color_a==B->color_a&&A->next_b==B->next_b,3005);PASS();
    /* 31: different loss mechanisms in the SAME logical update are DRAW.
       Spawn is a player step, never an extra terminal check inside settlement. */
    fresh();put(A,2,1,1);A->active_valid=0;A->phase=GAME_SPAWN;
    B->active_valid=0;B->phase=GAME_GARBAGE_FALL;B->garbage_overflow=1;
    boundary(B,20);B->phase=GAME_GARBAGE_FALL;B->turn_ready=0;
    step(0,0);CHECK(m.result==RESULT_DRAW && !m.sent_total[1],3101);PASS();

    /* 32: no overwrite of packed existing columns; garbage rests ABOVE them. */
    fresh();for(y=10;y<14;++y)put(A,1,y,2+(y&1));
    boundary(A,0);m.pending[0]=6;step(0,0);CHECK(run_until_active(A),3201);
    CHECK(A->board[9*6+1]==5 && population(A)==10,3202);
    for(y=10;y<14;++y) { CHECK(A->board[y*6+1]==2+(y&1),3203); }
    PASS();
    return versus_test_passed==versus_test_count ? 0 : 99999;
}

#ifdef HOST_TEST
int main(void)
{
    int result=versus_run_tests();
    if(result)printf("FAIL versus case/check %d (%u completed)\n",result,versus_test_passed);
    else printf("PASS: %u/%u versus scenarios (same C used for RV32I)\n",versus_test_passed,versus_test_count);
    return result ? 1 : 0;
}
#endif
