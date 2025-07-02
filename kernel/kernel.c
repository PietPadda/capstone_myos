// myos/kernel/kernel.c

void main() {
    // Directly define a pointer to the VGA text-mode buffer.
    volatile unsigned short* vga_buffer = (unsigned short*)0xB8000;

    // We'll write the character 'Y' with white text on a blue background.
    // This unique look ensures that if it appears, it's from this exact code.
    const unsigned short character = (unsigned short)'Y' | (0x1F << 8);

    // Write the character to the top-left corner of the screen (position 0).
    *vga_buffer = character;

    // Hang the CPU.
    while (1) {}
}