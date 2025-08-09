// myos/userspace/programs/beep.c

#include <syscall.h>

void user_program_main() {
    // Start the tone
    syscall_play_sound(440);

    // Sleep for 200ms
    syscall_sleep(200);

    // Stop the tone (by playing a frequency of 0)
    syscall_play_sound(0);

    // Exit the program
    syscall_exit();
}