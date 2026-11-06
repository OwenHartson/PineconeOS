#include <stdint.h>

#include "../include/paging.h"
#include "../include/terminal.h"

#define PAGE_PRESENT  0x001
#define PAGE_WRITABLE 0x002

#define PAGE_ENTRIES 1024
#define INITIAL_MAP_SIZE (4 * 1024 * 1024)

static uint32_t page_directory[PAGE_ENTRIES]
    __attribute__((aligned(4096)));

static uint32_t first_page_table[PAGE_ENTRIES]
    __attribute__((aligned(4096)));

static uint8_t paging_active = 0;

void paging_initialize(void)
{
    terminal_write("\n[PINECONE] Initializing virtual memory...\n");

    /* Clear the page directory. */
    for (uint32_t i = 0; i < PAGE_ENTRIES; i++)
    {
        page_directory[i] = 0;
    }

    /* Identity-map the first 4 MB of physical memory. */
    for (uint32_t i = 0; i < PAGE_ENTRIES; i++)
    {
        uint32_t address = i * PAGE_SIZE;

        first_page_table[i] =
            address | PAGE_PRESENT | PAGE_WRITABLE;
    }

    /* Install the first page table into the directory. */
    page_directory[0] =
        ((uint32_t)first_page_table) |
        PAGE_PRESENT |
        PAGE_WRITABLE;

    /* Load the physical address of the page directory into CR3. */
    uint32_t directory_address = (uint32_t)page_directory;

    __asm__ volatile (
        "mov %0, %%cr3"
        :
        : "r"(directory_address)
        : "memory"
    );

    /* Enable paging through the CR0 register. */
    uint32_t cr0;

    __asm__ volatile (
        "mov %%cr0, %0"
        : "=r"(cr0)
    );

    cr0 |= 0x80000000;

    __asm__ volatile (
        "mov %0, %%cr0"
        :
        : "r"(cr0)
        : "memory"
    );

    paging_active = 1;

    terminal_write("[MEMORY] Page directory created.\n");
    terminal_write("[MEMORY] First page table created.\n");
    terminal_write("[MEMORY] First 4 MB identity mapped.\n");
    terminal_write("[PINECONE] Paging enabled successfully.\n");
}

uint8_t paging_is_enabled(void)
{
    return paging_active;
}