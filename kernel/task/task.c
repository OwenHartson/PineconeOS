#include <stdint.h>

#include "../include/task.h"
#include "../include/heap.h"
#include "../include/terminal.h"

#define TASK_STACK_MAGIC 0x50494E45

extern void task_context_switch(
    uint32_t* old_stack_pointer,
    uint32_t new_stack_pointer
);

static task_t* task_list = 0;
static task_t* current_task = 0;

static uint32_t next_task_id = 1;
static uint32_t scheduler_ready = 0;

static void task_bootstrap(void);
static void scheduler_switch_to(task_t* next);

/*
 * Initialize the first task.
 * This represents the original kernel execution flow.
 */
void scheduler_initialize(void)
{
    terminal_write("\n[PINECONE] Initializing task scheduler...\n");

    task_t* main_task = (task_t*)kmalloc(sizeof(task_t));

    if (!main_task)
    {
        terminal_write("[TASK] Failed to allocate main task.\n");
        return;
    }

    main_task->id = 0;
    main_task->stack_pointer = 0;
    main_task->stack = 0;
    main_task->entry_point = 0;
    main_task->state = TASK_RUNNING;
    main_task->next = main_task;

    task_list = main_task;
    current_task = main_task;

    next_task_id = 1;
    scheduler_ready = 1;

    terminal_write("[TASK] Main task registered.\n");
    terminal_write("[TASK] Scheduler initialized.\n");
}

/*
 * Create a new kernel task.
 */
int task_create(void (*entry_point)(void))
{
    if (!scheduler_ready || !entry_point)
        return 0;

    task_t* task = (task_t*)kmalloc(sizeof(task_t));

    if (!task)
        return 0;

    void* stack = kmalloc(TASK_STACK_SIZE);

    if (!stack)
    {
        kfree(task);
        return 0;
    }

    uint32_t* stack_top =
        (uint32_t*)((uint8_t*)stack + TASK_STACK_SIZE);

    /*
     * Build the initial stack frame.
     *
     * task_context_switch restores:
     * EDI, ESI, EBX, EBP, then RET.
     */
    *(--stack_top) = (uint32_t)task_bootstrap;
    *(--stack_top) = 0; /* EBP */
    *(--stack_top) = 0; /* EBX */
    *(--stack_top) = 0; /* ESI */
    *(--stack_top) = 0; /* EDI */

    task->id = next_task_id++;
    task->stack_pointer = (uint32_t)stack_top;
    task->stack = stack;
    task->entry_point = entry_point;
    task->state = TASK_READY;

    /*
     * Insert the new task at the end of the circular list.
     */
    task_t* last = task_list;

    while (last->next != task_list)
        last = last->next;

    last->next = task;
    task->next = task_list;

    terminal_write("[TASK] New task created.\n");

    return (int)task->id;
}

/*
 * Switch to another task.
 */
static void scheduler_switch_to(task_t* next)
{
    if (!next || next == current_task)
        return;

    task_t* previous = current_task;

    if (previous->state == TASK_RUNNING)
        previous->state = TASK_READY;

    next->state = TASK_RUNNING;

    current_task = next;

    task_context_switch(
        &previous->stack_pointer,
        next->stack_pointer
    );
}

/*
 * Select the next runnable task.
 */
void task_yield(void)
{
    if (!scheduler_ready || !current_task)
        return;

    task_t* next = current_task->next;

    while (next != current_task)
    {
        if (next->state == TASK_READY)
        {
            scheduler_switch_to(next);
            return;
        }

        next = next->next;
    }

    /*
     * No other runnable task was found.
     * Continue executing the current task.
     */
}

/*
 * Entry point for newly created tasks.
 */
static void task_bootstrap(void)
{
    if (!current_task || !current_task->entry_point)
        task_exit();

    current_task->entry_point();

    /*
     * If the task function returns,
     * terminate the task automatically.
     */
    task_exit();

    while (1)
        __asm__ volatile ("hlt");
}

/*
 * Terminate the currently running task.
 */
void task_exit(void)
{
    if (!scheduler_ready || !current_task)
        return;

    current_task->state = TASK_TERMINATED;

    task_yield();

    /*
     * A terminated task must never resume.
     */
    while (1)
        __asm__ volatile ("hlt");
}

uint32_t scheduler_get_current_task_id(void)
{
    if (!current_task)
        return 0;

    return current_task->id;
}