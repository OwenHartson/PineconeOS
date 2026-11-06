#ifndef PINECONE_PAGING_H
#define PINECONE_PAGING_H

#include <stdint.h>

#define PAGE_SIZE 4096

void paging_initialize(void);
uint8_t paging_is_enabled(void);

#endif