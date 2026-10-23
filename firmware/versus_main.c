#include "match.h"
#include <stdint.h>
#define IO(offset) (*(volatile uint32_t *)(0x2000u + (offset)))
static PuyoMatch match;

/* Published mailboxes are display-only copies; the CPU owns both real boards. */
static void publish_player(unsigned player) {
    const PuyoGame *g=&match.game[player];
    unsigned i;
    IO(0x3c)=player;
    for(i=0;i<GAME_CELLS;++i) {
        IO(0x20)=i;
        IO(0x24)=(uint32_t)g->board[i] | ((uint32_t)(g->clear[i]!=0)<<8);
    }
    IO(0x28)=(uint32_t)g->active_x | ((uint32_t)g->active_y<<4)
        | ((uint32_t)g->orientation<<8) | ((uint32_t)g->color_a<<10)
        | ((uint32_t)g->color_b<<13);
    IO(0x2c)=(uint32_t)g->next_a | ((uint32_t)g->next_b<<3);
    IO(0x30)=g->score;
    IO(0x34)=(uint32_t)g->chain | ((uint32_t)g->last_chain<<8)
        | ((uint32_t)g->phase<<16) | ((uint32_t)g->active_valid<<24)
        | ((uint32_t)g->busy<<25) | ((uint32_t)g->game_over<<26) | (1u<<27);
    IO(0x60)=match.pending[player];
}
static void publish(void) {
    publish_player(0);publish_player(1);
    IO(0x64)=(uint32_t)match.mode_versus | ((uint32_t)match.state<<1)
        | ((uint32_t)match.result<<4) | ((uint32_t)match.countdown<<8);
    IO(0x38)=1; /* Both players and match status are made visible atomically. */
}
int main(void) {
    uint32_t now=IO(0x04),last_update;
    match_init(&match,IO(0x08)^now^0x6d2b79f5u,now);
#ifdef VERSUS_FAST_TEST
    match.countdown_ms=50;
#endif
    publish();last_update=now;
    for(;;) {
        uint32_t keys1,keys2,mode;
        now=IO(0x04);
        if(now==last_update) continue;
        last_update=now;
        keys1=IO(0x00);keys2=IO(0x18);mode=IO(0x1c)&1u;
        /* Both players receive the SAME timestamp and one input snapshot.
           No blocking animation waits: an individual busy phase never blocks P2. */
        match_step(&match,now,keys1&31u,keys2&31u,mode,(keys1>>5)&1u);
        publish();
        *(volatile uint32_t *)0x104u=(uint32_t)match.result | ((uint32_t)match.state<<4);
    }
}
