# LilHabOS

Lilhab is getting challenged by Trabowo (the same person that he met at 1st module) to create an simple kernel that have piping features using `echo`, `grep`, and `wc`. Lilhab feeling struggled and nervous as he think his work will not finished when met it's deadline. 

### Directory Structure

```
task-3/
├── bin/
├── include/
│   ├── kernel.h
│   ├── std_lib.h
│   └── std_type.h
├── src/
│   ├── bootloader.asm
│   ├── kernel.asm
│   ├── kernel.c
│   └── std_lib.c
├── bochsrc.txt
└── makefile
```

- [`bin/`](./bin/) : contains compiled files
- [`include/`](./include/) : contains header files
- [`src/`](./src/) : contains source code
- [`bochsrc.txt`](./bochsrc.txt) : Bochs configuration
- [`makefile`](./makefile) : make configuration file

  

### Kernel

#### [`kernel.asm`](./src/kernel.asm)

```asm
global _putInMemory
global _interrupt

; void putInMemory(int segment, int address, char character)
_putInMemory:
    push bp
    mov bp,sp
    push ds
    mov ax,[bp+4]
    mov si,[bp+6]
    mov cl,[bp+8]
    mov ds,ax
    mov [si],cl
    pop ds
    pop bp
    ret

; int interrupt(int number, int AX, int BX, int CX, int DX)
_interrupt:
    push bp
    mov bp,sp
    mov ax,[bp+4]
    push ds
    mov bx,cs
    mov ds,bx
    mov si,intr
    mov [si+1],al
    pop ds
    mov ax,[bp+6]
    mov bx,[bp+8]
    mov cx,[bp+10]
    mov dx,[bp+12]

intr:	int 0x00

    mov ah,0
    pop bp
    ret
```

- `_putInMemory` : function to write character to memory (explanation in modul)

- `_interrupt` : function to do interrupt

  - `number` : interrupt vector number
  - `AX`, `BX`, `CX`, `DX` : registers to be filled
  - Register `AX` is a combination of `AH` and `AL`
  - Register `BX` is a combination of `BH` and `BL`
  - Register `CX` is a combination of `CH` and `CL`
  - Register `DX` is a combination of `DH` and `DL`
  - To combine two `H` and `L` type registers into `X`, you can use one of the following methods.
    ```c
    AX = AH << 8 | AL // first method
    AX = AH * 256 + AL // second method
    ```

#### [`kernel.c`](./src/kernel.c)

```c
#include "std_lib.h"
#include "kernel.h"

// Insert global variables and function prototypes here

int main() {
    char buf[128];

    clearScreen();
    printString("LilHabOS - [PUT YOUR TEAM CODE HERE]\n");

    while (true) {
        printString("$> ");
        readString(buf);
        printString("\n");

        if (strlen(buf) > 0) {
            // Insert your functions here, you may not need to modify the rest of the main() function
        }
    }
}

// Insert function here
```

- The kernel will wait for input from the user and display the input back to the screen as long as the operating system is running.

- Insert your team code in the initial output of the shell, without the brackets `[]`.

  Example usage:

  ```bash
  LilHabOS - A01
  ```

- Do not change the contents of the `main` function, except inside the `if` function.

### Header Files

#### [`std_type.h`](./include/std_type.h)

This header contains the definition of data types that will be frequently used in the program.

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

- `byte` : unsigned char data type, used to store numbers in the range 0-255 (`0x00`-`0xFF`)

- `bool` : boolean data type, used to store `true` or `false` values

- `NULL` : NULL definition, used as null pointer constant

#### [`std_lib.h`](./include/std_lib.h)

This header contains the declaration of functions that will be frequently used in the program.

```c
int div(int a, int b);
int mod(int a, int b);

void memcpy(byte* src, byte* dst, unsigned int size);
unsigned int strlen(char* str);
bool strcmp(char* str1, char* str2);
void strcpy(char* src, char* dst);
void clear(byte* buf, unsigned int size);
```

- `div` : division function

- `mod` : modulo function

  The `div` and `mod` functions need to be created because there is no division and modulo operator in assembly language. Create these functions using available operators, such as `+`, `-`, `*`, `<<`, and `>>`.

