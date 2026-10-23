#include "../firmware/game.h"
#ifdef HOST_TEST
#include <stdio.h>
#endif

/* The SAME suite runs natively and as RV32I firmware. No assert, malloc,
   stdio, recursion, division, or system calls are used by game_run_tests. */
static struct { uint32_t before; PuyoGame game; uint32_t after; } box;
static uint8_t saved[GAME_CELLS];
static uint32_t clock_ms;
static const PuyoPair sequence[] = {{1,1},{2,4},{3,2},{4,4},{1,3}};
unsigned game_test_passed;
const unsigned game_test_count = 26;
#define G (&box.game)
#define CHECK(cond, id) do { if (!(cond)) return (id); } while (0)
#define PASS() do { ++game_test_passed; } while (0)

static void fresh(void)
{
    box.before = 0x13579bdfu; box.after = 0x2468ace0u;
    clock_ms = 0;
    game_init(G, 0x12345678u, sequence, 5, clock_ms);
    G->config.auto_drop_ms = 10000;
    G->config.fall_ms = 1; G->config.clear_ms = 2;
}
static void put(unsigned x, unsigned y, unsigned c) { G->board[y*6+x]=(uint8_t)c; }
static unsigned population(void)
{
    unsigned i,n=0;
    for(i=0;i<GAME_CELLS;++i) if(G->board[i]) ++n;
    return n;
}
static unsigned marked(void)
{
    unsigned i,n=0;
    for(i=0;i<GAME_CELLS;++i) if(G->clear[i]) ++n;
    return n;
}
static void save_board(void)
{
    unsigned i;for(i=0;i<GAME_CELLS;++i) saved[i]=G->board[i];
}
static int board_same(void)
{
    unsigned i;for(i=0;i<GAME_CELLS;++i) if(saved[i]!=G->board[i]) return 0;
    return 1;
}
static int sane(void)
{
    unsigned i;
    if(box.before!=0x13579bdfu || box.after!=0x2468ace0u) return 0;
    if(G->queue_head>GAME_CELLS || G->queue_tail>GAME_CELLS) return 0;
    for(i=0;i<GAME_CELLS;++i) if(G->board[i]>4 || G->clear[i]>1) return 0;
    return !G->active_valid || game_pair_fits(G,G->active_x,G->active_y,G->orientation);
}
static int finish(uint32_t buttons)
{
    unsigned i;
    for(i=0;i<2000;++i) {
        if(G->phase==GAME_ACTIVE || G->game_over) return sane();
        game_step(G,++clock_ms,buttons);
        if(!sane()) return 0;
    }
    return 0;
}
static int resolve(void) { game_begin_resolve(G,clock_ms);return finish(0); }
static void active(unsigned x,unsigned y,unsigned dir,unsigned a,unsigned b)
{
    G->active_x=(uint8_t)x;G->active_y=(uint8_t)y;G->orientation=(uint8_t)dir;
    G->color_a=(uint8_t)a;G->color_b=(uint8_t)b;G->active_valid=1;
    G->phase=GAME_ACTIVE;G->busy=G->game_over=0;
    G->drop_started_ms=clock_ms;
    G->previous_buttons=0;G->held_direction=0;G->repeating=0;
}
static void press(uint32_t buttons)
{
    game_step(G,++clock_ms,0);
    game_step(G,++clock_ms,buttons);
}
static void fixture_two(void)
{
    unsigned y;for(y=10;y<14;++y)put(0,y,1);
    put(0,9,2);put(1,13,2);put(2,13,2);put(3,13,2);
}
static void fixture_three(void)
{
    unsigned x,y;for(y=10;y<14;++y)put(0,y,1);
    put(0,9,2);put(0,8,2);
    for(x=1;x<4;++x){put(x,13,2);put(x,12,3);}
    put(0,7,3);
}

