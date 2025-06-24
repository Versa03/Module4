# LilHabOS Kernel Development Report

## Overview
This report documents the development of LilHabOS, a lightweight operating system kernel written in assembly and C. It provides basic functionality including screen output, keyboard input, a shell interface, and command piping for `echo`, `grep`, and `wc`. The report is divided into two parts:

- **Part 1**: Detailed explanation of all source code files
- **Part 2**: In-depth solutions for problems a to e

## Part 1

### 1. `kernel.c`

```c
#include "std_lib.h"
#include "kernel.h"

void handle_command(char* buf);
void intToString(int n, char* str);
void reverse(char* str);
char* strstr(char* haystack, char* needle);
void trim(char* str);
void strcat(char* dest, char* src);
void split_string(char* input, char delimiter, char** result, int* count);
void split_command_and_arg(char *str, char **parts, int *count);
```

- **Includes**: Incorporates `std_lib.h` for utility functions (e.g., string manipulation) and `kernel.h` for kernel-specific function declarations.
- **Function Prototypes**: Declares helper functions used within `kernel.c`:
  - `handle_command`: Processes user input commands.
  - `intToString`: Converts an integer to a string.
  - `reverse`: Reverses a string in-place.
  - `strstr`: Finds a substring within a string.
  - `trim`: Removes leading and trailing spaces from a string.
  - `strcat`: Concatenates two strings.
  - `split_string`: Splits a string by a delimiter.
  - `split_command_and_arg`: Separates a command and its argument.

---

```c
void printString(char* str) {
    int i = 0;
    while (str[i] != '\0') {
        int ax = 0x0E00 | str[i];
        int bx = 0x0007;
        interrupt(0x10, ax, bx, 0, 0);
        i++;
    }
}
```

- Prints a null-terminated string to the screen using BIOS video interrupts.
- **Loop**: Iterates through each character of `str` until the null terminator (`\0`) is reached.
- **Register Setup**:
  - `ax = 0x0E00 | str[i]`: Sets `AH=0x0E` for BIOS teletype output and `AL` to the character’s ASCII code.
  - `bx = 0x0007`: Sets `BH=0` (page number) and `BL=7` (light gray color).
- **Interrupt Call**: `interrupt(0x10, ...)` invokes BIOS interrupt 0x10 to print the character.
- **Increment**: Advances `i` to the next character.

---

```c
void readString(char* buf) {
    int i = 0;
    while (1) {
        int key_press = interrupt(0x16, 0x0000, 0, 0, 0);
        char c = key_press & 0xFF;

        if (c == '\r') {
            buf[i] = '\0';
            return;
        } else if (c == '\b') {
            if (i > 0) {
                i--;
                interrupt(0x10, 0x0E00 | '\b', 0x0007, 0, 0);
                interrupt(0x10, 0x0E00 | ' ', 0x0007, 0, 0);
                interrupt(0x10, 0x0E00 | '\b', 0x0007, 0, 0);
            }
        } else {
            if (i < 127) {
                buf[i] = c;
                interrupt(0x10, 0x0E00 | c, 0x0007, 0, 0);
                i++;
            }
        }
    }
}
```

- Reads keyboard input into a buffer, supporting backspace and enter keys.
- **Infinite Loop**: Continues reading until the enter key (`\r`) is pressed.
- **Keyboard Input**: `interrupt(0x16, 0x0000, ...)` uses BIOS interrupt 0x16 (AH=0) to get a key press, storing the ASCII code in `c`.
- **Enter Key**: If `c == '\r'`, null-terminates the buffer and returns.
- **Backspace Key**: If `c == '\b'` and `i > 0`, decrements `i`, removes the last character, and updates the screen by moving the cursor back, printing a space, and moving back again.
- **Other Characters**: If `i < 127` (to prevent buffer overflow), stores the character in `buf[i]`, echoes it to the screen, and increments `i`.

---

```c
void clearScreen() {
    interrupt(0x10, 0x0600, 0x0700, 0x0000, 0x184F);
    interrupt(0x10, 0x0200, 0x0000, 0x0000, 0);
}
```

