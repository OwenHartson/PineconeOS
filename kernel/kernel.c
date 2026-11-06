#include <stdint.h>

#include "include/gdt.h"
#include "include/idt.h"
#include "include/terminal.h"
#include "include/keyboard.h"
#include "include/memory.h"
#include "include/paging.h"
#include "include/heap.h"
#include "include/task.h"
#include "include/pit.h"
#include "include/ata.h"

extern void shell_run(void);

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

    /* Initialize ATA subsystem. */
    ata_initialize();
    static uint16_t disk_test_buffer[256];
    if (ata_get_device_count() > 0)
    {
        terminal_write("[ATA TEST] Reading sector 0...\n");

        if (ata_read_sector(
            0,
            0,
            disk_test_buffer
        ) == 0)
        {
            terminal_write("[ATA TEST] Sector read successful.\n");
        }
        else
        {
            terminal_write("[ATA TEST] Sector read failed.\n");
        }
    }



    /* Initialize task management. */
    scheduler_initialize();

    /* Configure the timer before enabling interrupts. */
    pit_initialize(100);

    /* Enable hardware interrupts */
    __asm__ volatile ("sti");

    terminal_write(
    "========================================\n"
    "           PINECONE KERNEL\n"
    "              Version 0.4\n"
    "========================================\n\n"
    );
    terminal_write("\n[PINECONE] Kernel initialization complete.\n");
    terminal_write("[PINECONE] Starting system shell...\n\n");

    shell_run();

    while (1)
        __asm__ volatile ("hlt");
}