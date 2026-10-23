# Versus Tests

`versus_tests.c` provides `versus_run_tests()`. It returns zero on success
or `case_number * 100 + check_number` on failure.
`versus_test_passed` counts completed cases; `versus_test_count` is 32.

The host and RV32I builds use the same game and match implementation.
`HOST_TEST` adds only the host entry point and console output.
Fixtures use static storage, deterministic seeds, and bounded update loops.

## Cases

| Case | Coverage |
|---:|---|
| 1 | Six connected garbage cells remain without score, chain, or attack |
| 2 | Only garbage directly adjacent to normal clear targets disappears |
| 3 | Shared adjacent garbage is removed once and excluded from normal counts |
| 4 | Two separate four-cell groups form one clearing stage and attack 2 |
| 5 | Two-chain fixture sends attack 3 only at completion; score 120 |
| 6 | Three-chain fixture sends attack 7; score 260; later factors are checked |
| 7 | P2 can move, rotate, and soft-drop while P1 resolves |
| 8 | Matching nth pairs, NEXT, and generator state despite different speeds |
| 9 | Garbage-column randomness does not consume the piece sequence |
| 10 | Pending 10, attack 6: pending 4, sent 0 |
| 11 | Pending 10, attack 10: pending 0, sent 0 |
| 12 | Pending 10, attack 14: pending 0, sent 4 |
| 13 | Simultaneous 10 versus 6 attacks; player-swap symmetry across combinations |
| 14 | Self-cancellation uses both pre-update pending values before cross-cancellation |
| 15 | Pending saturation at 65,535 with accounted excess and no wrap |
| 16 | Completion is consumed once; repeated timestamps do not duplicate attacks |
| 17 | Five garbage pieces occupy five distinct columns |
| 18 | Six garbage pieces distribute one per column |
| 19 | Eight distribute one per column plus two distinct extras |
| 20 | Thirty distribute five per column |
| 21 | Thirty-one reserve thirty and carry one past the next playable pair |
| 22 | Cancellation and reception precede a same-update drop reservation |
| 23 | Attacks received during a drop remain pending for the next turn |
| 24 | Blocked top entry neither fills lower holes nor overwrites cells; defeat follows settling |
| 25 | Simultaneous spawn failures draw; results freeze game and attack updates |
| 26 | An earlier defeat remains decisive despite a later opponent defeat |
| 27 | New rematch seed, countdown, synchronized start, and held-key release gating |
| 28 | Shared DAS/ARR, combined inputs, rotation-repeat suppression, and opposing directions |
| 29 | Single-player ignores the opponent and allows SW14 restart during PLAY |
| 30 | Versus ignores mid-match start; timestamp wrap and full rematch reset |
| 31 | Spawn failure and garbage overflow in the same update draw without a new attack |
| 32 | Garbage settles above packed columns without overwriting their contents |

Case 8 injects boundary notifications to isolate sequence behavior.
Cases 10-15 call `match_settle` directly to isolate arbitration.
Keyboard signaling, video timing, and peripheral transactions are tested by
separate RTL benches.

## Run

From the `software` directory:

```powershell
gcc -std=c99 -O2 -Wall -Wextra -Werror -DHOST_TEST firmware/game.c firmware/match.c tests/versus_tests.c -o versus_tests.exe
./versus_tests.exe
```

For RV32I execution, build `versus_tests` and use `tb_rv32_versus_tests`
with `puyo_versus_tests.mem` and `puyo_versus_tests_data.mem`.

The [single-player suite](README.md) adds 26 cases. These are 58 C scenarios,
not 58 separate RTL testbenches. The production and test images use the
same game implementation.

See [Game rules](../docs/RULES_AND_TESTS.md) for the cancellation and reservation
definitions, and [Verification](../docs/VERIFICATION.md) for status and limitations.