- **Purpose**: Clears the screen and resets the cursor to the top-left corner.
- **Screen Clear**: `interrupt(0x10, 0x0600, 0x0700, 0x0000, 0x184F)` uses BIOS interrupt 0x10 (AH=6) to scroll the entire screen up by 0 lines, effectively clearing it, with attribute 7 (light gray background).
- **Cursor Reset**: `interrupt(0x10, 0x0200, ...)` sets the cursor position to (0,0) using AH=2.

---

```c
int main() {
    char buf[128];

    clearScreen();
    printString("LilHabOS - U02\n");

    while (true) {
        printString("$> ");
        readString(buf);
        printString("\n");

        if (strlen(buf) > 0) {
            handle_command(buf);
        }
    }
}
```

- Serves as the kernel’s main entry point, implementing a simple shell loop.
- **Buffer**: Declares a 128-byte buffer for user input.
- **Initialization**: Calls `clearScreen()` to clear the display and prints a welcome message.
- **Main Loop**:
  - Prints the shell prompt (`$> `).
  - Calls `readString(buf)` to capture user input.
  - Prints a newline for formatting.
  - If the input is non-empty (`strlen(buf) > 0`), processes it with `handle_command(buf)`.

---

```c
void handle_command(char* buf) {
    char* commands[10];
    int cmd_count = 0;
    char pipe_buffer[128];
    char* cmd_parts[2];
    int parts_count;
    int i;

    split_string(buf, '|', commands, &cmd_count);
    if (cmd_count == 0) return;

    clear(pipe_buffer, 128);

    split_command_and_arg(commands[0], cmd_parts, &parts_count);
    if (parts_count > 0 && strcmp(cmd_parts[0], "echo")) {
        if (parts_count > 1) {
            strcpy(pipe_buffer, cmd_parts[1]);
        }
    } else {
        printString("Error: First command must be 'echo'.\n");
        return;
    }

    for (i = 1; i < cmd_count; i++) {
        char temp_buffer[128];
        clear(temp_buffer, 128);

        strcpy(temp_buffer, pipe_buffer);
        clear(pipe_buffer, 128);

        split_command_and_arg(commands[i], cmd_parts, &parts_count);

        if (parts_count > 0 && strcmp(cmd_parts[0], "grep")) {
            if (parts_count > 1) {
                if (strstr(temp_buffer, cmd_parts[1])) {
                    strcpy(pipe_buffer, temp_buffer);
                }
            }
        } else if (parts_count > 0 && strcmp(cmd_parts[0], "wc")) {
            int lines = 0, words = 0, chars = 0;
            char num_str[10];
            bool in_word = false;
            int j = 0;

            chars = strlen(temp_buffer);
            if (chars > 0) lines = 1;

            while(temp_buffer[j] != '\0') {
                if (temp_buffer[j] != ' ' && !in_word) {
                    in_word = true;
                    words++;
                } else if (temp_buffer[j] == ' ') {
                    in_word = false;
                }
                j++;
            }

            strcat(pipe_buffer, " ");
            intToString(lines, num_str); strcat(pipe_buffer, num_str);
            strcat(pipe_buffer, " ");
            intToString(words, num_str); strcat(pipe_buffer, num_str);
            strcat(pipe_buffer, " ");
            intToString(chars, num_str); strcat(pipe_buffer, num_str);
        } else {
            printString("Unknown or invalid command in pipe.\n");
            return;
        }
    }

    printString(pipe_buffer);
    printString("\n");
}
```

- Processes user commands, supporting piping with `echo`, `grep`, and `wc`.
- **Variables**:
  - `commands[10]`: Array to store up to 10 piped commands.
  - `cmd_count`: Number of commands after splitting by `|`.
  - `pipe_buffer[128]`: Holds the intermediate and final output of the pipeline.
  - `cmd_parts[2]`: Stores command and its argument.
- **Command Splitting**: `split_string(buf, '|', commands, &cmd_count)` splits input by `|` to handle piped commands.
- **Echo Check**:
  - Splits the first command with `split_command_and_arg`.
  - Checks if the command is `echo` using `strcmp`. (Note: `strcmp(cmd_parts[0], "echo")` is incorrect; it should be `!strcmp` as `strcmp` returns 0 on match.)
  - Copies the argument to `pipe_buffer` if present.
