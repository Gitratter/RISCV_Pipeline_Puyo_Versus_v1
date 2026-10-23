# Single-Player Tests

`game_tests.c` provides `game_run_tests()`. It returns zero on success or
`case_number * 100 + check_number` on failure.
`game_test_passed` counts completed cases; `game_test_count` is 26.

The same C source runs natively and on the RV32I CPU. Defining `HOST_TEST`
adds a host main function and console output. Game state and search arrays
use static storage. Tests check guards, cell values, queue limits, pair
bounds, and collisions. Update loops have finite limits.

## Cases

| Case | Coverage |
|---|---|
| 1-3 | Three cells remain; groups of four and five clear completely |
| 4-6 | L, square, and T shapes |
| 7 | Diagonal-only connection does not clear |
| 8 | Separate groups clear simultaneously after a complete scan |
| 9 | Unsupported half of a horizontal pair falls independently |
| 10 | Two-chain fixture: 40 + 80 points, empty final field |
| 11 | Three-chain fixture: 40 + 100 + 120 points, empty final field |
| 12-13 | Side, top, and bottom boundaries; no wall kick; fixed-cell collisions |
| 14 | Input isolation during chains; no queued rotation for the next pair |
| 15 | NEXT matches actual spawn; equal-color pairs |
| 16 | Blocked spawn after resolution and restart |
| 17 | Clearing includes hidden rows |
| 18 | Restart during clear wait resets all search arrays |
| 19 | Maximum 84-cell component |
| 20 | Gap removal and column-order preservation |
| 21 | Normal/soft drop timing and repeated timestamp |
| 22 | 180 ms clear display followed by simultaneous removal |
| 23 | Initial movement, 180 ms DAS, 60 ms ARR, opposing directions |
| 24 | One rotation per press and opposing rotation keys |
| 25 | Persistent random state, held restart, reproducible fixed sequence |
| 26 | Timestamp wrap and default timing |

## Run

From the `software` directory:

```powershell
gcc -std=c99 -Wall -Wextra -Werror -O2 -DHOST_TEST firmware/game.c tests/game_tests.c -o game_tests.exe
./game_tests.exe
```

For RV32I execution, build the `tests` firmware variant and use
`tb_rv32_game_tests` with `puyo_tests.mem` and `puyo_tests_data.mem`.

See [Versus tests](README_VERSUS.md) and
[Verification](../docs/VERIFICATION.md) for complementary coverage and
tool limitations.
