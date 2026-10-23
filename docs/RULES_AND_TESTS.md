# Game Rules

## Board and movement

Each field has six columns and fourteen internal rows. Rows 0 and 1 are hidden;
rows 2 through 13 form the visible 6 x 12 field. Cell values are 0 for empty,
1 through 4 for colors, and 5 for garbage.

The active pair is separate from fixed cells. It spawns with its pivot at
`(2,1)` and its satellite at `(2,0)`. Orientations 0, 1, 2, and 3 mean up,
right, down, and left. Both destination cells must be in bounds and empty.
Failed moves and rotations leave the pair unchanged.

A blocked downward move locks the pair immediately. There are no wall kicks,
quick turns, or lock delays. A horizontal pair placed on uneven support
separates under gravity after locking.

Horizontal movement advances once on press, waits 180 ms, then repeats every
60 ms. Holding both directions produces no movement. Rotation occurs once
per new press; holding both rotation keys produces no rotation. Movement and
rotation can be combined, as can movement and soft drop.

## Piece sequence

Both players start with the same piece-generator seed and advance independent
generator states. Their nth pairs therefore match even when placement speeds
differ. Equal-color pairs are allowed. The displayed NEXT pair becomes the
next active pair without regeneration.

Garbage-column selection uses separate random state. It does not consume the
normal piece sequence. Rematches advance a persistent session generator and
mix in the current time. This is pseudorandom generation, not a guarantee that
every power cycle or pair will differ.

Tests can supply an explicit match seed or a fixed sequence of `PuyoPair`
values.

## Gravity, clearing, and chains

After locking, the field settles before connected-group search begins.
Gravity preserves the vertical order of each column. The two hidden rows
participate in settling and clearing.

Groups of four or more same-color cells connected horizontally or vertically
are marked for removal. Diagonal contact does not count. The entire field is
scanned before any marked cell is removed. Disconnected groups cleared in the
same stage count as one chain stage.

Garbage never forms a clearing group. Only garbage directly adjacent to a
marked normal cell is removed. Diagonal garbage and garbage connected only
through another garbage cell remain. Each cell is removed at most once.

After clearing, gravity and search repeat until no group remains. Active input
is ignored during this resolution. The next pair is not spawned early.
Garbage does not contribute to cleared-normal counts, score, or attack.

## Score and attack

Score and attack are separate:

```text
stage score  = cleared normal cells * 10 * chain number
stage attack = floor(cleared normal cells / 4) * chain factor
```

| Chain number | 1 | 2 | 3 | 4 | 5 | 6 | 7 and above |
|---|---:|---:|---:|---:|---:|---:|---:|
| Factor | 1 | 2 | 4 | 8 | 12 | 18 | 18 |

The factor table is in `game_attack_factor`. Stage attacks accumulate in
`round_attack` and are submitted once, after the entire chain ends.

Examples:

- Four normal cells cleared: attack 1.
- Two stages of four cells each: attack 1 + 2 = 3.
- Three stages of four cells each: attack 1 + 2 + 4 = 7.
- Two separate four-cell groups cleared together: attack 2 in one stage.

## Cancellation

Pending garbage is received garbage that has not yet been reserved for a drop.
When one or both players finish a chain, the shared match manager:

1. Cancels each player's attack against that player's pre-update pending count.
2. Cancels the two remaining outgoing attacks against each other.
3. Adds only the remaining difference to the receiving player's pending count.
4. Reserves a drop for each player whose turn has ended.

Thus pending 10 against attack 6 leaves pending 4 and sends nothing. Pending
10 against attack 14 leaves no pending garbage and sends 4. Simultaneous
attacks of 10 and 6 with no pending garbage add only 4 to P2.

A held completion notification is consumed once. It cannot submit the same
attack repeatedly. Simultaneous means the same logical `match_step` update,
not the same individual CPU instruction.

Pending counts saturate at 65,535. Intermediate arithmetic uses 32 bits to
prevent wraparound and unsigned underflow. Excess is recorded in
`overflow_total` and is not retained as additional pending garbage.
The 32-bit diagnostic totals also saturate.

## Garbage drops

After chain completion and cancellation, each player reserves at most 30
pending pieces once per turn. Reserved pieces are removed from pending
immediately and cannot be cancelled afterward. Additional attacks received
during the drop remain pending for the next turn.

For a reservation of N:

- Each column receives `floor(N / 6)` pieces.
- The remaining `N mod 6` pieces go to distinct randomly selected columns.

For example, eight pieces produce one per column plus one in two distinct
columns. A pending count of 31 reserves 30 and leaves one for a later turn,
with a playable pair in between.

Pieces enter at internal row 0. Each insertion wave settles before the next
wave. Existing cells are never overwritten, and gaps below occupied cells
are not filled directly. A blocked entry sets an overflow condition; pieces
are not reassigned to another column. Defeat is resolved after the inserted
wave settles.

`garbage_columns[x]` is the number still waiting for insertion in column x.
`garbage_inflight` is the total not yet inserted, not the number still falling.
Inserted pieces are already part of the board. A zero inflight count alone
does not mean the drop has settled. A defeat may leave reserved pieces
uninserted.

`drop_total` counts reservations rather than landed pieces.
`cancelled_total` counts cancellation against the player's own pending
garbage, not cross-cancellation between outgoing attacks.
`overflow_total` describes counter saturation; `garbage_overflow` describes
board overflow.

## Start, result, and rematch

A three-second countdown starts both players together. Gameplay keys held
through the countdown are masked until released.

A player loses if a next-pair spawn cell is occupied after resolution, or if
a garbage piece cannot enter its assigned column. Defeat for both players in
the same logical update produces DRAW, including different defeat causes.
Otherwise, the first confirmed defeat determines the result.

The result freezes both players, further attacks, and garbage reservations.
The display and clocks continue running. Rematch resets boards, search state,
active and next pairs, score, chains, pending and reserved garbage, counters,
and timers.

## Simplifications

This implementation does not reproduce the original game's full rules.
It omits wall kicks, quick turns, lock delay, special pairs, all-clear and
color bonuses, and time-dependent attack scaling. Attacks are settled at
chain completion, drops are capped at 30 per turn, and both hidden rows
participate in clearing.

## Tests

[Single-player tests](../tests/README.md) cover the common game mechanics.
[Versus tests](../tests/README_VERSUS.md) cover ordering, garbage, attacks,
cancellation, and results. See [Verification](VERIFICATION.md) for test
status and tool limitations.