- **Pipeline Processing**:
  - Loops through subsequent commands (`i = 1` to `cmd_count - 1`).
  - For `grep`: Checks if the pattern (`cmd_parts[1]`) exists in `temp_buffer` using `strstr`. If found, copies `temp_buffer` back to `pipe_buffer`; otherwise, `pipe_buffer` remains empty.
  - For `wc`: Counts lines (hardcoded to 1 if non-empty), words (by detecting non-space to space transitions), and characters (using `strlen`). Converts counts to strings and appends to `pipe_buffer`.
  - For unknown commands: Prints an error and exits.
- **Output**: Prints the final `pipe_buffer` content and a newline.

---

```c
void reverse(char *str) {
    int i = 0;
    int j = strlen(str) - 1;
    char temp;
    while (i < j) {
        temp = str[i]; str[i] = str[j]; str[j] = temp;
        i++; j--;
    }
}
```

- Reverses a string in-place.
- **Indices**: Uses `i` (start) and `j` (end, calculated as `strlen(str) - 1`).
- **Swap Loop**: While `i < j`, swaps characters at positions `i` and `j`, incrementing `i` and decrementing `j` until they meet.


```c
void intToString(int n, char* str) {
    int i = 0;
    if (n == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }
    while (n != 0) {
        str[i++] = mod(n, 10) + '0';
        n = div(n, 10);
    }
    str[i] = '\0';
    reverse(str);
}
```

- Converts an integer to a string representation.
- **Zero Case**: If `n == 0`, sets `str` to `"0"` and returns.
- **Digit Extraction**: Repeatedly uses `mod(n, 10)` to get the last digit, converts it to ASCII by adding `'0'`, and stores it in `str`. Divides `n` by 10 to process the next digit.
- **Termination**: Null-terminates the string and reverses it to get the correct order.


```c
char* strstr(char* haystack, char* needle) {
    int i, j;
    if (*needle == '\0') return haystack;
    for (i = 0; haystack[i] != '\0'; ++i) {
        bool match = true;
        for (j = 0; needle[j] != '\0'; ++j) {
            if (haystack[i + j] == '\0' || haystack[i + j] != needle[j]) {
                match = false; break;
            }
        }
        if (match) return (haystack + i);
    }
    return NULL;
}
```

- Finds the first occurrence of `needle` in `haystack`, returning a pointer to the start of the match or `NULL` if not found.
- **Empty Needle**: If `needle` is empty, returns `haystack`.
- **Outer Loop**: Iterates through each position `i` in `haystack`.
- **Inner Loop**: Checks if `needle` matches at position `i` by comparing characters. If a mismatch occurs or `haystack` ends, sets `match = false`.
- **Return**: Returns the starting address of the match or `NULL` if no match is found.


```c
void trim(char* str) {
    char *start = str;
    char *end;
    while (*start == ' ') start++;
    end = str + strlen(str) - 1;
    while (end >= start && *end == ' ') end--;
    *(end + 1) = '\0';
    if (str != start) {
        strcpy(start, str);
    }
}
```

- Removes leading and trailing spaces from a string.
- **Leading Spaces**: Advances `start` until a non-space character is found.
- **Trailing Spaces**: Moves `end` backward from the string’s end until a non-space character or `start` is reached.
- **Null Termination**: Sets `*(end + 1) = '\0'` to trim trailing spaces.
- **Shift String**: If `start` moved (due to leading spaces), copies the trimmed string to the original pointer.


```c
void strcat(char* dest, char* src) {
    int i = strlen(dest), j = 0;
    while (src[j] != '\0') {
        dest[i++] = src[j++];
    }
    dest[i] = '\0';
}
```

- Appends `src` to `dest`.
- **Index Setup**: Starts `i` at the end of `dest` (via `strlen`) and `j` at the start of `src`.
- **Copy Loop**: Copies each character from `src` to `dest` until `src[j] == '\0'`.
- **Termination**: Null-terminates `dest`.