- `memcpy` : function to copy memory

- `strlen` : function to count string length

- `strcmp` : function to compare two strings

- `strcpy` : function to copy string

- `clear` : function to fill memory with zeros

#### [`kernel.h`](./include/kernel.h)

This header contains the declaration of functions that have implementations in `kernel.asm` and will be implemented in `kernel.c`.

```c
extern void putInMemory(int segment, int address, char character);
extern int interrupt(int number, int AX, int BX, int CX, int DX);

void printString(char* str);
void readString(char* buf);
void clearScreen();
```

- `putInMemory` : the implementation of `_putInMemory` function is in `kernel.asm`
- `interrupt` : the implementation of `_interrupt` function is in `kernel.asm`
- `printString` : function to display string
- `readString` : function to read string
- `clearScreen` : function to clear the screen

### Makefile

```makefile
prepare:

bootloader:

stdlib:

kernel:

link:

build: prepare bootloader stdlib kernel link

run:
```

- `prepare` : create a new disk image `floppy.img` in the `bin/` directory with a size of 1.44 MB.

- `bootloader` : compile `bootloader.asm` into `bootloader.bin` in the `bin/` directory.

- `stdlib` : compile `std_lib.c` into `std_lib.o` in the `bin/` directory.

- `kernel` : compile `kernel.c` into `kernel.o` in the `bin/` directory and compile `kernel.asm` into `kernel_asm.o` in the `bin/` directory.

- `link` : merge `bootloader.bin`, `kernel.o`, `kernel_asm.o`, and `std_lib.o` into `floppy.img`.

- `build` : run `prepare`, `bootloader`, `stdlib`, `kernel`, and `link` commands.

- `run` : run Bochs with the `bochsrc.txt` configuration.

### Command Reference

#### `dd`

```bash
dd if=<input_file> of=<output_file> bs=<block_size> count=<block_count> seek=<seek_count> conv=<conversion>
```

#### `nasm`

```bash
nasm -f <format> <input_file> -o <output_file>
```

#### `bcc`

```bash
bcc -ansi -c <input_file> -o <output_file>
```

#### `ld86`

```bash
ld86 -o <output_file> -d [file1.o file2.o ...]
```

### Tasks

- a. Implement the `printString`, `readString`, and `clearScreen` function in `kernel.c` that will display and read a string on the screen.

  - `printString`: Displays a null-terminated string using `int 10h` with `AH=0x0E`.
  - `readString`: Reads characters from the keyboard using `int 16h` with `AH=0x00` until Enter is pressed. Includes basic Backspace handling.
  - `clearScreen`: Clears the screen and sets the cursor to the top-left corner `(0, 0)` using `int 10h` with `AH=0x06` and `AH=0x02`. The video buffer for character color will be changed to white.

- b. Complete the implementation of the functions in [`std_lib.h`](./include/std_lib.h) in [`std_lib.c`](./src/std_lib.c).

  The above functions can be used to simplify the implementation of the `printString`, `readString`, `clearScreen`, and further functions described on the next task.

- c. Implement the `echo` command

  The command takes the arguments provided (keyboard characters) for the `echo` command and prints them to the shell.

- c. Implement the `grep` command

  The command searches for lines matching a pattern in its input and prints the matching lines. `grep` will only take one argument using piping (`|`) from `echo` command. The output should be part of piped argument that passed to `grep`. If the argument didn't match, returns `NULL`

  Example usage:

  ```bash
  $> echo <argument> | grep <argument>
  ```

- d. Implement the `wc` command

  The command counts lines, words, and characters in its input. `wc` need no argument since it's getting piped (`|`) from earlier command. The output should be final count from piped argument that passed to `wc`. If the argument didn't match, returns `NULL` or `0`

  Example usage:

  ```bash
  $> echo <argument> | wc
  $> echo <argument> | grep <argument> | wc
  ```

- e. Create automation to compile by completing the [`makefile`](./makefile) file.

  To compile the program, the `make build` command will be used. All compiled program results will be stored in the [`bin/`](./bin) directory. To run the program, the `make run` command will be used.

