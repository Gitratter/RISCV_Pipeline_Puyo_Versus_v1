# Keyboard Interface

## Connection

Connect a USB keyboard directly to the Basys 3 J2 USB Type-A host connector.
The onboard PIC24 converts keyboard input to a PS/2 interface for the FPGA.
The FPGA receiver is not a USB host controller.

J4 Micro-USB is used for programming and UART, not keyboard input.
USB hubs are not supported. Wireless receivers and composite keyboards may
require separate compatibility testing.

| Signal | FPGA pin | Standard | Pull-up |
|---|---|---|---|
| PS2Clk | C17 | LVCMOS33 | Enabled |
| PS2Data | B17 | LVCMOS33 | Enabled |

Both signals are inputs. The interface is receive-only.

## Receiver

`PuyoKeyboard` runs in the 25 MHz system clock domain. PS2Clk is synchronized
and filtered rather than used as a generated clock. Clock and data each pass
through two synchronizer stages. The clock filter requires eight stable
samples by default.

Filtered falling edges sample a frame containing a low start bit, eight
data bits in least-significant-bit-first order, odd parity, and a high stop
bit. A valid frame produces a one-cycle `byte_valid` pulse and updates
`last_byte`.

Incomplete frames and prefixes time out after 2,000 microseconds without
clock edges. Invalid frames, malformed prefixes, and timeouts clear prefix
and held-key state and increment a saturating 32-bit error counter.

## Scan codes

| Key bit | Action | Make | Break |
|---:|---|---|---|
| 0 | Move left | E0 6B | E0 F0 6B |
| 1 | Move right | E0 74 | E0 F0 74 |
| 2 | Soft drop | E0 72 | E0 F0 72 |
| 3 | Z: counterclockwise | 1A | F0 1A |
| 4 | X: clockwise | 22 | F0 22 |

E0 selects extended codes and F0 selects release. Unknown codes consume their
prefix without changing game inputs. E1 consumes the next seven Pause
sequence bytes. AA clears held state after a successful keyboard self-test;
FC, FD, 00, and FF clear state and increment the error count.

## Held keys and repeat

The output contains five independent held-key levels. Repeated make codes
leave an already-held bit set and do not create another rotation edge.
Movement repeat and rotation-edge detection are implemented in the common
game firmware, using the same timing for both players.

Movement can be combined with rotation or soft drop. Keyboard typematic
timing does not control the game's horizontal repeat rate. Enter and gameplay
keys cannot restart the match or reset the system.

Held keys are not cleared merely because the keyboard becomes silent:
multi-key operation does not guarantee repeated make codes for every held
key. A disconnect that loses a release frame may therefore leave a key held.
Protocol errors clear the state, but hot-plug recovery is not guaranteed.

At match start, firmware masks held keys until their physical release is
observed. Resetting only the receiver would not reliably suppress later
typematic repeats.

## Constraints and limitations

Input timing exceptions end at the first synchronizer flip-flop. They do not
exclude the downstream game or CPU paths from timing analysis.

Actual keyboard compatibility and the 2 ms timeout margin through the USB
bridge require physical-board testing.

## References

- [Basys 3 Reference Manual, section 6](https://digilent.com/reference/_media/reference/programmable-logic/basys-3/basys3_rm.pdf)
- [Basys 3 Master XDC](https://github.com/Digilent/digilent-xdc/blob/master/Basys-3-Master.xdc)
- [Digilent keyboard example](https://github.com/Digilent/Basys-3-Keyboard)
