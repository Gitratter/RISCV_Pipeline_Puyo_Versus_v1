#include "game.h"
#include <stdint.h>
#define IO(offset) (*(volatile uint32_t *)(0x2000u + (offset)))
static PuyoGame game;

/* These are DISPLAY COPIES only. All authoritative game state is in CPU RAM. */
static void publish(const PuyoGame *g) {
    unsigned i;
    for (i = 0; i < 84; ++i) {
        IO(0x20) = i;
        IO(0x24) = (uint32_t)g->board[i] | ((uint32_t)(g->clear[i] != 0) << 8);
    }
    IO(0x28) = (uint32_t)g->active_x | ((uint32_t)g->active_y << 4)
        | ((uint32_t)g->orientation << 8) | ((uint32_t)g->color_a << 10)
        | ((uint32_t)g->color_b << 13);
    IO(0x2c) = (uint32_t)g->next_a | ((uint32_t)g->next_b << 3);
    IO(0x30) = g->score;
    IO(0x34) = (uint32_t)g->chain | ((uint32_t)g->last_chain << 8)
        | ((uint32_t)g->phase << 16) | ((uint32_t)g->active_valid << 24)
        | ((uint32_t)g->busy << 25) | ((uint32_t)g->game_over << 26) | (1u << 27);
    IO(0x38) = 1; /* Atomic display commit; VGA latches it at a frame boundary. */
}

int main(void) {
    uint32_t now = IO(0x04), last_publish;
#ifdef GAME_FIXED_TEST
    static const PuyoPair sequence[] = {{1,2},{3,4},{2,2},{4,1},{3,3},{1,4}};
    game_init(&game, 0x6d2b79f5u, sequence, 6, now);
#else
    game_init(&game, IO(0x08) ^ now ^ 0x6d2b79f5u, 0, 0, now);
#endif
#ifdef GAME_FAST_TEST
    game.config.auto_drop_ms = 120;
    game.config.soft_drop_ms = 8;
    game.config.das_ms = 40;
    game.config.arr_ms = 15;
    game.config.fall_ms = 5;
    game.config.clear_ms = 20;
#endif
    publish(&game);
    last_publish = now;
    for (;;) {
        now = IO(0x04);
        game_step(&game, now, IO(0x00));
        /* Poll faster than 1 ms; publish at most once per millisecond.
           No timing or physics is tied to the instruction execution rate. */
        if (now != last_publish) {
            publish(&game);
            last_publish = now;
            *(volatile uint32_t *)0x104u = game.score;
        }
    }
}
