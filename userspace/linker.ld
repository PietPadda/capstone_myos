/* myos/userspace/linker.ld */

ENTRY(_start_user_program)
OUTPUT_FORMAT(elf32-i386)

SECTIONS {
    /* Programs will now be loaded at 4MB */
    . = 0x400000;

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