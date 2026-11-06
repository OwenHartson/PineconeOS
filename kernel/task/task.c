#include <stdint.h>

#include "../include/task.h"
#include "../include/heap.h"
#include "../include/terminal.h"
#include "../include/pit.h"

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

void scheduler_set_task_priority(uint32_t task_id, uint8_t priority)
{
    if (!task_list)
        return;

    if (priority < 1)
        priority = 1;

    if (priority > 5)
        priority = 5;

    task_t* current = task_list;

    do
    {
        if (current->id == task_id)
        {
            current->priority = priority;
            return;
        }

        current = current->next;

    } while (current != task_list);
}

static void scheduler_wake_sleeping_tasks(void)
{
    uint32_t ticks = pit_get_ticks();

    if (!task_list)
        return;

    task_t* current = task_list;

    do
    {
        if (current->state == TASK_SLEEPING)
        {
            if ((int32_t)(ticks - current->wake_tick) >= 0)
            {
                current->state = TASK_READY;
            }
        }

        current = current->next;

    } while (current != task_list);
}

void scheduler_timer_tick(void)
{
    if (!scheduler_ready || !current_task)
        return;

    scheduler_wake_sleeping_tasks();

    task_yield();
}

void task_sleep(uint32_t milliseconds)
{
    if (!scheduler_ready || !current_task)
        return;

    if (milliseconds == 0)
    {
        task_yield();
        return;
    }

    uint32_t ticks =
        (milliseconds * 100 + 999) / 1000;

    if (ticks == 0)
        ticks = 1;

    /*
     * The current PIT frequency is initially 100 Hz.
     * This calculation will be generalized later.
     */
    current_task->wake_tick =
        pit_get_ticks() + ticks;

    current_task->state = TASK_SLEEPING;

    task_yield();
}

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
    main_task->priority = 1;
    main_task->wake_tick = 0;
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
    task->priority = 1;
    task->wake_tick = 0;

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
    __asm__ volatile ("sti");

    if (!current_task || !current_task->entry_point)
        task_exit();

    current_task->entry_point();

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