```c
void split_string(char* input, char delimiter, char** result, int* count) {
    char* token_start = input;
    int i;
    *count = 0;

    while (*input != '\0') {
        if (*input == delimiter) {
            result[*count] = token_start;
            (*count)++;
            *input = '\0';
            token_start = input + 1;
        }
        input++;
    }
    result[*count] = token_start;
    (*count)++;

    for (i = 0; i < *count; i++) {
        trim(result[i]);
    }
}
```

- Splits `input` by `delimiter`, storing pointers to substrings in `result` and the count in `count`.
- **Initialization**: Sets `token_start` to the input’s start and `*count = 0`.
- **Splitting Loop**: Scans `input` for `delimiter`, null-terminating each token and updating `result` and `token_start`.
- **Final Token**: Adds the last token after the loop.
- **Trimming**: Calls `trim` on each substring to remove spaces.


```c
void split_command_and_arg(char *str, char **parts, int *count) {
    char *arg_start;
    trim(str);
    if (strlen(str) == 0) {
        *count = 0;
        return;
    }
    arg_start = str;
    while(*arg_start != ' ' && *arg_start != '\0') arg_start++;

    parts[0] = str;
    if (*arg_start == ' ') {
        *arg_start = '\0';
        arg_start++;
        while(*arg_start == ' ') arg_start++;
        parts[1] = arg_start;
        *count = (strlen(parts[1]) > 0) ? 2 : 1;
    } else {
        *count = 1;
    }
}
```

- Splits a string into a command and its argument, storing results in `parts` and count in `count`.
- **Trimming**: Removes leading/trailing spaces from `str`.
- **Empty Check**: If `str` is empty, sets `*count = 0` and returns.
- **Command Extraction**: Sets `parts[0]` to the start of `str`, advances `arg_start` to the first space or end.
- **Argument Handling**: If a space is found, null-terminates the command, skips additional spaces, and sets `parts[1]` to the argument. Sets `*count` to 2 if an argument exists, otherwise 1.

---

### 2. `std_lib.c`

```c
#include "std_lib.h"

int div(int a, int b) {
    int result = 0;
    if (b == 0) return -1;
    while (a >= b) {
        a = a - b;
        result++;
    }
    return result;
}

int mod(int a, int b) {
    if (b == 0) return -1;
    while (a >= b) {
        a = a - b;
    }
    return a;
}

void memcpy(byte* src, byte* dst, unsigned int size) {
    unsigned int i;
    for (i = 0; i < size; i++) {
        dst[i] = src[i];
    }
}

unsigned int strlen(char* str) {
    unsigned int i = 0;
    while (str[i] != '\0') {
        i++;
    }
    return i;
}

bool strcmp(char* str1, char* str2) {
    int i = 0;
    while (str1[i] != '\0' && str2[i] != '\0') {
        if (str1[i] != str2[i]) {
            return false;
        }
        i++;
    }
    return (str1[i] == '\0' && str2[i] == '\0');
}

void strcpy(char* src, char* dst) {
    int i = 0;
    while (src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

void clear(byte* buf, unsigned int size) {
    unsigned int i;
    for (i = 0; i < size; i++) {
        buf[i] = 0;
    }
}
```

- Provides basic standard library functions for the kernel.
- **div**: Performs integer division using repeated subtraction, returns -1 if `b == 0`.
- **mod**: Computes the remainder using repeated subtraction, returns -1 if `b == 0`.
- **memcpy**: Copies `size` bytes from `src` to `dst` using a loop.
- **strlen**: Returns the length of a null-terminated string by counting characters.
- **strcmp**: Compares two strings, returning `true` if they are identical, `false` otherwise.
- **strcpy**: Copies `src` to `dst`, including the null terminator.
- **clear**: Sets `size` bytes of `buf` to 0.


### 3. `kernel.asm`

```asm
global _start
global _putInMemory
global _interrupt

extern _main

bits 16

_start:
    mov ax, 0x1000
    mov ss, ax
    mov sp, 0xFFF0

    call _main

    cli
    hlt

_putInMemory:
    push bp
    mov bp, sp
    push ds

    mov ax, [bp+4]
    mov si, [bp+6]
    mov cl, [bp+8]
    mov ds, ax
    mov [si], cl

    pop ds
    pop bp
    ret

_interrupt:
    push bp
    mov bp, sp
    mov ax, [bp+4]
    push ds
    mov bx, cs
    mov ds, bx
    mov si, intr
    mov [si+1], al
    pop ds

    mov ax, [bp+6]
    mov bx, [bp+8]
    mov cx, [bp+10]
    mov dx, [bp+12]

intr: int 0x00

    mov ah, 0
    pop bp
    ret
```

