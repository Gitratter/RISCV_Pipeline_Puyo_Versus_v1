# Build and Test Setup

All paths and commands in this document are relative to the repository root.
The repository contains sources and memory images, but no `.xpr`, `.bit`,
`software/` directory, or automated build scripts.

## Vivado project

Use Vivado 2025.2 and select the Basys 3 FPGA part `xc7a35tcpg236-1`.

1. Create an RTL project in a short path outside the source directories.
2. Add all `.v` files in `sources_1/new/` as design sources.
3. Add the four `.mem` files in `sources_1/new/` as memory initialization files.
4. Add `constrs_1/new/Basys3_Puyo_Versus.xdc` as the constraint file.
5. Set the design top to `RISCV_PUYO_VERSUS_TOP`.
6. Leave the synthesis Verilog definitions empty; do not define `SIMULATION`.
7. Run synthesis, implementation, and bitstream generation.
8. In Hardware Manager, connect to the Basys 3 and program the generated bitstream.

Enable post-placement physical optimization in the implementation run to
match the implementation flow used for the reported timing results. Check
setup and hold timing again after each build.

Keep the memory images registered in the project rather than relying on the
shell's working directory. Retain their filenames: the RTL uses them as
initialization-file parameters. Register only one copy of each basename;
the copies under `firmware/` should not be added as duplicate design files.

The versus top uses `puyo_versus.mem` and `puyo_versus_data.mem`. This image
supports both selectable game modes. The `puyo_game` images serve the
standalone system and its regression tests.

## Simulation sources

1. Add `.v` testbenches from `sim_1/new/` and `sim_1/new/cpu_regression/`
   to the simulation fileset only.
2. Add `sim_1/new/rv32i_encode.vh` and include `sim_1/new/` in the simulation
   include directories.
3. Add the eight `.mem` files from `sim_1/new/` as simulation-only memory files.
4. Set the testbench file type to SystemVerilog and define `SIMULATION` in
   the simulation fileset only.
5. Select a testbench top, such as `tb_versus_system`, and run until `$finish`.

| Testbench | Purpose |
|---|---|
| `tb_versus_system` | CPU, board input, PS/2 input, match flow, and display publication |
| `tb_keyboard` | Serial keyboard protocol and held-key states |
| `tb_versus_mmio` | Player banks, input reads, and atomic display commit |
| `tb_versus_video` | Two-player rendering and single-player compatibility |
| `tb_rv32_game_tests` | Single-player C suite on the CPU |
| `tb_rv32_versus_tests` | Versus C suite on the CPU |

The integration firmware uses a 50 ms countdown for testing. Production
firmware uses three seconds. Do not substitute integration MEM files when
generating the production bitstream.

See [Verification](VERIFICATION.md) for the unresolved Icarus compiler and
XSim elaboration issues. Source registration alone does not resolve them.

## Native C tests

Use a native GCC compiler, not the RISC-V cross-compiler. Run from the
repository root:

```powershell
gcc -std=c99 -O2 -Wall -Wextra -Werror -DHOST_TEST firmware/game.c tests/game_tests.c -o game_tests.exe
./game_tests.exe

gcc -std=c99 -O2 -Wall -Wextra -Werror -DHOST_TEST firmware/game.c firmware/match.c tests/versus_tests.c -o versus_tests.exe
./versus_tests.exe
```

These execute the same game source natively. They do not simulate the CPU or
program the FPGA. Expected success counts are 26 single-player scenarios
and 32 versus scenarios. Each test program returns a nonzero exit code on failure.

## Firmware and memory images

The production firmware source consists of `firmware/start.S`, `runtime.c`,
`game.c`, `match.c`, and `versus_main.c`, linked with `firmware/link.ld`.
It targets bare-metal RV32I with the ILP32 ABI and does not require the M or C
extensions.

Changing C source does not update the FPGA automatically. The current
repository does not include the ELF-to-MEM image-generation tool. The supplied
MEM files are usable as-is, but a reproducible firmware rebuild requires that
tool or an equivalent converter in addition to the RISC-V compiler.

The converter must follow the Harvard memory layout:

- Instruction image: 4,096 words, mapped from address 0, containing executable
  sections. Fill unused words with the RV32I NOP `00000013`.
- Data image: 1,024 words, mapped from address `0x10000`, containing read-only
  data and zero-initialized storage. Startup code also clears BSS on reset.
- Both images are text files with one eight-digit hexadecimal 32-bit word
  per line, assembled from little-endian bytes.
- A single raw binary copied into both memories is not a valid conversion.

After regenerating production firmware, update both `puyo_versus` images in
`firmware/` and their registered copies in `sources_1/new/`. If test firmware
changes, update its corresponding images in `firmware/` and `sim_1/new/`.
Then rerun the relevant tests, synthesis, implementation, and bitstream
generation. Native C test success does not establish that a stale MEM image
contains the modified source.

The [architecture](ARCHITECTURE.md) describes memory and MMIO details, and
the [game API](../firmware/GAME_ENGINE.md) describes configuration and test hooks.
