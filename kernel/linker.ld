/* myos/kernel/linker.ld */
ENTRY(_start)

SECTIONS {
    . = 0x10000; /* Kernel starts at 1MB */

    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    bss_start = .;
    .bss : {
        *(.bss)
    }
    bss_end = .;

    /* This section prevents the "executable stack" warning and cleans up output */
    /DISCARD/ : {
        *(.note.GNU-stack)
        *(.eh_frame)
    }
}