#ifndef PUYO_MATCH_H
#define PUYO_MATCH_H
#include "game.h"

enum { MATCH_MENU=0, MATCH_COUNTDOWN=1, MATCH_PLAY=2, MATCH_RESULT=3 };
enum { RESULT_NONE=0, RESULT_P1_WIN=1, RESULT_P2_WIN=2, RESULT_DRAW=3,
       RESULT_SOLO_OVER=4 };
enum { MATCH_PENDING_MAX=65535, MATCH_DROP_MAX=30 };

typedef struct {
    PuyoGame game[2];
    uint16_t pending[2];
    uint8_t mode_versus, state, result, countdown;
    uint8_t previous_start, blocked_keys[2];
    uint32_t session_rng, match_seed, countdown_ms, countdown_started_ms;
    uint32_t last_update_ms, updates;
    /* Saturating session diagnostics, reset at each match. */
    uint32_t attack_total[2], sent_total[2], received_total[2];
    uint32_t cancelled_total[2], drop_total[2], overflow_total[2];
} PuyoMatch;

void match_init(PuyoMatch *m, uint32_t seed, uint32_t now_ms);
void match_step(PuyoMatch *m, uint32_t now_ms, uint32_t keys_p1,
                uint32_t keys_p2, unsigned mode_versus, unsigned start_button);
/* Explicit seed for reproducible tests. Starts a fresh countdown. Normal UI
   uses match_step, which advances session RNG before calling this function. */
void match_start(PuyoMatch *m, uint32_t seed, unsigned mode_versus,
                 uint32_t now_ms, uint32_t keys_p1, uint32_t keys_p2);
/* Pure central settlement, also used by deterministic arbitration tests.
   p[] are previous pending; outgoing self-cancels first, then cross-cancels.
   sent[] is the final transmitted amount; clipped[] records saturation. */
void match_settle(uint16_t p[2], uint16_t a0, uint16_t a1,
                  uint16_t sent[2], uint16_t clipped[2]);
#endif
