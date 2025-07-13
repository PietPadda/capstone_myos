# myos/Makefile
# A robust and scalable Makefile for myos

# Tell Make that these are not real files
.PHONY: all clean run

# --- Variables ---
BUILD_DIR := build
QEMU_CMD := qemu-system-i386
QEMU_OPTS := -fda $(BUILD_DIR)/os_image.bin -debugcon stdio

# --- Source Files ---
BOOT_SRC := boot/bootloader.asm
# Separate kernel_entry.asm so we can control its link order
KERNEL_ENTRY_SRC := kernel/kernel_entry.asm
KERNEL_ASM_SRC := $(filter-out $(KERNEL_ENTRY_SRC), $(wildcard kernel/*.asm))
KERNEL_C_SRC := $(wildcard kernel/*.c)

# --- Object Files ---
BOOT_OBJ := $(BUILD_DIR)/bootloader.bin
KERNEL_ENTRY_OBJ := $(patsubst kernel/%.asm, $(BUILD_DIR)/%.o, $(KERNEL_ENTRY_SRC))
KERNEL_ASM_OBJ := $(patsubst kernel/%.asm, $(BUILD_DIR)/%.o, $(KERNEL_ASM_SRC))
KERNEL_C_OBJ := $(patsubst kernel/%.c, $(BUILD_DIR)/%.o, $(KERNEL_C_SRC))
ALL_KERNEL_OBJS := $(KERNEL_ASM_OBJ) $(KERNEL_C_OBJ)

# --- Final Binaries ---
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
DISK_IMAGE := $(BUILD_DIR)/os_image.bin

# --- Toolchain Flags ---
ASM := nasm
ASM_FLAGS := -f elf32 -F dwarf
CC := gcc
CFLAGS := -m32 -ffreestanding -nostdlib -fno-pie -fno-stack-protector -g -c
LD := ld
LDFLAGS := -m elf_i386 -T kernel/linker.ld
OBJCOPY := objcopy
OBJCOPY_FLAGS := -O binary

# --- Build Targets ---

# Default target: build everything
all: $(DISK_IMAGE)

# Rule to create the final disk image
$(DISK_IMAGE): $(BOOT_OBJ) $(KERNEL_BIN)
	@echo "Creating disk image: $@"
	@dd if=/dev/zero of=$@ bs=512 count=2880 >/dev/null 2>&1
	@dd if=$(BOOT_OBJ) of=$@ bs=512 seek=0 conv=notrunc >/dev/null 2>&1
	@dd if=$(KERNEL_BIN) of=$@ bs=512 seek=1 conv=notrunc >/dev/null 2>&1

# Rule to link all kernel objects into a single ELF file
# KERNEL_ENTRY_OBJ is listed first to ensure correct linking.
$(KERNEL_ELF): $(KERNEL_ENTRY_OBJ) $(ALL_KERNEL_OBJS)
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

# Rule to start debugger
debug: all
	@echo "--- Starting QEMU, waiting for GDB on localhost:1234 ---"
	qemu-system-i386 -fda build/os_image.bin -debugcon stdio -S -gdb tcp::1234