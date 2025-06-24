# Makefile for LilHabOS

# Compiler and tools
BCC = bcc
LD86 = ld86
NASM = nasm
DD = dd
BOCHS = bochs

# Directories
SRC_DIR = src
BIN_DIR = bin
INC_DIR = include

# Files
BOOTLOADER_SRC = $(SRC_DIR)/bootloader.asm
KERNEL_ASM_SRC = $(SRC_DIR)/kernel.asm
KERNEL_C_SRC = $(SRC_DIR)/kernel.c
STDLIB_C_SRC = $(SRC_DIR)/std_lib.c

BOOTLOADER_BIN = $(BIN_DIR)/bootloader.bin
KERNEL_ASM_OBJ = $(BIN_DIR)/kernel_asm.o
KERNEL_C_OBJ = $(BIN_DIR)/kernel.o
STDLIB_C_OBJ = $(BIN_DIR)/std_lib.o
KERNEL_BIN = $(BIN_DIR)/kernel.bin
FLOPPY_IMG = $(BIN_DIR)/floppy.img

# BCC flags
BCC_FLAGS = -ansi -c -I$(INC_DIR)

.PHONY: all build clean run prepare bootloader stdlib kernel link

all: build

prepare:
	@mkdir -p $(BIN_DIR)
	$(DD) if=/dev/zero of=$(FLOPPY_IMG) bs=512 count=2880

bootloader: $(BOOTLOADER_SRC)
	$(NASM) -f bin $(BOOTLOADER_SRC) -o $(BOOTLOADER_BIN)

stdlib: $(STDLIB_C_SRC)
	$(BCC) $(BCC_FLAGS) $(STDLIB_C_SRC) -o $(STDLIB_C_OBJ)

kernel: $(KERNEL_C_SRC) $(KERNEL_ASM_SRC)
	$(BCC) $(BCC_FLAGS) $(KERNEL_C_SRC) -o $(KERNEL_C_OBJ)
	$(NASM) -f as86 $(KERNEL_ASM_SRC) -o $(KERNEL_ASM_OBJ)

link:
	# URUTAN BARU: Assembly dulu, baru C.
	ld86 -o $(KERNEL_BIN) -d $(KERNEL_ASM_OBJ) $(KERNEL_C_OBJ) $(STDLIB_C_OBJ)
	$(DD) if=$(BOOTLOADER_BIN) of=$(FLOPPY_IMG) bs=512 count=1 conv=notrunc
	$(DD) if=$(KERNEL_BIN) of=$(FLOPPY_IMG) bs=512 seek=1 conv=notrunc

build: prepare bootloader stdlib kernel link

run: build
	$(BOCHS) -f bochsrc.txt

clean:
	rm -rf $(BIN_DIR)
