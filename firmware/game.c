#include "game.h"

static uint32_t random_next(uint32_t r)
{
    /* Nonzero xorshift32: software restart advances this session state. */
    if (r == 0) r = 0x6d2b79f5u;
    r ^= r << 13;
    r ^= r >> 17;
    r ^= r << 5;
    return r;
}

unsigned game_attack_factor(unsigned chain)
{
    static const uint8_t factors[6] = {1,2,4,8,12,18};
    return chain == 0 ? 0 : factors[chain < 6 ? chain - 1 : 5];
}

static uint32_t interval(uint32_t value) { return value ? value : 1u; }
static int elapsed(uint32_t now, uint32_t then, uint32_t duration)
{
    /* Unsigned subtraction handles millisecond timer wrap. Intervals must
       be below 2^31 ms and game_step must run at least once per timer wrap. */
    return (uint32_t)(now - then) >= duration;
}
static int satellite_dx(unsigned d) { return d == 1 ? 1 : d == 3 ? -1 : 0; }
static int satellite_dy(unsigned d) { return d == 0 ? -1 : d == 2 ? 1 : 0; }
static int direction(uint32_t buttons)
{
    unsigned b = buttons & (BUTTON_LEFT | BUTTON_RIGHT);
    return b == BUTTON_LEFT ? -1 : b == BUTTON_RIGHT ? 1 : 0;
}

int game_pair_fits(const PuyoGame *g, int x, int y, unsigned orientation)
{
    int sx, sy;
    if (orientation > 3) return 0;
    sx = x + satellite_dx(orientation);
    sy = y + satellite_dy(orientation);
    /* Never form an array subscript before checking BOTH coordinates. */
    if (x < 0 || x >= GAME_WIDTH || y < 0 || y >= GAME_HEIGHT ||
        sx < 0 || sx >= GAME_WIDTH || sy < 0 || sy >= GAME_HEIGHT) return 0;
    return g->board[y * GAME_WIDTH + x] == 0 &&
           g->board[sy * GAME_WIDTH + sx] == 0;
}

static void prepare_next(PuyoGame *g)
{
    if (g->fixed_pairs && g->fixed_count) {
        PuyoPair p = g->fixed_pairs[g->fixed_index];
        /* Invalid fixture colors become color1; never publish color0/5..255. */
        g->next_a = p.a >= 1 && p.a <= 4 ? p.a : 1;
        g->next_b = p.b >= 1 && p.b <= 4 ? p.b : 1;
        ++g->fixed_index;
        if (g->fixed_index >= g->fixed_count) g->fixed_index = 0;
    } else {
        g->rng = random_next(g->rng);
        g->next_a = (uint8_t)((g->rng & 3u) + 1u);
        g->next_b = (uint8_t)(((g->rng >> 2) & 3u) + 1u);
    }
}

static void set_phase(PuyoGame *g, unsigned phase, uint32_t now)
{
    g->phase = (uint8_t)phase;
    g->phase_started_ms = now;
    g->busy = phase != GAME_ACTIVE && phase != GAME_OVER;
}

static void spawn(PuyoGame *g, uint32_t now)
{
    g->chain = 0;
    g->drop_started_ms = now;
    g->held_direction = (int8_t)direction(g->previous_buttons);
    g->held_started_ms = g->repeat_started_ms = now;
    g->repeating = 0;
    if (!game_pair_fits(g, 2, 1, 0)) {
        g->active_valid = 0;
        g->game_over = 1;
        set_phase(g, GAME_OVER, now);
        return;
    }
    g->active_x = 2; g->active_y = 1; g->orientation = 0;
    g->color_a = g->next_a; g->color_b = g->next_b;
    g->active_valid = 1;
    ++g->pair_number;
    prepare_next(g);
    set_phase(g, GAME_ACTIVE, now);
}

static void reset_round(PuyoGame *g, uint32_t now)
{
    unsigned i;
    for (i = 0; i < GAME_CELLS; ++i) {
        g->board[i] = g->clear[i] = g->visited[i] = 0;
        g->queue[i] = g->queue_x[i] = 0;
    }
    g->active_valid = 0;
    g->active_x = 2; g->active_y = 1; g->orientation = 0;
    g->color_a = g->color_b = g->next_a = g->next_b = 0;
    g->score = 0;
    g->chain = g->last_chain = g->game_over = 0;
    g->queue_head = g->queue_tail = g->removed = 0;
    g->drop_started_ms = g->phase_started_ms = now;
    g->held_started_ms = g->repeat_started_ms = now;
    g->previous_buttons = 0;
    g->held_direction = 0; g->repeating = 0;
    g->fixed_index = 0;
    g->turn_ready = g->garbage_overflow = g->garbage_inflight = 0;
    g->round_attack = 0;
    g->pair_number = 0;
    for (i = 0; i < GAME_WIDTH; ++i) g->garbage_columns[i] = 0;
    prepare_next(g);
    spawn(g, now);
}

