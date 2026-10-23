# Firmware and Build Tools

The firmware runs on the RV32I CPU in the Vivado project. It owns both game
boards and implements movement, collision detection, gravity, connected-group
search, clearing, scoring, attacks, cancellation, and match results.

## Source layout

| File | Purpose |
|---|---|
| `firmware/game.c`, `game.h` | Shared per-player game logic |
| `firmware/match.c`, `match.h` | Match state, attack arbitration, garbage reservations, and results |
| `firmware/versus_main.c` | Input polling, game updates, and display publication |
| `firmware/main.c` | Standalone single-player firmware used by regression tests |
| `firmware/start.S`, `link.ld`, `runtime.c` | Startup, memory layout, and runtime support |
| `tests/game_tests.c` | 26 single-player scenarios |
| `tests/versus_tests.c` | 32 versus scenarios |

## Build the firmware

Run the following from the package root. Python 3 and a bare-metal RISC-V GCC
toolchain are required.

```powershell
python software/scripts/build_firmware.py --variant all --gcc "C:/AMDDesignTools/2025.2/gnu/riscv/nt/bin/riscv64-unknown-elf-gcc.exe"
```

Adjust the compiler path for the local installation. The build uses
`-march=rv32i -mabi=ilp32`, checks executable instructions, and creates separate
instruction and data images. Images are copied into the registered Vivado
source directories automatically.

| Variant | Use |
|---|---|
| `game` | Standalone single-player regression firmware |
| `tests` | Single-player C test suite |
| `integration` | Single-player integration fixture |
| `versus` | Production firmware for both selectable modes |
| `versus_tests` | Versus C test suite |
| `versus_integration` | Integration fixture with a 50 ms countdown |

Production firmware uses a three-second countdown. The shortened countdown
applies only to the integration fixture.

The `.mem` files contain machine instructions and initialized RAM data, not
sprites. The CPU loads them through its instruction and data memories.
After changing C code, rebuild the images and rerun synthesis, implementation,
and bitstream generation. The prebuilt bitstream in `programming/` is not
replaced automatically.

## Host tests

```powershell
python software/scripts/run_versus_host_tests.py --gcc "C:/AMDDesignTools/2025.2/tps/mingw/10.0.0/win64.o/nt/bin/gcc.exe"
```

This runs both C suites using a native compiler. Test descriptions are in
[Single-player tests](tests/README.md) and [Versus tests](tests/README_VERSUS.md).

## RTL tests

The default Vivado simulation top is `tb_versus_system`. The simulation fileset
defines `SIMULATION`; the synthesis fileset must not define it.

The Icarus runner supports all testbenches or a selected subset:

```powershell
python software/scripts/run_sim_tests.py --iverilog "C:/tools/iverilog/bin/iverilog.exe" --vvp "C:/tools/iverilog/bin/vvp.exe" --ivl-dir "C:/tools/iverilog/lib/ivl"
```

Use `--tests tb_keyboard tb_versus_mmio` to select individual benches.
The runner rejects compilation failures by default. The optional
`--diagnostic-compiler` permits only the known post-code-generation Windows
access violation, requires a fresh output file, and still records the compiler
failure. See [Verification](docs/VERIFICATION.md) before using that option.

## Hardware build

Open the XPR in Vivado, or run:

```powershell
./software/scripts/run_vivado.ps1 -VivadoRoot "C:/AMDDesignTools/2025.2/Vivado"
```

The batch flow writes reports and a bitstream to `software/build/vivado/`.
It withholds the bitstream if setup or hold timing fails.

## References

- [System architecture](docs/ARCHITECTURE.md)
- [Game engine API](firmware/GAME_ENGINE.md)
- [Rules and cancellation](docs/RULES_AND_TESTS.md)
