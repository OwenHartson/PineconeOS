#include <stdint.h>

#include "../include/heap.h"
#include "../include/terminal.h"

#define HEAP_ALIGNMENT 16
#define HEAP_MAGIC 0x50494E45

#define ALIGN_UP(value, alignment) \
    (((value) + ((alignment) - 1)) & ~((alignment) - 1))

typedef struct heap_block
{
    uint32_t size;
    uint32_t is_free;
    uint32_t magic;

    struct heap_block* previous;
    struct heap_block* next;

} heap_block_t;

/*
 * The heap is reserved inside the kernel's BSS section.
 * It is automatically zero-initialized by the loader.
 */
static uint8_t kernel_heap[PINECONE_HEAP_SIZE]
    __attribute__((aligned(HEAP_ALIGNMENT)));

static heap_block_t* first_block = 0;

static uint32_t heap_initialized = 0;

static uint32_t heap_header_size(void)
{
    return ALIGN_UP(sizeof(heap_block_t), HEAP_ALIGNMENT);
}

static uint32_t heap_start_address(void)
{
    return (uint32_t)kernel_heap;
}

static uint32_t heap_end_address(void)
{
    return (uint32_t)(kernel_heap + PINECONE_HEAP_SIZE);
}

static void merge_with_next(heap_block_t* block)
{
    if (!block || !block->next)
        return;

    heap_block_t* next = block->next;

    if (!block->is_free || !next->is_free)
        return;

    if (block->magic != HEAP_MAGIC ||
        next->magic != HEAP_MAGIC)
        return;

    block->size += next->size;

    block->next = next->next;

    if (next->next)
        next->next->previous = block;

    next->magic = 0;
}

void heap_print_status(void)
{
    terminal_write("\n[PINECONE HEAP STATUS]\n");
    terminal_write("Heap initialized and operational.\n");
}

void heap_initialize(void)
{
    terminal_write("\n[PINECONE] Initializing kernel heap...\n");

    uint32_t header_size = heap_header_size();

    first_block = (heap_block_t*)kernel_heap;

    first_block->size = PINECONE_HEAP_SIZE;
    first_block->is_free = 1;
    first_block->magic = HEAP_MAGIC;
    first_block->previous = 0;
    first_block->next = 0;

    heap_initialized = 1;

    terminal_write("[HEAP] Heap memory reserved: 256 KB\n");
    terminal_write("[HEAP] Block allocator initialized.\n");

    (void)header_size;
}

void* kmalloc(uint32_t size)
{
    if (!heap_initialized || size == 0)
        return 0;

    if (size > PINECONE_HEAP_SIZE)
        return 0;

    uint32_t header_size = heap_header_size();
    uint32_t required_size = ALIGN_UP(size, HEAP_ALIGNMENT);

    if (required_size > PINECONE_HEAP_SIZE - header_size)
        return 0;

    heap_block_t* current = first_block;

    while (current)
    {
        if (current->magic != HEAP_MAGIC)
            return 0;

        if (current->is_free &&
            current->size >= header_size + required_size)
        {
            uint32_t remaining =
                current->size - header_size - required_size;

            /*
             * Split the block if enough space remains
             * for another block header and an aligned payload.
             */
            if (remaining >= header_size + HEAP_ALIGNMENT)
            {
                heap_block_t* new_block =
                    (heap_block_t*)((uint8_t*)current +
                    header_size + required_size);

                new_block->size = remaining;
                new_block->is_free = 1;
                new_block->magic = HEAP_MAGIC;
                new_block->previous = current;
                new_block->next = current->next;

                if (new_block->next)
                    new_block->next->previous = new_block;

                current->next = new_block;

                current->size = header_size + required_size;
            }

            current->is_free = 0;

            return (uint8_t*)current + header_size;
        }

        current = current->next;
    }

    return 0;
}

void kfree(void* pointer)
{
    if (!heap_initialized || !pointer)
        return;

    uint32_t address = (uint32_t)pointer;
    uint32_t start = heap_start_address();
    uint32_t end = heap_end_address();
    uint32_t header_size = heap_header_size();

    if (address < start + header_size || address >= end)
        return;

    if ((address - start) % HEAP_ALIGNMENT != header_size % HEAP_ALIGNMENT)
        return;

    heap_block_t* block =
        (heap_block_t*)((uint8_t*)pointer - header_size);

    if ((uint32_t)block < start ||
        (uint32_t)block + header_size > end)
        return;

    if (block->magic != HEAP_MAGIC || block->is_free)
        return;

    block->is_free = 1;

    /*
     * Merge with adjacent free blocks.
     */
    merge_with_next(block);

    if (block->previous && block->previous->is_free)
    {
        block = block->previous;
        merge_with_next(block);
    }
}

uint32_t heap_get_total_size(void)
{
    return PINECONE_HEAP_SIZE;
}

uint32_t heap_get_used_size(void)
{
    if (!heap_initialized)
        return 0;

    uint32_t used = 0;
    uint32_t header_size = heap_header_size();

    heap_block_t* current = first_block;

    while (current)
    {
        if (current->magic != HEAP_MAGIC)
            break;

        if (!current->is_free)
            used += current->size - header_size;

        current = current->next;
    }

    return used;
}

uint32_t heap_get_free_size(void)
{
    uint32_t total = heap_get_total_size();
    uint32_t used = heap_get_used_size();

    return total - used;
}