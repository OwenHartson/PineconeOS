#ifndef PINECONE_PIT_H
#define PINECONE_PIT_H

#include <stdint.h>

#define PIT_DEFAULT_FREQUENCY 100

void pit_initialize(uint32_t frequency);

void pit_interrupt_handler(void);

uint32_t pit_get_ticks(void);

uint32_t pit_get_frequency(void);

uint32_t pit_get_seconds(void);

#endif