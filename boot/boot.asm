; PineconeOS - Initial Bootloader
; Multiboot v1 compatible

BITS 32

section .multiboot
align 4

MB_MAGIC    equ 0x1BADB002
MB_FLAGS    equ 0x00000003
MB_CHECKSUM equ -(MB_MAGIC + MB_FLAGS)

dd MB_MAGIC
dd MB_FLAGS
dd MB_CHECKSUM

section .bss
align 16

stack_bottom:
    resb 16384
stack_top:

section .text
global _start
extern kernel_main

_start:
    cli

    ; Set up kernel stack
    mov esp, stack_top

    ; Clear direction flag
    cld

    ; GRUB provides:
    ; EAX = Multiboot bootloader magic
    ; EBX = Multiboot information address
    push ebx
    push eax

    call kernel_main

    add esp, 8

.hang:
    cli
    hlt
    jmp .hang
