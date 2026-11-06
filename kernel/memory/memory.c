#include <stdint.h>
#include "../include/memory.h"
#include "../include/terminal.h"

#define MAX_FRAMES 1048576
#define BITMAP_SIZE (MAX_FRAMES / 8)

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

#define MULTIBOOT_INFO_MEMORY_MAP (1 << 6)
#define MULTIBOOT_INFO_MODULES    (1 << 3)

static uint8_t memory_bitmap[BITMAP_SIZE];

static uint32_t total_frames = 0;
static uint32_t used_frames = MAX_FRAMES;

extern uint32_t __kernel_start;
extern uint32_t __kernel_end;

/* Multiboot information structure, through mmap_addr. */
typedef struct __attribute__((packed))
{
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
} multiboot_info_t;

/* Multiboot memory map entry. */
typedef struct __attribute__((packed))
{
    uint32_t size;
    uint64_t address;
    uint64_t length;
    uint32_t type;
} multiboot_mmap_entry_t;

/* Multiboot module descriptor. */
typedef struct __attribute__((packed))
{
    uint32_t start;
    uint32_t end;
    uint32_t string;
    uint32_t reserved;
} multiboot_module_t;

static void bitmap_set(uint32_t frame)
{
    if (frame >= MAX_FRAMES)
        return;

    uint32_t index = frame / 8;
    uint8_t bit = frame % 8;

    memory_bitmap[index] |= (1 << bit);
}

static void bitmap_clear(uint32_t frame)
{
    if (frame >= MAX_FRAMES)
        return;

    uint32_t index = frame / 8;
    uint8_t bit = frame % 8;

    memory_bitmap[index] &= ~(1 << bit);
}

static uint8_t bitmap_test(uint32_t frame)
{
    if (frame >= MAX_FRAMES)
        return 1;

    uint32_t index = frame / 8;
    uint8_t bit = frame % 8;

    return (memory_bitmap[index] & (1 << bit)) != 0;
}

/* Reserve every frame intersecting [start, end). */
static void reserve_memory(uint64_t start, uint64_t end)
{
    if (end <= start)
        return;

    uint64_t first = start / PAGE_SIZE;
    uint64_t last = (end + PAGE_SIZE - 1) / PAGE_SIZE;

    if (first >= MAX_FRAMES)
        return;

    if (last > MAX_FRAMES)
        last = MAX_FRAMES;

    for (uint64_t frame = first; frame < last; frame++)
    {
        if (!bitmap_test((uint32_t)frame))
        {
            bitmap_set((uint32_t)frame);
            used_frames++;
        }
    }
}

/* Release only complete frames within a usable memory range. */
static void release_memory(uint64_t start, uint64_t end)
{
    if (end <= start)
        return;

    uint64_t first = (start + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t last = end / PAGE_SIZE;

    if (first >= MAX_FRAMES)
        return;

    if (last > MAX_FRAMES)
        last = MAX_FRAMES;

    for (uint64_t frame = first; frame < last; frame++)
    {
        if (bitmap_test((uint32_t)frame))
        {
            bitmap_clear((uint32_t)frame);
            used_frames--;
        }
    }
}

void memory_initialize(uint32_t multiboot_magic,
                       uint32_t multiboot_info_address)
{
    terminal_write("\n[PINECONE] Initializing memory manager...\n");

    /* Initially mark all memory as unavailable. */
    for (uint32_t i = 0; i < BITMAP_SIZE; i++)
        memory_bitmap[i] = 0xFF;

    total_frames = MAX_FRAMES;
    used_frames = MAX_FRAMES;

    if (multiboot_magic != MULTIBOOT_BOOTLOADER_MAGIC)
    {
        terminal_write("[MEMORY] Invalid Multiboot boot signature.\n");
        return;
    }

    multiboot_info_t* info =
        (multiboot_info_t*)multiboot_info_address;

    if (!(info->flags & MULTIBOOT_INFO_MEMORY_MAP))
    {
        terminal_write("[MEMORY] GRUB did not provide a memory map.\n");
        return;
    }

    uint32_t offset = 0;

    while (offset < info->mmap_length)
    {
        multiboot_mmap_entry_t* entry =
            (multiboot_mmap_entry_t*)(info->mmap_addr + offset);

        uint64_t end = entry->address + entry->length;

        /* Type 1 represents available RAM. */
        if (entry->type == 1 && end >= entry->address)
        {
            release_memory(entry->address, end);
        }

        offset += entry->size + sizeof(entry->size);
    }

    /* Keep low memory unavailable. */
    reserve_memory(0, 0x100000);

    /* Protect the complete loaded kernel, including its bitmap. */
    reserve_memory(
        (uint32_t)&__kernel_start,
        (uint32_t)&__kernel_end
    );

    /* Protect the Multiboot information and memory map. */
    reserve_memory(
        multiboot_info_address,
        (uint64_t)multiboot_info_address + sizeof(multiboot_info_t)
    );

    reserve_memory(
        info->mmap_addr,
        (uint64_t)info->mmap_addr + info->mmap_length
    );

    /* Protect any GRUB-loaded modules. */
    if (info->flags & MULTIBOOT_INFO_MODULES)
    {
        multiboot_module_t* modules =
            (multiboot_module_t*)info->mods_addr;

        for (uint32_t i = 0; i < info->mods_count; i++)
        {
            reserve_memory(modules[i].start, modules[i].end);
        }

        reserve_memory(
            info->mods_addr,
            (uint64_t)info->mods_addr +
            (uint64_t)info->mods_count * sizeof(multiboot_module_t)
        );
    }

    terminal_write("[MEMORY] Memory bitmap initialized.\n");
    terminal_write("[MEMORY] Physical frame allocator ready.\n");
}

uint32_t memory_allocate_frame(void)
{
    for (uint32_t frame = 0; frame < MAX_FRAMES; frame++)
    {
        if (!bitmap_test(frame))
        {
            bitmap_set(frame);
            used_frames++;

            return frame * PAGE_SIZE;
        }
    }

    return 0;
}

void memory_free_frame(uint32_t address)
{
    if (address == 0 || address % PAGE_SIZE != 0)
        return;

    uint32_t frame = address / PAGE_SIZE;

    if (frame >= MAX_FRAMES)
        return;

    if (bitmap_test(frame))
    {
        bitmap_clear(frame);
        used_frames--;
    }
}

uint32_t memory_get_total_frames(void)
{
    return total_frames;
}

uint32_t memory_get_used_frames(void)
{
    return used_frames;
}

uint32_t memory_get_free_frames(void)
{
    return total_frames - used_frames;
}