#ifndef PINECONE_HEAP_H
#define PINECONE_HEAP_H

#include <stdint.h>

#define PINECONE_HEAP_SIZE (256 * 1024)

void heap_print_status(void);

void heap_initialize(void);

void* kmalloc(uint32_t size);

void kfree(void* pointer);

uint32_t heap_get_total_size(void);

uint32_t heap_get_used_size(void);

uint32_t heap_get_free_size(void);

#endif