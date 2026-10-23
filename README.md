# RISC-V Pipeline Puyo Versus v1

A single-player and two-player falling-pair puzzle game for the Digilent Basys 3.
An RV32I pipelined CPU runs the game firmware. Verilog implements the CPU,
memory, input interfaces, display registers, and VGA scanout.

## Hardware

| Item | Configuration |
|---|---|
| Board | Digilent Basys 3 |
| FPGA | xc7a35tcpg236-1 |
| Development tool | Vivado 2025.2 |
| Board clock | 100 MHz |
| CPU and pixel clock | 25 MHz |
| Display | 640 x 480 VGA, approximately 59.52 Hz |
| Player 2 input | USB keyboard connected directly to the J2 USB Type-A port |

## Build and program

This repository contains HDL, constraints, firmware, memory images, and tests.
It does not contain a Vivado project file or a prebuilt bitstream.

Create an RTL project for `xc7a35tcpg236-1`, add the files from
`sources_1/new/`, and add `constrs_1/new/Basys3_Puyo_Versus.xdc` as the
constraint file. Set the design top to `RISCV_PUYO_VERSUS_TOP`.

Run **Synthesis**, **Implementation**, and **Generate Bitstream**, then use
Hardware Manager to program the Basys 3 with the resulting bitstream.
The supplied production MEM files can be used without compiling C.

See [Build and test setup](docs/BUILD.md) for source registration, simulation
settings, and firmware-image requirements.

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
USB hubs are not supported. See [Keyboard interface](docs/KEYBOARD.md).

## Gameplay

Each player has a 6 x 12 visible field, four colors, and a two-piece active pair.
Groups of at least four orthogonally connected pieces clear together. Gravity
and repeated clears produce chains.

Versus mode uses identical piece sequences for both players, independent of
placement speed. Attacks first cancel pending incoming garbage. Remaining
attacks are sent after the chain ends. At most 30 garbage pieces drop at the
end of one turn.

These are simplified rules, not a complete reproduction of the original game.
See [Game rules](docs/RULES_AND_TESTS.md).

## Repository layout

| Path | Contents |
|---|---|
| `sources_1/new/` | Synthesizable HDL and production memory images |
| `constrs_1/new/` | Basys 3 pin and timing constraints |
| `sim_1/new/` | RTL testbenches, include file, and test memory images |
| `firmware/` | RV32I C source, startup, linker script, and memory images |
| `tests/` | Native/RV32I game regression source and test descriptions |
| `docs/` | Architecture, rules, interfaces, and build documentation |

The repository uses these top-level directories directly; it has no
`software/`, `programming/`, or build-script directory.

## Documentation

- [Build and test setup](docs/BUILD.md)
- [Architecture and memory map](docs/ARCHITECTURE.md)
- [Game rules](docs/RULES_AND_TESTS.md)
- [Keyboard interface](docs/KEYBOARD.md)
- [Video interface](docs/VIDEO.md)
- [Verification status and limitations](docs/VERIFICATION.md)
- [Game engine API](firmware/GAME_ENGINE.md)
- [Single-player tests](tests/README.md)
- [Versus tests](tests/README_VERSUS.md)

The implementation meets its configured 25 MHz timing constraints.
Physical-board gameplay and keyboard compatibility remain unverified.
The simulation tool limitations are listed in the verification document.

## License

[MIT](LICENSE).
