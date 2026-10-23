#include "match.h"

static uint32_t next_random(uint32_t r)
{
    if (!r) r = 0x6d2b79f5u;
    r ^= r << 13; r ^= r >> 17; r ^= r << 5;
    return r;
}
static uint32_t saturating_add(uint32_t a, uint32_t b)
{ return a > 0xffffffffu-b ? 0xffffffffu : a+b; }

void match_settle(uint16_t p[2], uint16_t a0, uint16_t a1,
                  uint16_t sent[2], uint16_t clipped[2])
{
    uint32_t attack[2], left[2];
    unsigned i;
    attack[0] = a0; attack[1] = a1;
    for (i=0; i<2; ++i) {
        if (attack[i] >= p[i]) {
            attack[i] -= p[i]; left[i] = 0;
        } else { left[i] = p[i]-attack[i]; attack[i] = 0; }
    }
    /* Both use their PRE-update pending values; no P1-first advantage. */
    if (attack[0] >= attack[1]) {
        attack[0] -= attack[1]; attack[1] = 0;
    } else { attack[1] -= attack[0]; attack[0] = 0; }
    for (i=0; i<2; ++i) {
        uint32_t combined = left[i]+attack[i^1u];
        sent[i] = (uint16_t)attack[i];
        clipped[i] = (uint16_t)(combined > MATCH_PENDING_MAX ? combined-MATCH_PENDING_MAX : 0);
        p[i] = (uint16_t)(combined > MATCH_PENDING_MAX ? MATCH_PENDING_MAX : combined);
    }
}

void match_start(PuyoMatch *m, uint32_t seed, unsigned mode_versus,
                 uint32_t now, uint32_t keys_p1, uint32_t keys_p2)
{
    unsigned i;
    m->match_seed = seed ? seed : 0x6d2b79f5u;
    m->mode_versus = mode_versus != 0;
    m->state = MATCH_COUNTDOWN; m->result = RESULT_NONE;
    m->countdown_started_ms = now;
    m->countdown = m->countdown_ms ? 3 : 0;
    m->last_update_ms = now; m->updates = 0;
    m->blocked_keys[0] = (uint8_t)(keys_p1 & 31u);
    m->blocked_keys[1] = (uint8_t)(keys_p2 & 31u);
    for (i=0; i<2; ++i) {
        game_init(&m->game[i], m->match_seed, 0, 0, now);
        m->game[i].managed = 1;
        m->game[i].garbage_rng = next_random(m->match_seed ^ (i ? 0x243f6a88u : 0xb7e15162u));
        m->pending[i] = 0;
        m->attack_total[i] = m->sent_total[i] = m->received_total[i] = 0;
        m->cancelled_total[i] = m->drop_total[i] = m->overflow_total[i] = 0;
    }
}

void match_init(PuyoMatch *m, uint32_t seed, uint32_t now)
{
    m->session_rng = seed ? seed : 0x6d2b79f5u;
    m->countdown_ms = 3000;
    m->previous_start = 0;
    match_start(m, m->session_rng, 0, now, 0, 0);
    m->state = MATCH_MENU; m->countdown = 0;
}

static int finish_if_lost(PuyoMatch *m)
{
    unsigned a = m->game[0].game_over;
    unsigned b = m->mode_versus && m->game[1].game_over;
    if (!a && !b) return 0;
    m->state = MATCH_RESULT;
    m->result = !m->mode_versus ? RESULT_SOLO_OVER :
        a && b ? RESULT_DRAW : a ? RESULT_P2_WIN : RESULT_P1_WIN;
    return 1;
}

void match_step(PuyoMatch *m, uint32_t now, uint32_t keys_p1,
                uint32_t keys_p2, unsigned mode_versus, unsigned start_button)
{
    uint32_t keys[2];
    uint16_t attack[2], sent[2], clipped[2], previous_pending[2];
    unsigned i, players = m->mode_versus ? 2 : 1;
    unsigned start_edge;
    if (now == m->last_update_ms) return;
    m->last_update_ms = now;
    keys[0] = keys_p1 & 31u; keys[1] = keys_p2 & 31u;
    start_edge = start_button && !m->previous_start;
    m->previous_start = start_button != 0;
    if (m->state == MATCH_MENU || m->state == MATCH_RESULT ||
        (m->state == MATCH_PLAY && !m->mode_versus && start_edge)) {
        if (m->state == MATCH_MENU) m->mode_versus = mode_versus != 0;
        if (start_edge) {
            /* Session RNG survives matches. A UI restart never restores the
               original seed; wall time mixes in human restart timing. */
            m->session_rng = next_random(m->session_rng ^ now ^ 0x9e3779b9u);
            match_start(m, m->session_rng, mode_versus, now, keys[0], keys[1]);
        }
        return;
    }
    if (m->state == MATCH_COUNTDOWN) {
        uint32_t spent = now-m->countdown_started_ms;
        /* Include keys pressed at any point before PLAY, not merely keys
           held at the initial start edge. Each is masked until released. */
        for (i=0; i<2; ++i) m->blocked_keys[i] = (uint8_t)keys[i];
        if (spent < m->countdown_ms) {
            uint32_t left = m->countdown_ms-spent;
            m->countdown = (uint8_t)(left > 2000 ? 3 : left > 1000 ? 2 : 1);
            return;
        }
        m->countdown = 0; m->state = MATCH_PLAY;
        for (i=0; i<2; ++i) {
            m->game[i].drop_started_ms = now;
            m->game[i].held_started_ms = m->game[i].repeat_started_ms = now;
            m->game[i].previous_buttons = 0;
        }
        return;
    }
    ++m->updates;
    /* The clock/key snapshot is shared. Each player makes ONE bounded step;
       the other is never blocked by an opponent's animation or chain. */
    for (i=0; i<players; ++i) {
        m->blocked_keys[i] &= (uint8_t)keys[i];
        game_step(&m->game[i], now, keys[i] & ~m->blocked_keys[i]);
    }
    if (finish_if_lost(m)) return;
    for (i=0; i<2; ++i) {
        attack[i] = m->mode_versus && m->game[i].turn_ready ? m->game[i].round_attack : 0;
        previous_pending[i] = m->pending[i];
    }
    match_settle(m->pending, attack[0], attack[1], sent, clipped);
    for (i=0; i<2; ++i) {
        unsigned own_cancel = attack[i] < previous_pending[i] ? attack[i] : previous_pending[i];
        m->attack_total[i] = saturating_add(m->attack_total[i], attack[i]);
        m->cancelled_total[i] = saturating_add(m->cancelled_total[i], own_cancel);
        m->sent_total[i] = saturating_add(m->sent_total[i], sent[i]);
        m->received_total[i] = saturating_add(m->received_total[i], sent[i^1u]);
        m->overflow_total[i] = saturating_add(m->overflow_total[i], clipped[i]);
    }
    /* Reservation occurs after BOTH attacks have been received/cancelled.
       The grant is removed from pending and never cancellable in flight. */
    for (i=0; i<players; ++i) if (m->game[i].turn_ready) {
        unsigned grant = m->mode_versus ? m->pending[i] : 0;
        if (grant > MATCH_DROP_MAX) grant = MATCH_DROP_MAX;
        m->pending[i] -= (uint16_t)grant;
        m->drop_total[i] = saturating_add(m->drop_total[i], grant);
        game_finish_turn(&m->game[i], grant, now);
    }
    (void)finish_if_lost(m);
}