int game_run_tests(void)
{
    unsigned x,y,i,stages;
    uint32_t old_rng, old_score, started;
    uint8_t na,nb;
    game_test_passed=0;

    fresh();for(x=0;x<3;++x)put(x,13,1);
    CHECK(resolve(),101);CHECK(population()==3&&G->score==0&&G->last_chain==0,102);PASS();

    fresh();for(x=0;x<4;++x)put(x,13,1);
    CHECK(resolve(),201);CHECK(population()==0&&G->score==40&&G->last_chain==1,202);PASS();

    fresh();for(x=0;x<5;++x)put(x,13,2);
    CHECK(resolve(),301);CHECK(population()==0&&G->score==50&&G->last_chain==1,302);PASS();

    fresh();put(0,13,1);put(0,12,1);put(0,11,1);put(1,13,1);
    CHECK(resolve()&&population()==0&&G->score==40,401);PASS();

    fresh();put(0,13,1);put(0,12,1);put(1,13,1);put(1,12,1);
    CHECK(resolve()&&population()==0&&G->score==40,501);PASS();

    fresh();put(0,13,3);put(1,13,3);put(2,13,3);put(1,12,3);
    CHECK(resolve()&&population()==0&&G->score==40,601);PASS();

    fresh();put(0,13,1);put(1,12,1);put(2,11,1);put(3,10,1);
    put(1,13,2);put(2,13,3);put(2,12,2);put(3,13,4);put(3,12,3);put(3,11,2);
    save_board();CHECK(resolve()&&board_same()&&G->score==0&&G->last_chain==0,701);PASS();

    fresh();for(x=0;x<2;++x){put(x,12,1);put(x,13,1);put(x+4,12,2);put(x+4,13,2);}
    save_board();game_begin_resolve(G,clock_ms);
    for(i=0;i<20&&G->phase!=GAME_CLEAR_WAIT;++i){game_step(G,++clock_ms,0);CHECK(board_same(),801);}
    CHECK(G->phase==GAME_CLEAR_WAIT&&marked()==8,802);
    CHECK(finish(0)&&population()==0&&G->score==80&&G->last_chain==1,803);PASS();

    fresh();put(0,13,3);put(0,12,4);active(0,11,1,1,2);
    clock_ms=40;game_step(G,clock_ms,BUTTON_DOWN);
    CHECK(!G->active_valid&&G->busy,901);CHECK(finish(0),902);
    CHECK(G->board[11*6]==1&&G->board[12*6]==4&&G->board[13*6]==3&&G->board[13*6+1]==2&&population()==4,903);PASS();

    fresh();fixture_two();game_begin_resolve(G,clock_ms);stages=0;
    for(i=0;i<2000&&G->phase!=GAME_ACTIVE;++i){
        old_score=G->score;game_step(G,++clock_ms,0);
        if(G->score!=old_score){
            CHECK(G->score-old_score==(stages==0?40u:80u),1002);
            ++stages;
        }
    }
    CHECK(i<2000&&population()==0&&G->score==120&&G->last_chain==2&&stages==2&&G->chain==0,1001);PASS();

    fresh();fixture_three();game_begin_resolve(G,clock_ms);stages=0;
    for(i=0;i<2000&&G->phase!=GAME_ACTIVE;++i){
        old_score=G->score;game_step(G,++clock_ms,0);
        if(G->score!=old_score){
            CHECK(G->score-old_score==(stages==0?40u:stages==1?100u:120u),1102);
            ++stages;
        }
    }
    CHECK(i<2000&&population()==0&&G->score==260&&G->last_chain==3&&stages==3&&G->chain==0,1101);PASS();

    fresh();active(0,1,0,1,2);press(BUTTON_LEFT);CHECK(G->active_x==0,1201);
    press(BUTTON_ROTATE_LEFT);CHECK(G->orientation==0&&G->active_x==0,1202);
    press(BUTTON_LEFT|BUTTON_RIGHT);CHECK(G->active_x==0,1203);
    active(5,1,0,1,2);press(BUTTON_RIGHT);press(BUTTON_ROTATE_RIGHT);
    CHECK(G->active_x==5&&G->orientation==0,1204);
    active(2,0,1,1,2);press(BUTTON_ROTATE_LEFT);CHECK(G->orientation==1&&G->active_y==0,1205);
    active(2,13,1,1,2);press(BUTTON_ROTATE_RIGHT);CHECK(G->orientation==1&&G->active_y==13,1206);
    CHECK(population()==0&&sane()&&!game_pair_fits(G,-1,0,0)&&!game_pair_fits(G,6,0,0),1207);PASS();

    fresh();put(3,5,4);active(2,5,0,1,2);save_board();press(BUTTON_ROTATE_RIGHT);
    CHECK(G->orientation==0,1301);press(BUTTON_RIGHT);
    CHECK(G->active_x==2&&board_same()&&sane(),1302);PASS();

    fresh();fixture_three();game_begin_resolve(G,clock_ms);
    CHECK(finish(BUTTON_LEFT|BUTTON_RIGHT|BUTTON_DOWN|BUTTON_ROTATE_LEFT|BUTTON_ROTATE_RIGHT),1401);
    CHECK(population()==0&&G->score==260&&G->last_chain==3&&G->active_x==2&&G->orientation==0,1402);
    game_step(G,++clock_ms,BUTTON_LEFT|BUTTON_RIGHT|BUTTON_ROTATE_LEFT|BUTTON_ROTATE_RIGHT);
    CHECK(G->active_x==2&&G->orientation==0,1403);PASS();

    fresh();CHECK(G->color_a==1&&G->color_b==1&&G->next_a==2&&G->next_b==4,1501);
    na=G->next_a;nb=G->next_b;active(5,13,0,1,2);
    clock_ms=40;game_step(G,clock_ms,BUTTON_DOWN);CHECK(finish(0),1502);
    CHECK(G->color_a==na&&G->color_b==nb&&G->next_a==3&&G->next_b==2,1503);PASS();

    fresh();for(y=0;y<14;++y)put(2,y,(y&3u)+1u);
    game_begin_resolve(G,clock_ms);CHECK(!G->game_over,1601);
    CHECK(finish(0)&&G->game_over&&!G->active_valid&&population()==14,1602);
    press(BUTTON_RESTART);
    CHECK(!G->game_over&&G->active_valid&&population()==0&&G->score==0&&G->chain==0&&G->last_chain==0,1603);PASS();

    fresh();for(y=0;y<4;++y)put(2,y,1);
    for(y=4;y<14;++y)put(2,y,(y&1u)?2:3);
    CHECK(resolve()&&G->last_chain==1&&G->score==40&&population()==10&&!G->game_over,1701);PASS();

    fresh();fixture_three();game_begin_resolve(G,clock_ms);
    for(i=0;i<100&&G->phase!=GAME_CLEAR_WAIT;++i)game_step(G,++clock_ms,0);
    CHECK(G->phase==GAME_CLEAR_WAIT&&G->queue_tail!=0&&marked()!=0,1801);
    press(BUTTON_RESTART);
    CHECK(population()==0&&marked()==0&&G->score==0&&G->chain==0&&G->last_chain==0&&G->queue_head==0&&G->queue_tail==0,1802);
    for(i=0;i<GAME_CELLS;++i)CHECK(G->visited[i]==0&&G->queue[i]==0&&G->queue_x[i]==0,1803);
    old_rng=G->rng;game_step(G,++clock_ms,BUTTON_RESTART);
    CHECK(G->rng==old_rng&&G->active_valid,1804);PASS();

    fresh();for(i=0;i<GAME_CELLS;++i)G->board[i]=4;
    CHECK(resolve()&&population()==0&&G->score==840&&G->last_chain==1&&sane(),1901);PASS();

    fresh();put(0,2,1);put(0,5,2);put(0,8,3);
    CHECK(resolve()&&G->board[11*6]==1&&G->board[12*6]==2&&G->board[13*6]==3&&population()==3,2001);PASS();

    fresh();active(2,5,0,1,2);game_step(G,9999,0);CHECK(G->active_y==5,2101);
    game_step(G,10000,0);CHECK(G->active_y==6,2102);
    game_step(G,10039,BUTTON_DOWN);CHECK(G->active_y==6,2103);
    game_step(G,10040,BUTTON_DOWN);CHECK(G->active_y==7,2104);
    game_step(G,10040,BUTTON_DOWN);CHECK(G->active_y==7,2105);PASS();

    fresh();G->config.clear_ms=180;for(x=0;x<4;++x)put(x,13,1);save_board();
    game_begin_resolve(G,clock_ms);game_step(G,clock_ms,0);game_step(G,clock_ms,0);
    CHECK(G->phase==GAME_CLEAR_WAIT&&marked()==4,2201);started=clock_ms;
    game_step(G,started+179,0);CHECK(board_same()&&G->score==0,2202);
    game_step(G,started+180,0);CHECK(population()==0&&G->score==40&&marked()==0,2203);PASS();

    fresh();active(3,5,0,1,2);game_step(G,0,BUTTON_LEFT);CHECK(G->active_x==2,2301);
    game_step(G,179,BUTTON_LEFT);CHECK(G->active_x==2,2302);
    game_step(G,180,BUTTON_LEFT);CHECK(G->active_x==1,2303);
    game_step(G,239,BUTTON_LEFT);CHECK(G->active_x==1,2304);
    game_step(G,240,BUTTON_LEFT);CHECK(G->active_x==0,2305);
    game_step(G,241,BUTTON_LEFT|BUTTON_RIGHT);CHECK(G->active_x==0,2306);
    game_step(G,242,BUTTON_RIGHT);CHECK(G->active_x==1,2307);PASS();

    fresh();active(2,5,0,1,2);game_step(G,0,BUTTON_ROTATE_RIGHT);CHECK(G->orientation==1,2401);
    game_step(G,100,BUTTON_ROTATE_RIGHT);CHECK(G->orientation==1,2402);
    game_step(G,101,0);game_step(G,102,BUTTON_ROTATE_RIGHT);CHECK(G->orientation==2,2403);
    game_step(G,103,BUTTON_ROTATE_LEFT|BUTTON_ROTATE_RIGHT);CHECK(G->orientation==2,2404);
    game_step(G,104,0);game_step(G,105,BUTTON_ROTATE_LEFT);CHECK(G->orientation==1,2405);PASS();

    fresh();game_init(G,0x12345678u,0,0,0);old_rng=G->rng;
    game_restart(G,1234);CHECK(G->rng!=old_rng&&G->score==0,2501);
    old_rng=G->rng;game_restart(G,1234);CHECK(G->rng!=old_rng,2502);
    CHECK(G->color_a>=1&&G->color_a<=4&&G->color_b>=1&&G->color_b<=4,2503);
    game_step(G,1235,BUTTON_RESTART);old_rng=G->rng;
    game_step(G,1236,BUTTON_RESTART);CHECK(G->rng==old_rng,2505);
    game_step(G,1237,0);game_step(G,1238,BUTTON_RESTART);CHECK(G->rng!=old_rng,2506);
    game_init(G,1,sequence,5,10);game_restart(G,20000);
    CHECK(G->color_a==1&&G->color_b==1&&G->next_a==2&&G->next_b==4,2504);PASS();

    fresh();game_init(G,7,sequence,5,0xffffff00u);started=0xffffff00u;
    game_step(G,started+699u,0);CHECK(G->active_y==1,2601);
    game_step(G,started+700u,0);CHECK(G->active_y==2&&sane(),2602);
    CHECK(G->config.auto_drop_ms==700&&G->config.soft_drop_ms==40&&G->config.das_ms==180&&G->config.arr_ms==60&&G->config.fall_ms==40&&G->config.clear_ms==180,2603);PASS();

    return game_test_passed==game_test_count ? 0 : 9999;
}

#ifdef HOST_TEST
int main(void)
{
    int result=game_run_tests();
    if(result) printf("FAIL: game case/check %d (%u cases completed)\n",result,game_test_passed);
    else printf("PASS: %u/%u software game scenarios (same C used for RV32I)\n",game_test_passed,game_test_count);
    return result ? 1 : 0;
}
#endif
