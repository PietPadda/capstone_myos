# myos/Makefile

# --- Variables ---
BUILD_DIR := build
QEMU_CMD := qemu-system-i386
QEMU_OPTS := -hda $(BUILD_DIR)/os_image.bin -debugcon stdio

# --- Source Files ---
# Find all .c and .asm files within the kernel directory and its subdirectories
KERNEL_SRC_DIRS := $(shell find kernel -type d)
KERNEL_C_SRC    := $(foreach dir,$(KERNEL_SRC_DIRS),$(wildcard $(dir)/*.c))
KERNEL_ASM_SRC  := $(foreach dir,$(KERNEL_SRC_DIRS),$(wildcard $(dir)/*.asm))
# Manually specify bootloader sources
STAGE1_SRC := boot/stage1.asm
STAGE2_SRC := boot/stage2.asm

# --- Object Files ---
# Map source files to object files in the build directory, preserving paths
STAGE1_OBJ := $(BUILD_DIR)/boot/stage1.bin
STAGE2_OBJ := $(BUILD_DIR)/boot/stage2.bin
KERNEL_C_OBJ   := $(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_C_SRC))
KERNEL_ASM_OBJ := $(patsubst %.asm,$(BUILD_DIR)/%.o,$(KERNEL_ASM_SRC))
ALL_KERNEL_OBJS := $(KERNEL_C_OBJ) $(KERNEL_ASM_OBJ)

# --- Final Binaries ---
KERNEL_ELF := $(BUILD_DIR)/kernel.elf
KERNEL_BIN := $(BUILD_DIR)/kernel.bin
DISK_IMAGE := $(BUILD_DIR)/os_image.bin

# --- Toolchain Flags ---
ASM := nasm
ASM_FLAGS := -f elf32 -F dwarf
CC := gcc
CFLAGS := -m32 -ffreestanding -nostdlib -fno-pie -fno-stack-protector -g -c -Iinclude
LD := ld
LDFLAGS := -m elf_i386 -T kernel/linker.ld -nostdlib
OBJCOPY := objcopy

# --- Build Targets ---
.PHONY: all clean run debug

# Default target: build everything
all: $(DISK_IMAGE)

# Rule to create the final disk image
$(DISK_IMAGE): $(STAGE1_OBJ) $(STAGE2_OBJ) $(KERNEL_BIN)
	@echo "--> Creating blank disk image..."
	dd if=/dev/zero of=$@ bs=512 count=2880 >/dev/null 2>&1
	@echo "--> Formatting disk with FAT12..."
	mkfs.fat -F 12 $@ >/dev/null 2>&1
	@echo "--> Installing Stage 1 bootloader..."
	dd if=$(STAGE1_OBJ) of=$@ conv=notrunc >/dev/null 2>&1
	@echo "--> Copying test file to disk image..."
	mcopy -i $@ test_files/test.txt ::
	@echo "--> Installing Stage 2 bootloader..."
	dd if=$(STAGE2_OBJ) of=$@ seek=1 conv=notrunc >/dev/null 2>&1
	@echo "--> Installing kernel..."
	dd if=$(KERNEL_BIN) of=$@ seek=5 conv=notrunc >/dev/null 2>&1

# Rule to link all kernel objects into a single ELF file
# Note: We must ensure kernel_entry.o is first in the link order.
KERNEL_ENTRY_OBJ := $(BUILD_DIR)/kernel/kernel_entry.o
OTHER_KERNEL_OBJS := $(filter-out $(KERNEL_ENTRY_OBJ), $(ALL_KERNEL_OBJS))
$(KERNEL_ELF): $(KERNEL_ENTRY_OBJ) $(OTHER_KERNEL_OBJS)
	$(LD) $(LDFLAGS) -o $@ $^
	@echo "Kernel linked: $@"

# Rule to extract the raw binary from the ELF file
$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@
	@echo "Kernel extracted to binary: $@"
	@echo -n "Kernel size: "; stat -c %s $@

# --- Generic Build Rules ---

# Rule for any C object file
$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $< -o $@

# Rule for any ASM object file
$(BUILD_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	$(ASM) $(ASM_FLAGS) $< -o $@

# Rule for stage 1 bootloader
$(STAGE1_OBJ): $(STAGE1_SRC)
	@mkdir -p $(dir $@)
	$(ASM) -f bin $< -o $@

# Rule for stage 2 bootloader
$(STAGE2_OBJ): $(STAGE2_SRC)
	@mkdir -p $(dir $@)
	$(ASM) -f bin $< -o $@

# --- Utility Targets ---
run: all
	$(QEMU_CMD) $(QEMU_OPTS)

debug: all
	$(QEMU_CMD) -S -gdb tcp::1234 $(QEMU_OPTS)

clean:
	@echo "Cleaning build directory..."
	rm -rf $(BUILD_DIR)