global _start
global _putInMemory
global _interrupt

extern _main ; Beritahu assembly bahwa 'main' ada di file C

bits 16

_start:
    ; Pindahkan alamat stack ke lokasi yang lebih aman
    mov ax, 0x1000 ; Sama dengan KERNEL_SEGMENT
    mov ss, ax
    mov sp, 0xFFF0
    
    ; Panggil fungsi main dari C
    call _main

    ; Jika main kembali (seharusnya tidak), halt sistem
    cli
    hlt

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

intr: int 0x00

  mov ah,0
  pop bp
  ret
