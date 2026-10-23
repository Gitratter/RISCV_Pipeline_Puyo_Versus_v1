# Game Engine API

`game.c` implements the per-player state machine. `match.c` applies it to
two independent `PuyoGame` instances and manages start, cancellation, garbage
reservations, and results.

## Coordinates and state

The fixed field is `board[y*6+x]`, with x=0..5 and y=0..13.
Rows 0 and 1 are hidden. Values are 0 for empty, 1..4 for normal colors, and
5 for garbage.

The active pair is separate from the field. It spawns with pivot `(2,1)`
and satellite `(2,0)`. Orientations 0..3 mean up, right, down, and left.

## Functions

| Function | Behavior |
|---|---|
| `game_init` | Initializes state, timing defaults, random or fixed sequence, active pair, and NEXT |
| `game_step` | Advances one bounded game update using a millisecond timestamp and stable key levels |
| `game_restart` | Resets per-player state; normal mode advances random state, fixed mode restarts its sequence |
| `game_begin_resolve` | Starts gravity and clearing for the fixed field |
| `game_pair_fits` | Checks both pair cells for bounds and occupancy |
| `game_finish_turn` | Consumes a managed boundary notification after arbitration and accepts a garbage reservation |
| `game_attack_factor` | Returns the configured chain attack factor |

`game_init(g, seed, fixed_pairs, fixed_count, now_ms)` uses random generation
when `fixed_pairs` is null. Otherwise it cycles through the supplied
`PuyoPair` array, which must remain valid for the game lifetime.

For a managed match, `managed=1` enables boundary handoff to `match.c`.
Use `match_step` for production input dispatch, including SW14. Calling
standalone restart directly would bypass match-wide state management.

To test a fixed sequence in versus mode, initialize the match, initialize
both player states with the same fixed array before PLAY, and restore
`managed=1` on both.

## Input

Bits 0..4 are left, right, down, counterclockwise, and clockwise.
Bit 5 is the standalone restart input; the match manager handles start and
rematch separately and passes only gameplay bits to the managed states.

Simultaneous left/right or simultaneous rotation directions are ignored.
Movement is attempted before rotation, then drop timing is evaluated.
A failed operation does not change position or orientation. There are no
wall kicks or quick turns.

Input history continues to update while resolving, so a rotation press does
not carry over to a later pair. Match-start masking also requires held keys
to be released before use.

## Resolution

Locking leads to gravity, then whole-field breadth-first search.
Search uses at most 84 queue entries, marks visited cells before enqueueing,
and does not modify the board until all clearing groups are known.

All marked groups clear together. Only directly adjacent garbage joins their
clear mask. Gravity preserves column order, then search repeats.
The two hidden rows participate in both operations.

Each clearing stage increments the chain once. Score uses normal cleared
cells only. The active chain resets for the next pair, while `last_chain`
retains the previous result.

## Time

Elapsed-time comparisons use unsigned subtraction to support 32-bit counter
wrap. Configure intervals below 2^31 ms and call the engine more frequently
than a full counter wrap.

A call performs at most one automatic or soft-drop movement and at most one
horizontal-repeat movement. It does not replay an arbitrary number of missed
ticks. Normal and soft drop share the last-drop timestamp and select different
thresholds; pressing down may therefore cause an immediate drop if 40 ms has
already elapsed.

| Configuration field | Default |
|---|---:|
| `auto_drop_ms` | 700 ms |
| `soft_drop_ms` | 40 ms |
| `das_ms` | 180 ms |
| `arr_ms` | 60 ms |
| `fall_ms` | 40 ms |
| `clear_ms` | 180 ms |

See [Game rules](../docs/RULES_AND_TESTS.md) for attack and garbage behavior.
