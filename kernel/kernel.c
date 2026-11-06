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
#include "include/fat32.h"

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
    /*
    * Update 9: FAT32 filesystem test
    */
    if (ata_get_device_count() > 0)
    {
        terminal_write("[FAT32] Attempting filesystem mount...\n");

        if (fat32_mount(0) == 0)
        {
            terminal_write("[FAT32] Filesystem mounted successfully.\n");

            fat32_list_root();
        }
        else
        {
            terminal_write("[FAT32] No mountable filesystem found.\n");
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