- Bootstraps the kernel and provides low-level functions for memory and interrupt handling.
- **_start**:
  - Sets the stack segment to 0x1000 and stack pointer to 0xFFF0.
  - Calls the C `main` function.
  - Disables interrupts (`cli`) and halts the CPU (`hlt`) after `main` returns.
- **_putInMemory**:
  - Saves base pointer and data segment.
  - Retrieves parameters from the stack: segment, address, and character.
  - Sets the data segment and writes the character to the specified memory location.
  - Restores registers and returns.
- **_interrupt**:
  - Saves base pointer and retrieves the interrupt number from the stack.
  - Sets up the interrupt vector by modifying the `intr` instruction.
  - Loads registers (AX, BX, CX, DX) from the stack.
  - Executes the interrupt (`int 0x00`, dynamically set).
  - Clears AH and returns.


### 4. `bootloader.asm`

```asm
org 0x7C00
bits 16

KERNEL_SEGMENT equ 0x1000
KERNEL_START_SECTOR equ 2
SECTORS_TO_READ equ 16

start:
    mov ax, 0x0000
    mov es,וע

System: ax
    mov ds, ax

    mov ax, KERNEL_SEGMENT
    mov es, ax
    mov bx, 0x0000
    mov ah, 0x02
    mov al, SECTORS_TO_READ
    mov ch, 0
    mov cl, KERNEL_START_SECTOR
    mov dh, 0
    mov dl, 0x00
    int 0x13
    jc disk_error

    mov ax, KERNEL_SEGMENT
    mov ds, ax
    mov es, ax

    jmp KERNEL_SEGMENT:0x0000

disk_error:
    jmp $

times 510 - ($ - $$) db 0
dw 0xAA55
```

- Loads the kernel from disk into memory and jumps to its entry point.
- **Setup**: Initializes segment registers (`es`, `ds`) to 0x0000.
- **Disk Read**:
  - Sets `es` to `KERNEL_SEGMENT` (0x1000) and `bx` to 0x0000 (offset).
  - Uses BIOS interrupt 0x13 (AH=0x02) to read `SECTORS_TO_READ` (16) sectors starting from `KERNEL_START_SECTOR` (2) on drive 0.
  - Checks for errors using the carry flag (`jc disk_error`).
- **Jump**: Sets `ds` and `es` to `KERNEL_SEGMENT` and jumps to `KERNEL_SEGMENT:0x0000`.
- **Error Handling**: Loops indefinitely on disk read failure.
- **Boot Signature**: Pads the bootloader to 510 bytes and adds the 0xAA55 signature.


### 5. `std_type.h`

```c
#ifndef __STD_TYPE_H__
#define __STD_TYPE_H__

typedef unsigned char byte;

#ifndef bool
typedef char bool;
#define true 1
#define false 0
#endif

#ifndef NULL
#define NULL ((void*)0)
#endif

#endif /* __STD_TYPE_H__ */
```

- Defines basic types for the kernel.
- **byte**: Defines `byte` as an unsigned char.
- **bool**: Defines `bool` as a char with `true` (1) and `false` (0), if not already defined.
- **NULL**: Defines `NULL` as a null pointer, if not already defined.


### 6. `kernel.h`

```c
extern void putInMemory(int segment, int address, char character);
extern int interrupt(int number, int AX, int BX, int CX, int DX);

void printString(char* str);
void readString(char* buf);
void clearScreen();
```

- Declares function prototypes for kernel core functions.
- **extern Functions**:
  - `putInMemory`: Writes a character to a specific memory location (implemented in assembly).
  - `interrupt`: Executes a BIOS interrupt with specified registers (implemented in assembly).
- **Kernel Functions**:
  - `printString`: Prints a string to the screen.
  - `readString`: Reads user input from the keyboard.
  - `clearScreen`: Clears the screen and resets the cursor.


### 7. `std_lib.h`

