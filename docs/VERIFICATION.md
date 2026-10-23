# Verification

## Status

| Area | Status |
|---|---|
| Native C tests | 26 single-player and 32 versus scenarios pass |
| Same C tests on the RV32I RTL | 26 single-player and 32 versus scenarios pass |
| RTL testbench execution | 22 benches pass their assertions; compiler limitation below |
| Vivado synthesis, place and route | Complete |
| Bitstream generation | Complete |
| Physical-board gameplay | Not verified |
| USB keyboard and monitor compatibility | Not verified |
| XSim simulation completion | Not verified; elaboration failure below |

These results apply to the supplied implementation and memory images.
Test cycle totals are not measurements of per-frame latency or worst-case
game-update execution time. Integration fixtures shorten timers and cannot
be used as physical response-time measurements.

## Test coverage

The 22 RTL benches cover twelve CPU regression groups, keyboard reception,
two C test images, standalone MMIO, standalone restart and system behavior,
versus MMIO and system behavior, and two video renderers.

The versus integration test covers PS/2 input, board buttons, simultaneous
key states, rotation-repeat suppression, held keys at start, matching NEXT
pairs, mode selection, start/restart behavior, single-player operation, and
CPU halt/resume. Halting the CPU stops both game updates while hardware
clocks continue.

Detailed C scenarios are listed in [Single-player tests](../tests/README.md)
and [Versus tests](../tests/README_VERSUS.md).

## Simulation limitations

The Windows Icarus Verilog 14.0 development build exits with access-violation
code `3221225477` after reporting code-generation completion for all 22
benches. Clean compiler completion is therefore 0/22.

Freshly generated VVP outputs execute with exit code 0 and pass all
assertions. This does not establish a clean end-to-end compiler run.
The test runner is strict by default. Its diagnostic option accepts only
this exit code with a completion marker and a newly generated output, and
continues to record the compiler failure.

Vivado `xvlog` source analysis succeeds. `xelab` fails while compiling
generated `xsim_2.c`, with errors `XSIM 43-3409` and `43-3915`.
XSim simulation completion is not confirmed.

## FPGA implementation

Target: Basys 3, `xc7a35tcpg236-1`, Vivado 2025.2.
The 100 MHz input is converted to a 25 MHz CPU/peripheral/pixel clock.

| Metric | Result |
|---|---:|
| Slice LUTs | 5,241 / 20,800 |
| LUTs used as memory | 44, included in the LUT total |
| Slice registers | 4,295 / 41,600 |
| RAMB36 | 5 / 50 |
| DSP | 0 |
| WNS | +8.576 ns |
| TNS | 0 ns |
| WHS | +0.025 ns |
| THS | 0 ns |
| DRC violations | 0 |

Internal unconstrained endpoints are zero. Asynchronous buttons, switches,
and PS/2 inputs have timing exceptions at the synchronizer boundary.

Methodology warnings remain: one LUTAR-1 warning for the reset path, two
SYNTH-6 warnings for BRAM output registers, and three XDCB-5 query-efficiency
warnings. Zero DRC violations does not mean zero warnings.

The configured operating frequency is 25 MHz. Timing slack is not a measured
maximum operating frequency. The power report is a vectorless estimate,
not a measured board power value.

## Hardware checks

Before relying on the implementation in hardware:

1. Check single-player controls, NEXT, drop timing, and restart.
2. Connect a keyboard directly to J2 and check make/break behavior.
3. Check simultaneous movement, rotation, and soft drop.
4. Check held keys through countdown and rematch.
5. Check garbage, cancellation, carryover, overflow, and results.
6. Check that LED15, the CPU fault indicator, remains off.

Keyboard hot-plug behavior, USB device compatibility, the PS/2 timeout
margin, monitor compatibility, input latency, and worst-case execution time
remain hardware validation items.
