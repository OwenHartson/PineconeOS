#ifndef PINECONE_MEMORY_H
#define PINECONE_MEMORY_H

#include <stdint.h>

#define PAGE_SIZE 4096

void memory_initialize(uint32_t multiboot_magic,
                       uint32_t multiboot_info_address);

uint32_t memory_allocate_frame(void);

void memory_free_frame(uint32_t address);

uint32_t memory_get_total_frames(void);

uint32_t memory_get_used_frames(void);

uint32_t memory_get_free_frames(void);

#endif