void game_init(PuyoGame *g, uint32_t seed, const PuyoPair *fixed_pairs,
               uint16_t fixed_count, uint32_t now_ms)
{
    g->config.auto_drop_ms = 700;
    g->config.soft_drop_ms = 40;
    g->config.das_ms = 180;
    g->config.arr_ms = 60;
    g->config.fall_ms = 40;
    g->config.clear_ms = 180;
    g->rng = seed ? seed : 0x6d2b79f5u;
    g->fixed_pairs = fixed_pairs;
    g->fixed_count = fixed_pairs ? fixed_count : 0;
    g->managed = 0;
    g->garbage_rng = random_next(g->rng ^ 0xa511e9b3u);
    reset_round(g, now_ms);
}

void game_restart(PuyoGame *g, uint32_t now_ms)
{
    /* Preserve/advance ordinary session RNG; do NOT restore a fixed seed.
       Colors can legitimately repeat. Fixed fixtures deliberately replay. */
    if (!(g->fixed_pairs && g->fixed_count))
        g->rng = random_next(g->rng ^ now_ms ^ 0x9e3779b9u);
    reset_round(g, now_ms);
}

void game_begin_resolve(PuyoGame *g, uint32_t now_ms)
{
    unsigned i;
    g->active_valid = 0;
    g->game_over = 0;
    g->chain = 0;
    g->round_attack = 0;
    g->turn_ready = 0;
    g->queue_head = g->queue_tail = g->removed = 0;
    g->drop_started_ms = now_ms;
    for (i = 0; i < GAME_CELLS; ++i) {
        g->clear[i] = g->visited[i] = 0;
        g->queue[i] = g->queue_x[i] = 0;
    }
    set_phase(g, GAME_GRAVITY, now_ms);
}

static void move_horizontal(PuyoGame *g, int d)
{
    int x = (int)g->active_x + d;
    if (game_pair_fits(g, x, g->active_y, g->orientation))
        g->active_x = (uint8_t)x;
}

static void handle_active(PuyoGame *g, uint32_t now, uint32_t buttons,
                          uint32_t pressed)
{
    int d = direction(buttons);
    unsigned next_orientation = g->orientation;
    uint32_t delay;
    if (d != g->held_direction) {
        g->held_direction = (int8_t)d;
        g->held_started_ms = g->repeat_started_ms = now;
        g->repeating = 0;
        if (d) move_horizontal(g, d);
    } else if (d) {
        if (!g->repeating && elapsed(now, g->held_started_ms, g->config.das_ms)) {
            move_horizontal(g, d);
            g->repeating = 1;
            g->repeat_started_ms = now;
        } else if (g->repeating && elapsed(now, g->repeat_started_ms,
                                          interval(g->config.arr_ms))) {
            move_horizontal(g, d);
            g->repeat_started_ms = now;
        }
    }
    /* Simultaneously-held rotation buttons cancel, including a new edge on
       one while the other is held. Holding a single button never repeats. */
    if ((buttons & (BUTTON_ROTATE_LEFT | BUTTON_ROTATE_RIGHT)) == BUTTON_ROTATE_LEFT &&
        (pressed & BUTTON_ROTATE_LEFT)) next_orientation = (g->orientation + 3u) & 3u;
    if ((buttons & (BUTTON_ROTATE_LEFT | BUTTON_ROTATE_RIGHT)) == BUTTON_ROTATE_RIGHT &&
        (pressed & BUTTON_ROTATE_RIGHT)) next_orientation = (g->orientation + 1u) & 3u;
    if (game_pair_fits(g, g->active_x, g->active_y, next_orientation))
        g->orientation = (uint8_t)next_orientation;
    delay = buttons & BUTTON_DOWN ? g->config.soft_drop_ms : g->config.auto_drop_ms;
    if (elapsed(now, g->drop_started_ms, interval(delay))) {
        int x = g->active_x, y = g->active_y;
        g->drop_started_ms = now;
        if (game_pair_fits(g, x, y + 1, g->orientation)) {
            ++g->active_y;
        } else {
            /* Only this core owns the board. Pair is checked before writing;
               its cells are distinct for all four allowed orientations. */
            if (game_pair_fits(g, x, y, g->orientation)) {
                int sx = x + satellite_dx(g->orientation);
                int sy = y + satellite_dy(g->orientation);
                g->board[y * GAME_WIDTH + x] = g->color_a;
                g->board[sy * GAME_WIDTH + sx] = g->color_b;
            }
            game_begin_resolve(g, now);
        }
    }
}

