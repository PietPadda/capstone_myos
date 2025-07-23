/* myos/userspace/linker.ld */

ENTRY(_start_user_program)
OUTPUT_FORMAT(elf32-i386)

SECTIONS {
    /* Programs will be loaded at 1MB */
    . = 0x100000;

    .text : {
        *(.text)
    }

    .data : {
        *(.data)
    }

    .rodata : {
        *(.rodata)
    }

    .bss : {
        *(.bss)
    }
}