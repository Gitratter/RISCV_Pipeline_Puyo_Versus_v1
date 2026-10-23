# VGA Display

## Layout

`PuyoVersusVideo` renders published display data. It does not calculate game
rules or results. Single-player PLAY uses the existing `PuyoVideo` output;
menu, countdown, and result states add a 24-pixel top banner.

Versus mode retains 32 x 32 pixel cells. Sidebar text uses a 5 x 7 pixel font
with an eight-pixel character pitch.

Coordinates are relative to the top-left visible pixel.

| Element | P1 | P2 |
|---|---|---|
| 6 x 12 field | x=16..207, y=48..431 | x=336..527, y=48..431 |
| Outer border | x=12..211, y=44..435 | x=332..531, y=44..435 |
| Sidebar start | x=216 | x=536 |
| NEXT pair | x=232..263, y=288..351 | x=552..583, y=288..351 |
| Control labels | x=16, y=440/456 | x=336, y=440/456 |

Each sidebar shows state, ten score digits, three current-chain digits,
three previous-chain digits, five incoming-garbage digits, and NEXT.
Results are WIN, LOSE, or DRAW. The top banner shows mode, start/rematch
instructions, and the countdown.

Internal rows 0 and 1 are hidden; visible row 0 corresponds to internal row 2.
The active pair is overlaid separately from fixed cells.

## Colors

| Cell value | Color | RGB444 |
|---:|---|---|
| 1 | Red | F45 |
| 2 | Green | 4E6 |
| 3 | Blue | 49F |
| 4 | Yellow | FD4 |
| 5 | Garbage gray | AAA |

Clearing cells are highlighted in white. Color IDs are separate from
`palette` and `puyo_pixel`, allowing the renderer to change without changing
board encoding.

## Packed display fields

For two-player fields, P1 occupies the low bits and P2 the high bits.

| Port | Width | Per-player encoding |
|---|---:|---|
| `boards` | 504 | 252 bits; cell color at `[index*3 +: 3]` |
| `clear_masks` | 168 | 84 one-bit clear marks |
| `active_words` | 32 | x[3:0], y[7:4], orientation[9:8], A[12:10], B[15:13] |
| `next_pairs` | 12 | A[2:0], B[5:3] |
| `scores` | 64 | 32-bit unsigned |
| `status_words` | 56 | chain[7:0], last[15:8], phase[23:16], active[24], busy[25], over[26], boot[27] |
| `pending_counts` | 32 | 16-bit unsigned |
| `match_status` | 32 | Shared: mode[0], state[3:1], result[6:4], countdown[9:8] |

Mode is 0 for single-player and 1 for versus.
State values are MENU=0, COUNTDOWN=1, PLAY=2, RESULT=3.
Result values are none=0, P1 win=1, P2 win=2, draw=3, single-player over=4.

## Frame consistency

The CPU writes both players' staging registers and then commits once.
MMIO publishes both players and the match status atomically. At pixel
`(0,480)`, the renderer captures all published values for the next visible
frame. It does not update them during active display.

If commit and capture occur on the same clock edge, nonblocking assignment
semantics select the previously published snapshot. The new snapshot appears
on the following frame. Both renderers share clock and reset, and mode
selection is also captured per frame.

Score conversion uses 32 double-dabble cycles; pending-count conversion uses
16. These conversions finish during vertical blanking.

## Timing

| Parameter | Value |
|---|---|
| Pixel clock | 25 MHz |
| Visible area | 640 x 480 |
| Total frame | 800 x 525 |
| Refresh rate | Approximately 59.52 Hz |
| HSYNC low interval | x=656..751, 96 pixels |
| VSYNC low interval | y=490..491, two lines |

RGB and synchronization outputs have the same one-cycle delay. RGB is zero
outside the visible area. `frame_tick` pulses once per frame.

The pixel clock is 25 MHz rather than the nominal 25.175 MHz VGA value.
Monitor compatibility requires physical testing.

## Display tests

`tb_versus_video` checks timing, blanking, palette and overlays, frame
snapshots, full-range score and pending conversion, chain values, and result
labels. It also compares a complete single-player PLAY frame with an
independent `PuyoVideo` instance.

The `+PPM` option exports a visible test frame. This is a renderer fixture,
not a photograph or capture of physical-board gameplay.
