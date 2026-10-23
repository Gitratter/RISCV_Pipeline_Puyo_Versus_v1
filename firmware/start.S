    .section .text.start,"ax",@progbits
    .globl _start
    .option norvc
    .option norelax
_start:
    la sp, __stack_top
    la t0, __bss_start
    la t1, __bss_end
1:  bgeu t0, t1, 2f
    sw zero, 0(t0)
    addi t0, t0, 4
    j 1b
2:  call main
    # Test firmware returns a unique assertion ID, or zero on success.
    # Game firmware never returns. No ECALL, CSR, M, C or privileged instructions.
    li t0, 0x2054
    sw a0, 0(t0)
3:  j 3b
