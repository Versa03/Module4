org 0x7C00
bits 16

KERNEL_SEGMENT equ 0x1000
KERNEL_START_SECTOR equ 2
SECTORS_TO_READ equ 16

start:
    ; Set up segmen untuk membaca dari disk
    mov ax, 0x0000
    mov es, ax
    mov ds, ax
    
    ; Muat kernel ke 0x1000:0000
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

    ; Lompat ke awal kernel (yang sekarang adalah _start di kernel.asm)
    ; Kita set SEMUA segment register ke segment kernel
    mov ax, KERNEL_SEGMENT
    mov ds, ax
    mov es, ax
    ; SS akan diset di dalam kernel.asm
    
    jmp KERNEL_SEGMENT:0x0000

disk_error:
    jmp $ ; Halt

times 510 - ($ - $$) db 0
dw 0xAA55
