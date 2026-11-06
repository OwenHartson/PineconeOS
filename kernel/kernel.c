#include <stdint.h>

#include "include/gdt.h"
#include "include/idt.h"
#include "include/terminal.h"
#include "include/keyboard.h"
#include "include/memory.h"

extern void shell_run(void);

void kernel_main(uint32_t multiboot_magic,
                 uint32_t multiboot_info_address)
{
    gdt_initialize();

    terminal_initialize();

    terminal_write(
        "========================================\n"
        "           PINECONE KERNEL\n"
        "              Version 0.3\n"
        "========================================\n\n"
    );

    memory_initialize(
        multiboot_magic,
        multiboot_info_address
    );

    keyboard_initialize();

    idt_initialize();

    __asm__ volatile ("sti");

    terminal_write("\n[PINECONE] Kernel initialization complete.\n");
    terminal_write("[PINECONE] Starting system shell...\n\n");

    shell_run();

    while (1)
        __asm__ volatile ("hlt");
}