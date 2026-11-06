BITS 32

section .text

global task_context_switch

; void task_context_switch(
;     uint32_t* old_stack_pointer,
;     uint32_t new_stack_pointer
; );

task_context_switch:

    ; Save registers belonging to the current task.
    push ebp
    push ebx
    push esi
    push edi

    ; Get function arguments.
    mov eax, [esp + 20]
    mov edx, [esp + 24]

    ; Save current task's stack.
    mov [eax], esp

    ; Switch to the next task's stack.
    mov esp, edx

    ; Restore next task's registers.
    pop edi
    pop esi
    pop ebx
    pop ebp

    ; Continue execution on the new task.
    ret