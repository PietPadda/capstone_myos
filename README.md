# MyOS: A 32-bit x86 Operating System Kernel

MyOS is a minimal, 32-bit, preemptive multitasking operating system kernel built from scratch as a capstone project for the Boot.dev curriculum. This project stands as a practical demonstration of fundamental computer science and systems programming skills, covering everything from the boot process to a basic command-line shell with process management and memory protection.

The goal of this project was to learn the core principles of operating system development by building a functional system from the ground up, with no reliance on a standard C library or existing operating system services.

## Features

- **Bootloader:** A two-stage bootloader that loads the kernel from a FAT12 formatted disk image using a modern LBA (Logical Block Addressing) method.
- **Kernel Core:** A 32-bit Protected Mode kernel that handles essential system initialization and manages the CPU's state.
- **Memory Management:**
  - **Physical Memory Manager (PMM):** A bitmap-based allocator that tracks and manages physical memory frames.
  - **Virtual Memory:** A two-level paging system with a recursive page directory trick, providing each user process with its own isolated virtual address space.
- **Process Management:**
  - **Preemptive Multitasking:** A round-robin scheduler that switches between tasks on every timer interrupt.
  - **Task States:** Processes can be in one of four states: running, sleeping, waiting, or zombie.
  - **Process Control:** The kernel manages the process lifecycle, including creating new processes and safely reaping zombie processes.
- **System Calls (Syscalls):** A `syscall` dispatch table for handling requests from user-mode programs.
  - `sys_exit`: A syscall that terminates a user program and safely returns control to the shell.
  - `sys_getchar`: A syscall that blocks until a key is pressed, providing a way for user programs to receive input.
  - `sys_print`: A syscall that prints a string from a user-mode program to the screen.
- **Drivers:**
  - **VGA Driver:** A text-mode driver that handles screen output, cursor management, backspace functionality, and scrolling.
  - **Keyboard Driver:** An interrupt-driven driver that uses a circular buffer to handle input and supports the Shift key.
  - **ATA Driver:** A low-level disk driver using PIO (Programmed I/O) to read raw sectors from a hard disk.
- **Command-Line Interface (CLI):** A simple shell that supports a variety of commands, including:
  - `help`: Lists all available commands.
  - `ls`: Lists files in the root directory by parsing the FAT12 filesystem.
  - `cat`: Reads and displays the contents of a file.
  - `ps`: Shows a dynamic list of currently running processes.
  - `run`: Loads and executes a user-mode program from the disk.
  - `reboot`: Reboots the system by sending a command to the keyboard controller.

## Architecture

MyOS follows a classic monolithic kernel architecture. The core design principles are:

- **Freestanding Environment:** The entire kernel and its subsystems are written from scratch without the use of a standard C library.
- **Kernel-Space and User-Space:** The OS enforces a clear separation between the kernel (Ring 0) and user programs (Ring 3), with syscalls acting as the sole, secure interface between them.
- **ELF Loader:** The kernel can load programs compiled in the Executable and Linkable Format (ELF), which is the standard executable format for Unix-like systems.

## Development Journey

The journey to building MyOS was a challenging but rewarding process of incremental development and meticulous debugging. Many of the most difficult bugs were subtle and exposed deep-seated issues in memory management and synchronization.

- **Initial Struggles:** Early challenges included `QEMU` and `GDB` setup, understanding the BIOS boot process, and correctly setting up the 16-bit to 32-bit mode switch.
- **Debugging Hurdles:** The project required solving a variety of classic bugs, including:
  - **Heisenbugs:** Bugs that changed behaviour or disappeared when debugging tools were added.
  - **Memory Conflicts:** The Physical Memory Manager and the heap allocator fighting for the same memory regions, causing crashes.
  - **Subtle Mismatches:** Off-by-one errors in pointer arithmetic, unaligned memory accesses, and incorrect `C` struct layouts.
  - **Race Conditions:** Critical sections in the kernel that were not protected from timer interrupts, leading to stack corruption and unpredictable behavior.
- **Final Solution:** The final, stable kernel was the result of a comprehensive process of building, breaking, and methodically fixing each layer of the system. The successful implementation of a context-switching scheduler and a page-based memory protection model was the ultimate achievement of this journey.

## How to Build and Run MyOS

To build and run MyOS, you need to have `git`, `make`, `gcc`, `nasm`, `qemu-system-i386`, `dosfstools`, and `mtools` installed on a Linux-like system.

1. Clone the repository:
   ```sh
   git clone [https://github.com/github-username/repo-name.git](https://github.com/github-username/repo-name.git)
   cd repo-name