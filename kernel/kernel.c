#include <stdint.h>

#include "include/gdt.h"
#include "include/idt.h"
#include "include/terminal.h"
#include "include/keyboard.h"
#include "include/memory.h"
#include "include/paging.h"
#include "include/heap.h"
#include "include/task.h"

extern void shell_run(void);

static void task_one(void)
{
    for (int i = 1; i <= 3; i++)
    {
        terminal_write("[TASK 1] Running...\n");

        task_yield();
    }

    terminal_write("[TASK 1] Finished.\n");
}

static void task_two(void)
{
    for (int i = 1; i <= 3; i++)
    {
        terminal_write("[TASK 2] Running...\n");

        task_yield();
    }

    terminal_write("[TASK 2] Finished.\n");
}

void kernel_main(uint32_t multiboot_magic,
                 uint32_t multiboot_info_address)
{
    gdt_initialize();

    terminal_initialize();

    /* Physical memory management */
    memory_initialize(
        multiboot_magic,
        multiboot_info_address
    );

    /* Hardware and interrupt initialization */
    keyboard_initialize();
    idt_initialize();

    /* Virtual memory management */
    paging_initialize();

    /* Initialize dynamic kernel memory. */
    heap_initialize();

    /* Initialize task management. */
    scheduler_initialize();

    /* Enable hardware interrupts */
    __asm__ volatile ("sti");

    terminal_write("\n[PINECONE] Kernel initialization complete.\n");
    terminal_write("[PINECONE] Starting system shell...\n\n");

    terminal_write(
        "========================================\n"
        "           PINECONE KERNEL\n"
        "              Version 0.4\n"
        "========================================\n\n"
    );

    shell_run();

    while (1)
        __asm__ volatile ("hlt");
}