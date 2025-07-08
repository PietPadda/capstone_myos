/* kernel/linker.ld */
ENTRY(_start)

SECTIONS {
    . = 0x10000;

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

    /* This section prevents the "executable stack" warning */
    /DISCARD/ : {
        *(.note.GNU-stack)
    }
}