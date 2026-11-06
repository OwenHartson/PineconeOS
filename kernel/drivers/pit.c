#include <stdint.h>

#include "../include/pit.h"
#include "../include/io.h"
#include "../include/task.h"
#include "../include/terminal.h"

#define PIT_INPUT_FREQUENCY 1193182

static volatile uint32_t timer_ticks = 0;
static uint32_t timer_frequency = 0;

void pit_initialize(uint32_t frequency)
{
    if (frequency < 19 || frequency > 1000)
    {
        terminal_write("[PIT] Invalid frequency. Using 100 Hz.\n");
        frequency = PIT_DEFAULT_FREQUENCY;
    }

    uint32_t divisor = PIT_INPUT_FREQUENCY / frequency;

    timer_frequency = frequency;
    timer_ticks = 0;

    terminal_write("[PIT] Configuring programmable interval timer...\n");

    /* Channel 0, low/high byte, square wave mode 3. */
    outb(0x43, 0x36);

    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);

    terminal_write("[PIT] Timer configured successfully.\n");
}

void pit_interrupt_handler(void)
{
    timer_ticks++;

    /*
     * Give the scheduler an opportunity to run
     * every timer interrupt.
     */
    scheduler_timer_tick();
}

uint32_t pit_get_ticks(void)
{
    return timer_ticks;
}

uint32_t pit_get_frequency(void)
{
    return timer_frequency;
}

uint32_t pit_get_seconds(void)
{
    if (timer_frequency == 0)
        return 0;

    return timer_ticks / timer_frequency;
}