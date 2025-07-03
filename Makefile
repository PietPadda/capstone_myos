# myos/Makefile
# This Makefile orchestrates the build process for our OS.

.PHONY: all clean run

# Define our build directory for object files and final binaries
BUILD_DIR := build

# --- Bootloader  ---
BOOT_ASM_SRC := boot/bootloader.asm

BOOT_BIN := $(BUILD_DIR)/bootloader.bin

# --- Kernel ---
KERNEL_ENTRY_SRC := kernel/kernel_entry.asm
KERNEL_C_SRC := kernel/kernel.c
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
KERNEL_OBJ_ENTRY := $(BUILD_DIR)/kernel_entry.o
KERNEL_OBJ_C := $(BUILD_DIR)/kernel.o
KERNEL_BIN := $(BUILD_DIR)/kernel.bin

# --- Disk Image & QEMU ---
DISK_IMAGE := $(BUILD_DIR)/os_image.bin
QEMU_CMD := qemu-system-x86_64
QEMU_OPTS := -fda $(DISK_IMAGE) -debugcon stdio

# --- Main build target ---
all: $(DISK_IMAGE)

# --- Rule for the bootloader ---
$(BOOT_BIN): $(BOOT_ASM_SRC)
	@mkdir -p $(BUILD_DIR)
	nasm $(BOOT_ASM_SRC) -f bin -o $(BOOT_BIN)
	@echo "Bootloader built: $(BOOT_BIN)"
	@if [ `stat -c %s $(BOOT_BIN)` -ne 512 ]; then \
		echo "ERROR: Bootloader is not 512 bytes! Size: `stat -c %s $(BOOT_BIN)` bytes"; \
		exit 1; \
	fi

# --- Rules for the kernel ---
# Assemble the kernel's assembly entry point
$(KERNEL_OBJ_ENTRY): $(KERNEL_ENTRY_SRC)
	nasm $(KERNEL_ENTRY_SRC) -f elf32 -o $(KERNEL_OBJ_ENTRY)

# Compile the kernel's C code
$(KERNEL_OBJ_C): $(KERNEL_C_SRC)
	gcc -m32 -ffreestanding -nostdlib -fno-pie -fno-stack-protector -nodefaultlibs \
        -c $(KERNEL_C_SRC) -o $(KERNEL_OBJ_C)

# Link the kernel objects into the final kernel binary
$(KERNEL_BIN): $(KERNEL_OBJ_ENTRY) $(KERNEL_OBJ_C)
	ld -m elf_i386 -Ttext 0x10000 --oformat binary \
		--entry=_start \
		$(KERNEL_OBJ_ENTRY) $(KERNEL_OBJ_C) -o $(KERNEL_BIN)
	@echo "Kernel built: $(KERNEL_BIN)"
	@echo -n "Kernel size: "
	@stat -c %s $(KERNEL_BIN)

# --- Rule for the final disk image ---
$(DISK_IMAGE): $(BOOT_BIN) $(KERNEL_BIN)
	@echo "Creating disk image: $(DISK_IMAGE)"
	@dd if=/dev/zero of=$(DISK_IMAGE) bs=512 count=2880 >/dev/null 2>&1
	@dd if=$(BOOT_BIN) of=$(DISK_IMAGE) bs=512 seek=0 conv=notrunc >/dev/null 2>&1
	@dd if=$(KERNEL_BIN) of=$(DISK_IMAGE) bs=512 seek=1 conv=notrunc >/dev/null 2>&1

# --- Rule to run QEMU ---
run: $(DISK_IMAGE)
	@echo "Running OS in QEMU..."
	$(QEMU_CMD) $(QEMU_OPTS)

# --- Rule to clean artifacts ---
clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)