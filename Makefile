# myos/Makefile
# A more robust and scalable Makefile for myos

.PHONY: all clean run

BUILD_DIR := build

# --- Source Files ---
# Automatically find all .c and .asm source files in the specified directories
BOOT_SRC := boot/bootloader.asm
KERNEL_ASM_SRC := $(wildcard kernel/*.asm)
KERNEL_C_SRC := $(wildcard kernel/*.c)

# --- Object Files ---
# Automatically generate the list of .o files from the source files
BOOT_OBJ := $(BUILD_DIR)/bootloader.bin
KERNEL_ASM_OBJ := $(patsubst kernel/%.asm, $(BUILD_DIR)/%.o, $(KERNEL_ASM_SRC))
KERNEL_C_OBJ := $(patsubst kernel/%.c, $(BUILD_DIR)/%.o, $(KERNEL_C_SRC))

# --- Final Binaries ---
KERNEL_BIN := $(BUILD_DIR)/kernel.bin

# --- Disk Image & QEMU ---
DISK_IMAGE := $(BUILD_DIR)/os_image.bin
QEMU_CMD := qemu-system-x86_64
QEMU_OPTS := -fda $(DISK_IMAGE) -debugcon stdio

# --- Compiler/Linker Flags ---
# Use NASM for assembly
ASM := nasm
ASM_FLAGS := -f elf32
# Use GCC for C
CC := gcc
CFLAGS := -m32 -ffreestanding -nostdlib -fno-pie -fno-stack-protector -nodefaultlibs -c
# Use LD for linking
LD := ld
LDFLAGS := -m elf_i386 -Ttext 0x10000 --oformat binary

# --- Build Targets ---

# Default target
all: $(DISK_IMAGE)

# --- Rule for the final disk image ---
$(DISK_IMAGE): $(BOOT_OBJ) $(KERNEL_BIN)
	@echo "Creating disk image: $@"
	
	# ADD THESE DEBUG LINES
	@echo "--- Debugging BOOT_OBJ variable ---"
	@echo "BOOT_OBJ is: [$(BOOT_OBJ)]"

	@echo "--- Running dd (step 1: create empty disk) ---"
	dd if=/dev/zero of=$@ bs=512 count=2880
	@echo "--- Running dd (step 2: copy bootloader) ---"
	dd if=$(BOOT_OBJ) of=$@ bs=512 seek=0 conv=notrunc
	@echo "--- Running dd (step 3: copy kernel) ---"
	dd if=$(KERNEL_BIN) of=$@ bs=512 seek=1 conv=notrunc

# Rule to build the bootloader
$(BUILD_DIR)/%.bin: boot/%.asm
	@mkdir -p $(BUILD_DIR)
	$(ASM) $< -f bin -o $@

# Rule to link the kernel
$(KERNEL_BIN): $(KERNEL_ASM_OBJ) $(KERNEL_C_OBJ)
	$(LD) $(LDFLAGS) --entry=_start $^ -o $@
	@echo "Kernel built: $@"

# Generic rule to compile any C source file to an object file
$(BUILD_DIR)/%.o: kernel/%.c
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@

# Generic rule to assemble any kernel assembly file to an object file
$(BUILD_DIR)/%.o: kernel/%.asm
	@mkdir -p $(BUILD_DIR)
	$(ASM) $(ASM_FLAGS) $< -o $@

# Rule to run QEMU
run: $(DISK_IMAGE)
	@echo "Running OS in QEMU..."
	$(QEMU_CMD) $(QEMU_OPTS)

# Rule to clean up
clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)