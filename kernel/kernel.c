// myos/kernel/kernel.c

// headers
#include <kernel/idt.h> // interrupt tables
#include <kernel/vga.h> // writing to screen
#include <kernel/io.h> // hardware comms
#include <kernel/keyboard.h> // Include our new keyboard driver header
#include <kernel/timer.h> // Include our new timer driver header
#include <kernel/shell.h> // shell for CLI
#include <kernel/memory.h> // heap memory allocation
#include <kernel/gdt.h> // kernel permanent GDT
#include <kernel/fs.h> // Filesystem driver for init_fs()
#include <kernel/cpu/tss.h> // Task-State Segment
#include <kernel/syscall.h> // User Mode Syscalls
#include <kernel/debug.h> // debug prints
#include <kernel/cpu/process.h> // process_init()
#include <kernel/pmm.h> // physical memory manager
#include <kernel/paging.h> // paging creator

// Make the global flag visible to kmain
extern volatile int multitasking_enabled;

// Let kmain know about the new assembly function
extern void start_multitasking(cpu_state_t* cpu_state);

// Tell this file about the process table defined in process.c
extern task_struct_t process_table[MAX_PROCESSES];

// Helper for debug prints
static inline void outb(unsigned short port, unsigned char data) {
    __asm__ __volatile__("outb %0, %1" : : "a"(data), "Nd"(port));
}

// This is our new dedicated idle task. It does nothing but halt.
void idle_task() {
    qemu_debug_string("idle_task: entered.\n");
    while (1) {
        __asm__ __volatile__("hlt");
    }
}

// This wrapper function will be the entry point for our shell process.
void shell_task() {
    qemu_debug_string("shell_task: entered.\n");
    shell_init();
    qemu_debug_string("shell_init ");
    shell_run();
    qemu_debug_string("shell_proc_loop ");
}

void kmain() {
    qemu_debug_string("KERNEL:\nkmain_start ");

    // Install the IDT first so we can catch any exceptions.
    // install our Interrupt Service Routines and load the IDT.
    idt_install();
    qemu_debug_string("idt_inst ");

    // Initialize the Physical Memory Manager.
    // We'll assume 16MB of RAM for now. (16 * 1024 * 1024 = 16777216)
    pmm_init(16777216);
    qemu_debug_string("pmm_init ");

    // PMM Sanity Check
    void* frame1 = pmm_alloc_frame();
    qemu_debug_string("PMM Test: Alloc frame 1 at ");
    qemu_debug_hex((uint32_t)frame1);
    qemu_debug_string("\n");

    void* frame2 = pmm_alloc_frame();
    qemu_debug_string("PMM Test: Alloc frame 2 at ");
    qemu_debug_hex((uint32_t)frame2);
    qemu_debug_string("\n");

    pmm_free_frame(frame1);
    qemu_debug_string("PMM Test: Freed frame 1\n");

    void* frame3 = pmm_alloc_frame();
    qemu_debug_string("PMM Test: Alloc frame 3 at ");
    qemu_debug_hex((uint32_t)frame3);
    qemu_debug_string(" (should be same as frame 1)\n");

    // initiate dynamic mem allocation
    // Modules like TSS and FS depend on malloc() being ready
    // Initialize our old heap allocator AFTER the PMM.
    init_memory(); 
    qemu_debug_string("mem_init ");

    // Initialize and enable paging
    // Must happen after PPM and Dynamic Heap
    paging_init();
    qemu_debug_string("paging_init ");

    // Install the kernel's GDT
    // Reload the GDT AFTER paging is enabled. This ensures the CPU
    // is using the correct descriptor table in the new memory map.
    gdt_install();
    qemu_debug_string("gdt_inst ");

    // Now, initialize the rest of the systems that depend on the GDT.
    // Install the TSS right after the GDT
    tss_install(); 
    qemu_debug_string("tss_inst ");

    // Initialize the process table BEFORE syscalls and interrupts
    process_init(); // Sets up idle_task and shell_task
    qemu_debug_string("proc_init ");

    // Syscall install after TSS
    syscall_install();
    qemu_debug_string("sysc_inst ");

    // Remap the PIC first to get the hardware into a stable state.
    pic_remap(0x20, 0x28); // Master PIC at 0x20, Slave at 0x28
    qemu_debug_string("pic_remap ");

    // Install the keyboard driver.
    keyboard_install();
    qemu_debug_string("keyb_inst ");

    // Install the timer driver.
    timer_install(); // Install our new timer driver
    qemu_debug_string("pit_inst ");

    // Initialize the filesystem driver
    init_fs();
    qemu_debug_string("fs_init ");

    // clear the bios text
    delay_ms(800); // Use our blocking delay before the scheduler is active.
    clear_screen();
    qemu_debug_string("clear_bios_scr ");

    // print the boot screen
    print_bootscreen();
    delay_ms(1800); // Use our blocking delay before the scheduler is active.
    qemu_debug_string("print_boot_scr ");
    clear_screen();

    // Enable interrupts! From this point on, the CPU will respond to hardware.
    __asm__ __volatile__ ("sti");
    qemu_debug_string("interrupt_enable ");

    // Open the gate for the scheduler right before starting.
    multitasking_enabled = 1;
    qemu_debug_string("multitasking_enabled ");

    // Start multitasking by jumping to the first task
    qemu_debug_string("kmain: Calling start_multitasking...\n");
    start_multitasking(&process_table[0].cpu_state);
    qemu_debug_string("kmain: Returned from start_multitasking (this is an error).\n");

    // This part of kmain should never be reached.
    while (1) {
        __asm__ __volatile__("hlt");
    }
}