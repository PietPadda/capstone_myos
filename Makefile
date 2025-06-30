# myos/Makefile
# This Makefile orchestrates the build process for our OS.

.PHONY: all clean run

# Define our build directory for object files and final binaries
BUILD_DIR := build

# --- Bootloader specific variables ---
# Source assembly file for the bootloader
BOOT_ASM_SRC := boot/bootloader.asm
# Output binary for the bootloader (must be 512 bytes)
BOOT_BIN := $(BUILD_DIR)/bootloader.bin

# --- Kernel specific variables ---
# Source C file for the kernel
KERNEL_C_SRC := kernel/kernel.c
# Output binary for the kernel
KERNEL_BIN := $(BUILD_DIR)/kernel.bin

# --- QEMU specific variables ---
# Command to run QEMU for a 64-bit x86 system
QEMU_CMD := qemu-system-x86_64
# QEMU options: -fda treats our .bin as a floppy disk;
# For now, we only boot the bootloader. Later, we'll combine them.
QEMU_OPTS := -fda $(BOOT_BIN)

# --- Default target: Builds both the bootloader and kernel (but not linked yet) ---
all: $(BOOT_BIN) $(KERNEL_BIN) # Add KERNEL_BIN to the 'all' target

# --- Rule to build the bootloader binary (.bin) from its assembly source (.asm) ---
$(BOOT_BIN): $(BOOT_ASM_SRC)
	@mkdir -p $(BUILD_DIR) # Ensure the build directory exists before compiling
	nasm $(BOOT_ASM_SRC) -f bin -o $(BOOT_BIN) # Assemble the bootloader
	@echo "Bootloader built: $(BOOT_BIN)"
	@# Critical check: Ensure the assembled bootloader is exactly 512 bytes
	@if [ `stat -c %s $(BOOT_BIN)` -ne 512 ]; then \
		echo "ERROR: Bootloader is not 512 bytes! Size: `stat -c %s $(BOOT_BIN)` bytes"; \
		exit 1; \
	fi

# --- Rule to build the kernel binary (.bin) from its C source (.c) ---
$(KERNEL_BIN): $(KERNEL_C_SRC)
	@mkdir -p $(BUILD_DIR) # Ensure the build directory exists
	# Compile C source to object file, then link into raw binary.
	# -m32: Compile for 32-bit (Protected Mode is 32-bit)
	# -ffreestanding: No standard library (we don't have one yet)
	# -nostdlib: Don't link standard libraries
	# -fno-pie: Don't generate Position-Independent Executable (important for bare metal)
	# -fno-stack-protector: Disable stack protector (simpler for bare metal)
	# -nodefaultlibs: Don't link default libs
	# -lgcc: Manually link GCC support library (needed for some built-ins)
	# -Ttext=0x10000: Set entry point of kernel to a specific memory address (1MB for kernel)
	gcc -m32 -ffreestanding -nostdlib -fno-pie -fno-stack-protector -nodefaultlibs \
		-c $(KERNEL_C_SRC) -o $(BUILD_DIR)/kernel.o
	ld -m elf_i386 -Ttext 0x10000 --oformat binary \
		$(BUILD_DIR)/kernel.o -o $(KERNEL_BIN)
	@echo "Kernel built: $(KERNEL_BIN)"

# --- Rule to run the bootloader in QEMU ---
run: $(BOOT_BIN)
	@echo "Running bootloader in QEMU..."
	$(QEMU_CMD) $(QEMU_OPTS)

# --- Rule to clean up build artifacts ---
clean:
	@echo "Cleaning build directory..."
	# Remove all .bin and .o files from build dir
	rm -rf $(BUILD_DIR)/*.bin $(BUILD_DIR)/*.o 