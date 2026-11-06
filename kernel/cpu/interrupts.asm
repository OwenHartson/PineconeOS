
BITS 32

section .text

extern interrupt_dispatch

global gdt_flush
global idt_load
global isr_common

; ==========================================
; Load Global Descriptor Table
; ==========================================

gdt_flush:
    mov eax, [esp + 4]
    lgdt [eax]

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    jmp 0x08:.gdt_done

.gdt_done:
    ret


; ==========================================
; Load Interrupt Descriptor Table
; ==========================================

idt_load:
    mov eax, [esp + 4]
    lidt [eax]
    ret


; ==========================================
; Common Interrupt Handler
; ==========================================

isr_common:
    pusha

    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call interrupt_dispatch
    add esp, 4

    pop gs
    pop fs
    pop es
    pop ds

    popa

    add esp, 8

    iretd


; ==========================================
; CPU Exception Macros
; ==========================================

%macro ISR_NOERR 1
global isr%1

isr%1:
    push strict dword 0
    push strict dword %1
    jmp isr_common
%endmacro


%macro ISR_ERR 1
global isr%1

isr%1:
    push strict dword %1
    jmp isr_common
%endmacro


; ==========================================
; Hardware IRQ Macro
; ==========================================

%macro IRQ 2
global irq%1

irq%1:
    push strict dword 0
    push strict dword %2
    jmp isr_common
%endmacro


; ==========================================
; CPU Exceptions 0-31
; ==========================================

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_ERR   21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_ERR   29
ISR_ERR   30
ISR_NOERR 31


; ==========================================
; Hardware IRQs 0-15
; ==========================================

IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47


; ==========================================
; Interrupt Address Table
; ==========================================

section .rodata

align 4

global isr_stub_table

isr_stub_table:

%assign i 0
%rep 32
    dd isr%+i
%assign i i+1
%endrep

%assign i 0
%rep 16
    dd irq%+i
%assign i i+1
%endrep
