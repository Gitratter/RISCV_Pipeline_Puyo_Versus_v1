# RISC-V Pipeline Puyo Versus v1

A single-player and two-player falling-pair puzzle game for the Digilent Basys 3.
An RV32I pipelined CPU runs the game firmware. Verilog implements the CPU,
memory, input interfaces, display registers, and VGA scanout.

## Hardware

| Item | Configuration |
|---|---|
| Board | Digilent Basys 3 |
| FPGA | xc7a35tcpg236-1 |
| Toolchain | Vivado 2025.2 |
| Board clock | 100 MHz |
| CPU and pixel clock | 25 MHz |
| Display | 640 x 480 VGA, approximately 59.52 Hz |
| Player 2 input | USB keyboard connected directly to the J2 USB Type-A port |

## Open and program

Extract the entire archive to a short path, such as `C:/FPGA/PuyoVersus`.
Open [RISCV_Puyo_Versus_v3.xpr](RISCV_Puyo_Versus_v3.xpr). The project already
contains the HDL, constraints, memory images, and simulation sources.

The design top is `RISCV_PUYO_VERSUS_TOP`. To build the hardware, run
**Run Synthesis**, **Run Implementation**, and **Generate Bitstream**.

A prebuilt [bitstream](programming/RISCV_PUYO_VERSUS_TOP.bit) is included.
In Hardware Manager, connect to the Basys 3 and select this file under
**Program Device**. A C compiler is not required to use the supplied memory
images or bitstream.

## Controls

| Action | Player 1: board | Player 2: keyboard |
|---|---|---|
| Move left / right | Left / right buttons | Left / right arrow keys |
| Soft drop | Down button | Down arrow key |
| Rotate counterclockwise | Center button | Z |
| Rotate clockwise | Up button | X |

- Set SW0 before starting: 0 for single-player, 1 for versus.
- Change SW14 from 0 to 1 to start the three-second countdown.
- After a result, return SW14 to 0 and then to 1 for a new match.
- SW14 and mode changes do not interrupt an active versus match.
- In single-player mode, SW14 can also restart an active game.
- SW15 is the dedicated hardware reset; keep it at 0 during play.
- Release any gameplay keys held through the countdown before pressing them again.

Connect the keyboard to J2, not the Micro-USB programming connector.
USB hubs are not supported. See [Keyboard interface](software/docs/KEYBOARD.md).

## Gameplay

Each player has a 6 x 12 visible field, four colors, and a two-piece active pair.
Groups of at least four orthogonally connected pieces clear together. Gravity
and repeated clears produce chains.

Versus mode uses identical piece sequences for both players, independent of
placement speed. Attacks first cancel pending incoming garbage. Remaining
attacks are sent after the chain ends. At most 30 garbage pieces drop at the
end of one turn.

These are simplified rules, not a complete reproduction of the original game.
See [Game rules](software/docs/RULES_AND_TESTS.md).

## Files

| Path | Contents |
|---|---|
| `RISCV_Puyo_Versus_v3.xpr` | Vivado project |
| `RISCV_Puyo_Versus_v3.srcs/` | Registered HDL, XDC, MEM, and testbench files |
| `software/firmware/` | RV32I firmware source |
| `software/tests/` | Game and match regression tests |
| `software/scripts/` | Firmware, simulation, and hardware build scripts |
| `software/docs/` | Technical documentation |
| `programming/` | Bitstream and implementation reports |

The project filenames are part of the build configuration; do not rename them
individually. For firmware changes, follow the [build instructions](software/README.md).

## Documentation

- [Architecture and memory map](software/docs/ARCHITECTURE.md)
- [Game rules](software/docs/RULES_AND_TESTS.md)
- [Keyboard interface](software/docs/KEYBOARD.md)
- [Video interface](software/docs/VIDEO.md)
- [Verification status and limitations](software/docs/VERIFICATION.md)
- [Game engine API](software/firmware/GAME_ENGINE.md)

The supplied implementation meets its 25 MHz timing constraints.
Physical-board gameplay and keyboard compatibility remain unverified.
The simulation tool limitations are listed in the verification document.

## License

[MIT](software/LICENSE).
