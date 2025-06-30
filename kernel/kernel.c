// myos/kernel/kernel.c

// The main function of our kernel.
// This is where the kernel's execution will begin after the bootloader
// hands over control in 32-bit Protected Mode.
void main() {
    // We are now in C code!

    // Define the VGA text buffer address (same as 0xB8000 in assembly)
    // This is a pointer to a 16-bit unsigned integer (char + attribute)
    unsigned short* vga_buffer = (unsigned short*)0xB8000;

    // Define a simple message
    const char* message = "Kernel Loaded!";

    // Loop through the message and print it to the screen
    int i = 0;
    while (message[i] != '\0') {
        // Each character on screen takes 2 bytes:
        // 1st byte: ASCII character
        // 2nd byte: Color attribute (0x0F = white text on black background)
        vga_buffer[i] = (unsigned short)message[i] | 0x0F00; // Combine char with attribute
        i++;
    }

    // Hang forever (prevent the CPU from executing random memory)
    // In a real OS, this would be the main loop of the kernel.
    while (1) {
        // Do nothing, just loop
    }
}