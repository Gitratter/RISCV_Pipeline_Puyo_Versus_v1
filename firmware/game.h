#ifndef PUYO_GAME_H
#define PUYO_GAME_H
#include <stdint.h>

enum { GAME_WIDTH=6, GAME_HEIGHT=14, GAME_CELLS=84, GAME_GARBAGE=5 };
enum { BUTTON_LEFT=1u, BUTTON_RIGHT=2u, BUTTON_DOWN=4u,
       BUTTON_ROTATE_LEFT=8u, BUTTON_ROTATE_RIGHT=16u, BUTTON_RESTART=32u };
enum { GAME_SPAWN=1, GAME_ACTIVE=2, GAME_GRAVITY=4,
       GAME_FALL_WAIT=6, GAME_SCAN=7, GAME_CLEAR_WAIT=12, GAME_OVER=14,
       GAME_BOUNDARY=15, GAME_GARBAGE_INSERT=16, GAME_GARBAGE_FALL=17,
       GAME_GARBAGE_WAIT=18 };

typedef struct { uint8_t a, b; } PuyoPair;
typedef struct {
    uint32_t auto_drop_ms, soft_drop_ms, das_ms, arr_ms, fall_ms, clear_ms;
} PuyoTiming;

typedef struct {
    /* y=0,1 hidden; y=2..13 displayed; index=y*6+x. 0 empty,1..4 colors,5 garbage. */
    uint8_t board[GAME_CELLS], clear[GAME_CELLS];
    uint8_t active_valid, active_x, active_y, orientation;
    uint8_t color_a, color_b, next_a, next_b;
    uint8_t chain, last_chain, busy, game_over, phase;
    uint32_t score;
    PuyoTiming config;

    /* Bounded, persistent search workspace; no heap, recursion or large stack. */
    uint8_t visited[GAME_CELLS], queue[GAME_CELLS], queue_x[GAME_CELLS];
    uint8_t queue_head, queue_tail, removed;
    uint32_t rng, drop_started_ms, phase_started_ms;
    uint32_t held_started_ms, repeat_started_ms, previous_buttons;
    int8_t held_direction;
    uint8_t repeating;
    const PuyoPair *fixed_pairs;
    uint16_t fixed_count, fixed_index;
    /* Match extension. managed=0 retains the original standalone/solo API. */
    uint8_t managed, turn_ready, garbage_overflow;
    uint8_t garbage_columns[GAME_WIDTH], garbage_inflight;
    uint16_t round_attack;
    uint32_t garbage_rng, pair_number;
} PuyoGame;

void game_init(PuyoGame *g, uint32_t seed, const PuyoPair *fixed_pairs,
               uint16_t fixed_count, uint32_t now_ms);
void game_step(PuyoGame *g, uint32_t now_ms, uint32_t stable_buttons);
void game_restart(PuyoGame *g, uint32_t now_ms);
void game_begin_resolve(PuyoGame *g, uint32_t now_ms);
int game_pair_fits(const PuyoGame *g, int x, int y, unsigned orientation);
/* Consume one boundary token AFTER central cancellation. Maximum grant=30. */
void game_finish_turn(PuyoGame *g, unsigned garbage_count, uint32_t now_ms);
unsigned game_attack_factor(unsigned chain);
#endif
