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

# --- QEMU specific variables ---
# Command to run QEMU for a 64-bit x86 system
QEMU_CMD := qemu-system-x86_64
# QEMU options: -fda treats our .bin as a floppy disk;
QEMU_OPTS := -fda $(BOOT_BIN)

# --- Default target: Builds the bootloader ---
all: $(BOOT_BIN)

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

# --- Rule to run the bootloader in QEMU ---
run: $(BOOT_BIN)
	@echo "Running bootloader in QEMU..."
	$(QEMU_CMD) $(QEMU_OPTS)

# --- Rule to clean up build artifacts ---
clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)/*.bin $(BUILD_DIR)/*.o # Remove all .bin and .o files from build dir