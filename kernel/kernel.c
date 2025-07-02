// myos/kernel/kernel.c

void main() {
    // The bootloader passes the VGA buffer address in EAX.
    volatile unsigned short* vga_buffer = (unsigned short*)0xB8000;

    // Define the message as a local char array.
    // This places the string data on the stack, which we know works,
    // bypassing any potential linker issues with data sections.
    char message[] = "Kernel Loaded! Success!";

    // --- Clear the screen ---
    for (int i = 0; i < 80 * 25; i++) {
        vga_buffer[i] = (unsigned short)' ' | 0x0F00;
    }


    // --- Print the message from the stack array ---
    int j = 0;
    while (message[j] != '\0') {
        vga_buffer[j] = (unsigned short)message[j] | 0x0F00;
        j++;
    }

    // Hang the CPU.
    while (1) {}
}