```c
#include "std_type.h"

int div(int a, int b);
int mod(int a, int b);

void memcpy(byte* src, byte* dst, unsigned int size);
unsigned int strlen(char* str);
bool strcmp(char* str1, char* str2);
void strcpy(char* src, char* dst);
void clear(byte* buf, unsigned int size);
```

- Declares prototypes for standard library utility functions.
- **Functions**:
  - `div` and `mod`: Integer division and modulo operations.
  - `memcpy`: Copies bytes between memory locations.
  - `strlen`: Computes string length.
  - `strcmp`: Compares strings for equality.
  - `strcpy`: Copies a string.
  - `clear`: Zeros out a buffer.


### 8. `Makefile`

```makefile
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
	ld86 -o $(KERNEL_BIN) -d $(KERNEL_ASM_OBJ) $(KERNEL_C_OBJ) $(STDLIB_C_OBJ)
	$(DD) if=$(BOOTLOADER_BIN) of=$(FLOPPY_IMG) bs=512 count=1 conv=notrunc
	$(DD) if=$(KERNEL_BIN) of=$(FLOPPY_IMG) bs=512 seek=1 conv=notrunc

build: prepare bootloader stdlib kernel link

run: build
	$(BOCHS) -f bochsrc.txt

clean:
	rm -rf $(BIN_DIR)
```

- Automates the build process for the kernel and bootloader.
- **Tools and Directories**: Defines tools (`bcc`, `ld86`, `nasm`, `dd`, `bochs`) and directory paths.
- **Files**: Specifies source and output files for the bootloader, kernel, and standard library.
- **Targets**:
  - `prepare`: Creates the output directory and a 1.44MB floppy image.
  - `bootloader`: Assembles `bootloader.asm` into a binary.
  - `stdlib`: Compiles `std_lib.c` into an object file.
  - `kernel`: Compiles `kernel.c` and assembles `kernel.asm`.
  - `link`: Links object files into `kernel.bin` and writes the bootloader and kernel to the floppy image.
  - `build`: Runs all build steps.
  - `run`: Builds and runs the emulator with `bochsrc.txt`.
  - `clean`: Removes the output directory.


## Part 2: Solving Problems a to e Using the Code

### Problem a: Basic Echo Command

**Code Used**:

```c
split_command_and_arg(commands[0], cmd_parts, &parts_count);
if (parts_count > 0 && strcmp(cmd_parts[0], "echo")) {
    if (parts_count > 1) {
        strcpy(pipe_buffer, cmd_parts[1]);
    }
} else {
    printString("Error: First command must be 'echo'.\n");
    return;
}
```

- **Objective**: Implement a basic `echo` command that outputs its argument to the screen.
- **Process**:
  - `split_command_and_arg(commands[0], cmd_parts, &parts_count)` splits the first command into command name (`cmd_parts[0]`) and argument (`cmd_parts[1]`).
  - Checks if the command is `echo` using `strcmp`. (Note: The condition `strcmp(cmd_parts[0], "echo")` is incorrect; it should be `!strcmp` since `strcmp` returns 0 on match, causing the error branch to execute for valid `echo` commands.)
  - If valid and an argument exists (`parts_count > 1`), copies the argument to `pipe_buffer` for output or further piping.
  - If invalid, prints an error message and exits.
- **Logic Flow**:
  - Input is split into command and argument.
  - Validates that the command is `echo`.
  - Stores the argument for display.
- **Edge Cases**:
  - Empty input: Handled by `if (strlen(buf) > 0)` in `main`.
  - No argument: `parts_count == 1` results in an empty `pipe_buffer`.
  - Incorrect command: Prints error if not `echo`.

**Test Cases**:


### Problem b: Echo with Grep Filter

**Code Used**:

```c
for (i = 1; i < cmd_count; i++) {
    char temp_buffer[128];
    clear(temp_buffer, 128);
    strcpy(temp_buffer, pipe_buffer);
    clear(pipe_buffer, 128);

    split_command_and_arg(commands[i], cmd_parts, &parts_count);

    if (parts_count > 0 && strcmp(cmd_parts[0], "grep")) {
        if (parts_count > 1) {
            if (strstr(temp_buffer, cmd_parts[1])) {
                strcpy(pipe_buffer, temp_buffer);
            }
        }
    } else {
        printString("Unknown or invalid command in pipe.\n");
        return;
    }
}
```

