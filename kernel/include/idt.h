
#ifndef PINECONE_IDT_H
#define PINECONE_IDT_H

#include <stdint.h>

void idt_initialize(void);
void interrupt_dispatch(uint32_t* frame);

#endif
