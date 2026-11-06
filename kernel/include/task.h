#ifndef PINECONE_TASK_H
#define PINECONE_TASK_H

#include <stdint.h>

#define TASK_STACK_SIZE 8192

typedef enum
{
    TASK_READY = 0,
    TASK_RUNNING,
    TASK_SLEEPING,
    TASK_TERMINATED
} task_state_t;

typedef struct task
{
    uint32_t id;

    uint32_t stack_pointer;

    void* stack;

    void (*entry_point)(void);

    task_state_t state;

    uint8_t priority;
    
    uint32_t wake_tick;

    struct task* next;

} task_t;

void scheduler_initialize(void);

int task_create(void (*entry_point)(void));

void task_yield(void);

void task_exit(void);

uint32_t scheduler_get_current_task_id(void);

void scheduler_timer_tick(void);

void task_sleep(uint32_t milliseconds);

void scheduler_set_task_priority(uint32_t task_id, uint8_t priority);

#endif