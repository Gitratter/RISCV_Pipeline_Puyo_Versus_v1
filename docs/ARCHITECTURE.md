# Architecture

## Hardware and software boundary

One RV32I pipelined CPU runs two instances of the same game state machine.
The CPU owns the authoritative board data in data RAM. Hardware provides
synchronized inputs, timekeeping, display registers, and VGA scanout.

```text
Board buttons -> synchronizers/debounce --+
USB keyboard -> PS/2 receiver -----------+-> MMIO -> RV32I CPU
Millisecond timer -----------------------+             |
                                                  PuyoMatch
                                               /             \
                                         PuyoGame[0]     PuyoGame[1]
                                               \             /
                                              Match arbitration
                                                       |
                                               Display MMIO writes
                                                       |
                                               Atomic two-player commit
                                                       |
                                                VGA frame snapshot
```

The CPU runs continuously at 25 MHz. Movement, automatic drop, key repeat, and
animations use elapsed milliseconds rather than a reduced CPU clock.
Both instruction and data memories use the existing synchronous BRAM interface.

## Components

| Component | Responsibility |
|---|---|
| `TD8_RISCV_V31_Core` | RV32I execution, pipeline control, memory transactions |
| `PuyoButtons` | Board input synchronization and debounce |
| `PuyoKeyboard` | PS/2 reception and independent held-key states |
| `PuyoModeSwitch` | Mode-switch synchronization and debounce |
| `PuyoVersusMMIO` | Input reads and staged display writes |
| `PuyoVersusVideo` | Two-player layout and match overlays |
| `PuyoVideo` | Single-player renderer |
| `PuyoVersusSystem` | CPU and peripheral integration |
| `RISCV_PUYO_VERSUS_TOP` | Board pins, clock generation, and reset |
| `game.c` | Per-player rules and state |
| `match.c` | Shared match state and attack arbitration |
| `versus_main.c` | Main loop and display publication |

The standalone `PuyoSoftwareSystem` and its firmware remain available for
regression tests. They are not instantiated by the versus board top.

## Update model

Each main-loop iteration reads one timestamp and both players' input states.
It advances each player once, then resolves defeat, attacks, and garbage
reservations. Input reads occur in successive instructions; they are not an
electrically simultaneous latch.

A simultaneous event means an event produced in the same logical
`match_step` update. Both players' events are collected before arbitration.
Instruction execution is sequential, but gameplay is not turn-based.

Animation phases store a start time and return to the main loop. A player
waiting for gravity or clearing does not block the opponent. Repeated calls
with the same timestamp do not advance the match again. Delayed updates do
not trigger an unbounded catch-up loop. A hard one-millisecond execution
deadline has not been established.

Only the main loop writes game state. Display registers hold copies and never
perform collision, clearing, attack, or result calculations.

## State machines

The match states are `MENU`, `COUNTDOWN`, `PLAY`, and `RESULT`.

```text
ACTIVE
  -> lock -> GRAVITY <-> FALL_WAIT
  -> SCAN
       -> groups found -> CLEAR_WAIT -> clear all -> GRAVITY
       -> no groups -> BOUNDARY
                         -> cancel attacks and receive remainder
                         -> reserve up to 30 garbage pieces
                              -> zero -> SPAWN
                              -> nonzero -> GARBAGE_INSERT
                                           -> GARBAGE_FALL <-> GARBAGE_WAIT
                                           -> overflow check -> SPAWN
```

A boundary notification remains set until `game_finish_turn` consumes it.
Spawn failure is evaluated in a player step, not partway through arbitration.
This allows different defeat mechanisms in the same update to produce a draw.

Connected-group search uses bounded arrays and a queue of at most 84 cells.
There is no recursion or heap allocation. Search completes within a finite C
call; animation delays span multiple updates.

## Memory

| Region | Size | Use |
|---|---:|---|
| Instruction memory | 16 KiB | RV32I code |
| Data memory at 0x10000 | 4 KiB | Constants, game state, stack |

The production versus image uses 6,408 bytes of code, 8 bytes of read-only
data, and 1,132 bytes of BSS. The linker reserves at least 1,536 bytes for the
stack. Compiler stack-usage files are available, but runtime maximum stack
usage has not been measured.

`puyo_versus.mem` initializes instruction memory.
`puyo_versus_data.mem` initializes data memory.
Startup code clears BSS after reset. The linker keeps constants accessible
through data memory in the Harvard memory architecture.

## MMIO map

All accesses below are 32-bit.

| Address | Access | Meaning |
|---|---|---|
| 0x2000 | R | P1 keys: left, right, down, counterclockwise, clockwise, start in bits 0..5 |
| 0x2004 | R | Millisecond counter |
| 0x2008 | R | CPU cycle counter |
| 0x200C | R | Peripheral ID: 0x50565933 |
| 0x2010 | R | VGA frame count |
| 0x2014 | R | Display commit count |
| 0x2018 | R | P2 keys in bits 0..4, same order as P1 |
| 0x201C | R | Mode switch in bit 0 |
| 0x2020 | W | Selected player's cell index, valid range 0..83 |
| 0x2024 | W | Cell color in bits 2:0; clear mark in bit 8 |
| 0x2028 | W | Active pair position, orientation, and colors |
| 0x202C | W | Next pair colors |
| 0x2030 | W | Score |
| 0x2034 | W | Player phase and status |
| 0x2038 | W | Bit 0 commits both players and match status |
| 0x203C | W | Player write bank: bit 0 selects P1=0 or P2=1 |
| 0x2040 | R | Cycle count |
| 0x2044 | R | Retired instruction count |
| 0x2048 | R | Memory stall count |
| 0x204C | R | Load stall count |
| 0x2050 | R | Flush count |
| 0x2060 | W | Selected player's pending garbage count |
| 0x2064 | W | Match mode, state, result, and countdown |
| 0x2068 | R | Keyboard error count |

The legacy CPU input/output addresses 0x100 and 0x104 remain available.
See [Video](VIDEO.md) for packed display fields.

## Timing parameters

Both players use the same defaults.

| Field | Default |
|---|---:|
| `auto_drop_ms` | 700 |
| `soft_drop_ms` | 40 |
| `das_ms` | 180 |
| `arr_ms` | 60 |
| `fall_ms` | 40 |
| `clear_ms` | 180 |
| `countdown_ms` | 3000 |

Game timing is configured in `game_init`; countdown timing is configured in
`match_init`. Rebuild firmware and hardware after changing these values.