static int gravity_one_row(PuyoGame *g)
{
    int x, y, moved = 0;
    /* Bottom-to-top order moves each puyo by at most one cell in this pass,
       preserves ordering, and separates a horizontal pair over a step. */
    for (x = 0; x < GAME_WIDTH; ++x) {
        for (y = GAME_HEIGHT - 2; y >= 0; --y) {
            unsigned i = (unsigned)(y * GAME_WIDTH + x);
            if (g->board[i] && !g->board[i + GAME_WIDTH]) {
                g->board[i + GAME_WIDTH] = g->board[i];
                g->board[i] = 0;
                moved = 1;
            }
        }
    }
    return moved;
}

static void enqueue(PuyoGame *g, unsigned index, unsigned x, unsigned color)
{
    if (!g->visited[index] && g->board[index] == color && g->queue_tail < GAME_CELLS) {
        g->visited[index] = 1;
        g->queue[g->queue_tail] = (uint8_t)index;
        g->queue_x[g->queue_tail] = (uint8_t)x;
        ++g->queue_tail;
    }
}

static void scan_all(PuyoGame *g)
{
    unsigned i, x = 0, h, k;
    g->removed = 0;
    g->queue_head = g->queue_tail = 0;
    for (i = 0; i < GAME_CELLS; ++i) g->visited[i] = g->clear[i] = 0;
    for (i = 0; i < GAME_CELLS; ++i) {
        if (g->board[i] >= 1 && g->board[i] <= 4 && !g->visited[i]) {
            unsigned color = g->board[i];
            g->queue_head = g->queue_tail = 0;
            enqueue(g, i, x, color);
            /* Explicit 84-iteration limit; each index is enqueued once. */
            for (h = 0; h < GAME_CELLS && h < g->queue_tail; ++h) {
                unsigned cell = g->queue[h], cx = g->queue_x[h];
                g->queue_head = (uint8_t)(h + 1);
                if (cx > 0) enqueue(g, cell - 1, cx - 1, color);
                if (cx < GAME_WIDTH - 1) enqueue(g, cell + 1, cx + 1, color);
                if (cell >= GAME_WIDTH) enqueue(g, cell - GAME_WIDTH, cx, color);
                if (cell < GAME_CELLS - GAME_WIDTH) enqueue(g, cell + GAME_WIDTH, cx, color);
            }
            if (g->queue_tail >= 4) {
                for (k = 0; k < GAME_CELLS && k < g->queue_tail; ++k) {
                    g->clear[g->queue[k]] = 1;
                    ++g->removed;
                }
            }
        }
        ++x;
        if (x == GAME_WIDTH) x = 0;
    }
    /* Only marked NORMAL cells are sources. Marked garbage is never a
       source, so removal cannot propagate along a row of garbage. */
    x = 0;
    for (i = 0; i < GAME_CELLS; ++i) {
        if (g->clear[i] && g->board[i] >= 1 && g->board[i] <= 4) {
            if (x && g->board[i-1] == GAME_GARBAGE) g->clear[i-1] = 1;
            if (x < GAME_WIDTH-1 && g->board[i+1] == GAME_GARBAGE) g->clear[i+1] = 1;
            if (i >= GAME_WIDTH && g->board[i-GAME_WIDTH] == GAME_GARBAGE)
                g->clear[i-GAME_WIDTH] = 1;
            if (i < GAME_CELLS-GAME_WIDTH && g->board[i+GAME_WIDTH] == GAME_GARBAGE)
                g->clear[i+GAME_WIDTH] = 1;
        }
        if (++x == GAME_WIDTH) x = 0;
    }
}

static void erase_marked(PuyoGame *g)
{
    unsigned i;
    uint32_t points = (uint32_t)g->removed * 10u;
    ++g->chain;
    g->last_chain = g->chain;
    /* removed counts normal colors only. All groups in one scan constitute
       one stage. Total attack is held until the complete chain settles. */
    {
        unsigned groups = g->removed >> 2;
        unsigned factor = game_attack_factor(g->chain);
        for (i = 0; i < groups; ++i) g->round_attack += (uint16_t)factor;
    }
    /* At most 84/4=21 erase stages, since no new pair appears during a chain.
       Bounded addition avoids a runtime RV32I multiply helper. */
    for (i = 0; i < 21 && i < g->chain; ++i) g->score += points;
    for (i = 0; i < GAME_CELLS; ++i) {
        if (g->clear[i]) g->board[i] = 0;
        g->clear[i] = 0;
    }
    g->removed = 0;
}