- **Objective**: Filter `echo` output through a `grep` command that checks for a substring.
- **Process**:
  - Iterates through piped commands starting from the second command (`i = 1`).
  - Copies `pipe_buffer` to `temp_buffer` and clears `pipe_buffer`.
  - Splits the current command with `split_command_and_arg`.
  - If the command is `grep` (`strcmp(cmd_parts[0], "grep")`, which should be `!strcmp`), checks if the pattern (`cmd_parts[1]`) exists in `temp_buffer` using `strstr`.
  - If found, copies `temp_buffer` back to `pipe_buffer`; otherwise, `pipe_buffer` remains empty.
  - Prints an error for unknown commands.
- **Logic Flow**:
  - Takes `echo` output from `pipe_buffer`.
  - For each `grep`, checks if the pattern matches the current output.
  - Updates `pipe_buffer` only if the pattern is found.
- **Edge Cases**:
  - No pattern provided: `parts_count == 1` leaves `pipe_buffer` empty.
  - Pattern not found: `strstr` returns `NULL`, leaving `pipe_buffer` empty.
  - Case sensitivity: `strstr` is case-sensitive, so `Hello` won’t match `hello`.

**Test Cases**:




### Problem c: Echo, Grep, and Word Count (wc)

**Code Used**:

```c
else if (parts_count > 0 && strcmp(cmd_parts[0], "wc")) {
    int lines = 0, words = 0, chars = 0;
    char num_str[10];
    bool in_word = false;
    int j = 0;

    chars = strlen(temp_buffer);
    if (chars > 0) lines = 1;

    while(temp_buffer[j] != '\0') {
        if (temp_buffer[j] != ' ' && !in_word) {
            in_word = true;
            words++;
        } else if (temp_buffer[j] == ' ') {
            in_word = false;
        }
        j++;
    }

    strcat(pipe_buffer, " ");
    intToString(lines, num_str); strcat(pipe_buffer, num_str);
    strcat(pipe_buffer, " ");
    intToString(words, num_str); strcat(pipe_buffer, num_str);
    strcat(pipe_buffer, " ");
    intToString(chars, num_str); strcat(pipe_buffer, num_str);
}
```

- **Objective**: Count lines, words, and characters in the piped output and append the counts.
- **Process**:
  - Checks if the command is `wc` (`strcmp(cmd_parts[0], "wc")`, which should be `!strcmp`).
  - Initializes counters: `lines`, `words`, `chars`.
  - Sets `chars` to `strlen(temp_buffer)`.
  - Sets `lines = 1` if `chars > 0` (assumes single-line input).
  - Counts words by detecting transitions from space to non-space characters.
  - Converts counts to strings using `intToString` and appends them to `pipe_buffer` with spaces.
- **Logic Flow**:
  - Computes character count directly.
  - Assumes one line for non-empty input.
  - Tracks word boundaries using `in_word` flag.
  - Formats output as `<lines> <words> <chars>`.
- **Edge Cases**:
  - Empty input: `chars = 0`, so `lines = 0`, `words = 0`.
  - Multiple spaces: Handled by `in_word` flag, avoiding multiple word counts.
  - Non-alphanumeric characters: Counted as part of words, which may not be desired.

**Test Cases**:


### Problem d: Updated Echo and Grep Pipe

**Code Used**:

```c
split_command_and_arg(commands[0], cmd_parts, &parts_count);
if (parts_count > 0 && strcmp(cmd_parts[0], "echo")) {
    if (parts_count > 1) {
        strcpy(pipe_buffer, cmd_parts[1]);
    }
} else {
    printString("Error: First command must be 'echo'.\n");
    return;
}

for (i = 1; i < cmd_count; i++) {
    char temp_buffer[128];
    clear(temp_buffer, 128);
    strcpy(temp_buffer, pipe_buffer);
    clear(pipe_buffer, 128);

    split_command_and_arg(commands[i], cmd_parts, &parts_count);

    if (parts_count > 0 && strcmp(cmd_parts[0], "grep")) {
        if (parts_count > 1) {
            if (strstr(temp_buffer, cmd_parts[1])) {
                strcpy(pipe_buffer, temp_buffer);
            }
        }
    } else {
        printString("Unknown or invalid command in pipe.\n");
        return;
    }
}

printString(pipe_buffer);
printString("\n");
```

