OUTPUT_ARCH(riscv)
ENTRY(_start)
MEMORY {
    ROM (rx) : ORIGIN = 0x00000000, LENGTH = 16K
    RAM (rw) : ORIGIN = 0x00010000, LENGTH = 4K
}
SECTIONS {
    .text : { KEEP(*(.text.start)) *(.text .text.*) } > ROM
    .rodata : { . = ALIGN(4); *(.rodata .rodata.* .srodata .srodata.*) . = ALIGN(4); } > RAM
    .data : { *(.data .data.* .sdata .sdata.*) } > RAM
    .bss (NOLOAD) : {
        . = ALIGN(4); __bss_start = .;
        *(.bss .bss.* .sbss .sbss.*) *(COMMON)
        . = ALIGN(4); __bss_end = .;
    } > RAM
    __stack_top = ORIGIN(RAM) + LENGTH(RAM);
    __stack_floor = __bss_end;
    /DISCARD/ : { *(.comment .eh_frame .eh_frame.* .note .note.*) }
    ASSERT(_start == ORIGIN(ROM), "CPU reset PC must be zero")
    ASSERT(SIZEOF(.text) <= LENGTH(ROM), "Instruction ROM overflow")
    ASSERT(SIZEOF(.data) == 0, "Mutable initialized globals are forbidden: initialize in C after BSS reset")
    ASSERT(__bss_end <= __stack_top - 1536, "Less than 1536 bytes of stack remain")
}
