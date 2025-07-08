# myos/Makefile
# A robust and scalable Makefile for myos

# Tell Make that these are not real files
.PHONY: all clean run

# --- Variables ---
BUILD_DIR := build
QEMU_CMD := qemu-system-x86_64
QEMU_OPTS := -fda $(BUILD_DIR)/os_image.bin -debugcon stdio

# --- Source Files ---
# Automatically find all .c and .asm source files
BOOT_SRC := boot/bootloader.asm
KERNEL_ASM_SRC := $(wildcard kernel/*.asm)
KERNEL_C_SRC := $(wildcard kernel/*.c)

# --- Object Files ---
# Generate the list of .o files from the source files
BOOT_OBJ := $(BUILD_DIR)/bootloader.bin
KERNEL_ASM_OBJ := $(patsubst kernel/%.asm, $(BUILD_DIR)/%.o, $(KERNEL_ASM_SRC))
KERNEL_C_OBJ := $(patsubst kernel/%.c, $(BUILD_DIR)/%.o, $(KERNEL_C_SRC))
ALL_KERNEL_OBJS := $(KERNEL_ASM_OBJ) $(KERNEL_C_OBJ)

# --- Final Binaries ---
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
DISK_IMAGE := $(BUILD_DIR)/os_image.bin

# --- Toolchain Flags ---
# Use NASM for assembly. -F dwarf creates debug symbols.
ASM := nasm
ASM_FLAGS := -f elf32 -F dwarf
# Use GCC for C. -g adds debug symbols.
CC := gcc
CFLAGS := -m32 -ffreestanding -nostdlib -fno-pie -fno-stack-protector -g -c
# Use LD for linking
LD := ld
LDFLAGS := -m elf_i386 -T kernel/linker.ld
# Use objcopy for extracting the binary
OBJCOPY := objcopy
OBJCOPY_FLAGS := -O binary

# --- Build Targets ---

# Default target: build everything
all: $(DISK_IMAGE)

# Rule to create the final disk image from the bootloader and kernel binary
$(DISK_IMAGE): $(BOOT_OBJ) $(KERNEL_BIN)
	@echo "Creating disk image: $@"
	@dd if=/dev/zero of=$@ bs=512 count=2880 >/dev/null 2>&1
	@dd if=$(BOOT_OBJ) of=$@ bs=512 seek=0 conv=notrunc >/dev/null 2>&1
	@dd if=$(KERNEL_BIN) of=$@ bs=512 seek=1 conv=notrunc >/dev/null 2>&1

# Rule to link all kernel objects into a single ELF file
$(KERNEL_ELF): $(ALL_KERNEL_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^
	@echo "Kernel linked: $@"

# Rule to extract the raw binary from the ELF file
$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) $(OBJCOPY_FLAGS) $< $@
	@echo "Kernel extracted to binary: $@"
	@echo -n "Kernel size: "
	@stat -c %s $@

# Rule to build the bootloader (special case, not an ELF object)
$(BOOT_OBJ): $(BOOT_SRC)
	@mkdir -p $(BUILD_DIR)
	$(ASM) $< -f bin -o $@

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

# Rule to clean up all build artifacts
clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)