void game_finish_turn(PuyoGame *g, unsigned garbage_count, uint32_t now_ms)
{
    unsigned i, q, rem;
    uint8_t columns[GAME_WIDTH];
    if (!g->turn_ready || g->phase != GAME_BOUNDARY) return;
    /* This held ready token is consumed exactly once, even if the caller
       retries while the garbage animation is still busy. */
    g->turn_ready = 0;
    if (garbage_count > 30) garbage_count = 30;
    g->garbage_inflight = (uint8_t)garbage_count;
    g->garbage_overflow = 0;
    q = garbage_count / GAME_WIDTH;
    rem = garbage_count % GAME_WIDTH;
    for (i = 0; i < GAME_WIDTH; ++i) {
        g->garbage_columns[i] = (uint8_t)q;
        columns[i] = (uint8_t)i;
    }
    /* Partial Fisher-Yates: the remainder visits distinct columns. This
       RNG is entirely independent from the normal pair generator. */
    for (i = 0; i < rem; ++i) {
        unsigned j;
        uint8_t temp;
        g->garbage_rng = random_next(g->garbage_rng);
        j = i + g->garbage_rng % (GAME_WIDTH-i);
        temp = columns[i]; columns[i] = columns[j]; columns[j] = temp;
        ++g->garbage_columns[columns[i]];
    }
    if (garbage_count) set_phase(g, GAME_GARBAGE_INSERT, now_ms);
    else set_phase(g, GAME_SPAWN, now_ms);
}

static void garbage_insert(PuyoGame *g, uint32_t now)
{
    unsigned x, remaining = 0;
    /* Injection is ALWAYS at internal row zero, never into a hole beneath
       an occupied cell. One horizontal wave is settled before the next. */
    for (x = 0; x < GAME_WIDTH; ++x) if (g->garbage_columns[x]) {
        if (g->board[x]) g->garbage_overflow = 1;
        else {
            g->board[x] = GAME_GARBAGE;
            --g->garbage_columns[x];
            --g->garbage_inflight;
        }
        remaining += g->garbage_columns[x];
    }
    if (remaining || g->garbage_overflow || g->garbage_inflight == 0)
        set_phase(g, GAME_GARBAGE_FALL, now);
}

static void garbage_settled(PuyoGame *g, uint32_t now)
{
    if (g->garbage_overflow) {
        g->active_valid = 0; g->game_over = 1;
        set_phase(g, GAME_OVER, now);
    } else if (g->garbage_inflight) set_phase(g, GAME_GARBAGE_INSERT, now);
    else set_phase(g, GAME_SPAWN, now);
}

void game_step(PuyoGame *g, uint32_t now_ms, uint32_t stable_buttons)
{
    uint32_t buttons = stable_buttons & 63u;
    uint32_t pressed = buttons & ~g->previous_buttons;
    g->previous_buttons = buttons;
    if (pressed & BUTTON_RESTART) {
        game_restart(g, now_ms);
        /* Restart must be edge-triggered, even if held for many frames. */
        g->previous_buttons = buttons;
        return;
    }
    if (g->phase == GAME_ACTIVE) {
        handle_active(g, now_ms, buttons, pressed);
        return;
    }
    /* Busy input edges are consumed here, never queued for a future pair. */
    g->held_direction = (int8_t)direction(buttons);
    g->held_started_ms = g->repeat_started_ms = now_ms;
    g->repeating = 0;
    switch (g->phase) {
    case GAME_SPAWN:
        spawn(g, now_ms);
        break;
    case GAME_GRAVITY:
        if (gravity_one_row(g)) set_phase(g, GAME_FALL_WAIT, now_ms);
        else set_phase(g, GAME_SCAN, now_ms);
        break;
    case GAME_FALL_WAIT:
        if (elapsed(now_ms, g->phase_started_ms, interval(g->config.fall_ms)))
            set_phase(g, GAME_GRAVITY, now_ms);
        break;
    case GAME_SCAN:
        scan_all(g);
        if (g->removed) set_phase(g, GAME_CLEAR_WAIT, now_ms);
        else if (g->managed) {
            g->turn_ready = 1;
            set_phase(g, GAME_BOUNDARY, now_ms);
        } else set_phase(g, GAME_SPAWN, now_ms);
        break;
    case GAME_CLEAR_WAIT:
        if (elapsed(now_ms, g->phase_started_ms, interval(g->config.clear_ms))) {
            erase_marked(g);
            set_phase(g, GAME_GRAVITY, now_ms);
        }
        break;
    case GAME_GARBAGE_INSERT:
        garbage_insert(g, now_ms);
        break;
    case GAME_GARBAGE_FALL:
        if (gravity_one_row(g)) set_phase(g, GAME_GARBAGE_WAIT, now_ms);
        else garbage_settled(g, now_ms);
        break;
    case GAME_GARBAGE_WAIT:
        if (elapsed(now_ms, g->phase_started_ms, interval(g->config.fall_ms)))
            set_phase(g, GAME_GARBAGE_FALL, now_ms);
        break;
    case GAME_BOUNDARY:
        break;
    case GAME_OVER:
    default:
        break;
    }
}