- **Objective**: Support a pipeline of `echo` followed by multiple `grep` commands to filter output sequentially.
- **Process**:
  - Validates the first command as `echo` and copies its argument to `pipe_buffer`.
  - For each subsequent `grep` command:
    - Copies `pipe_buffer` to `temp_buffer` and clears `pipe_buffer`.
    - Splits the command into name and pattern.
    - If the pattern is found in `temp_buffer`, restores `temp_buffer` to `pipe_buffer`.
    - If not found, `pipe_buffer` remains empty.
  - Prints the final `pipe_buffer` content.
- **Logic Flow**:
  - Starts with `echo`’s argument.
  - Each `grep` filters the current output, passing it through only if the pattern matches.
  - Outputs the final filtered result.
- **Edge Cases**:
  - Multiple `grep` commands: Each applies sequentially, potentially clearing `pipe_buffer` if any fails.
  - Invalid commands: Exits with an error message.
  - No matches: Results in empty output.

**Test Cases**:


### Problem e: Full Pipeline with Echo, Grep, and Wc

**Code Used**:

```c
split_command_and_arg(commands[0], cmd_parts, &parts_count);
if (parts_count > 0 && strcmp(cmd_parts[0], "echo")) {
    if (parts_count > 1) {
        strcpy(pipe_buffer, cmd_parts[1]);
    }
} else {
    printString("Error: First command must be 'echo'.\n");
    return;
}

for (i = 1; i < cmd_count; i++) {
    char temp_buffer[128];
    clear(temp_buffer, 128);
    strcpy(temp_buffer, pipe_buffer);
    clear(pipe_buffer, 128);

    split_command_and_arg(commands[i], cmd_parts, &parts_count);

    if (parts_count > 0 && strcmp(cmd_parts[0], "grep")) {
        if (parts_count > 1) {
            if (strstr(temp_buffer, cmd_parts[1])) {
                strcpy(pipe_buffer, temp_buffer);
            }
        }
    } else if (parts_count > 0 && strcmp(cmd_parts[0], "wc")) {
        int lines = 0, words = 0, chars = 0;
        char num_str[10];
        bool in_word = false;
        int j = 0;

        chars = strlen(temp_buffer);
        if (chars > 0) lines = 1;

        while (temp_buffer[j] != '\0') {
            if (temp_buffer[j] != ' ' && !in_word) {
                in_word = true;
                words++;
            } else if (temp_buffer[j] == ' ') {
                in_word = false;
            }
            j++;
        }

        strcat(pipe_buffer, " ");
        intToString(lines, num_str); strcat(pipe_buffer, num_str);
        strcat(pipe_buffer, " ");
        intToString(words, num_str); strcat(pipe_buffer, num_str);
        strcat(pipe_buffer, " ");
        intToString(chars, num_str); strcat(pipe_buffer, num_str);
    } else {
        printString("Unknown or invalid command in pipe.\n");
        return;
    }
}

printString(pipe_buffer);
printString("\n");
```

- **Objective**: Implement a full pipeline starting with `echo`, followed by zero or more `grep` commands, and optionally ending with `wc`.
- **Process**:
  - Validates `echo` as the first command and copies its argument to `pipe_buffer`.
  - Processes each subsequent command:
    - For `grep`: Filters `pipe_buffer` based on the pattern, clearing it if no match.
    - For `wc`: Counts lines (1 if non-empty), words, and characters, appending counts to `pipe_buffer`.
  - Prints the final `pipe_buffer`.
- **Logic Flow**:
  - `echo` seeds the pipeline.
  - Each `grep` filters the output.
  - `wc` computes and appends counts.
  - Final output is displayed.
- **Edge Cases**:
  - No `grep` commands: `wc` processes `echo` output directly.
  - Multiple `grep` failures: Results in `0 0 0` for `wc`.
  - Long input: Limited by `pipe_buffer` (128 bytes).

**Test Cases**:

