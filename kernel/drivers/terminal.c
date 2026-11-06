
#include "../include/terminal.h"
#include <stdint.h>
#include <stddef.h>

#define WIDTH 80
#define HEIGHT 25

static volatile uint16_t* const VGA =
    (volatile uint16_t*)0xB8000;

static size_t row = 0;
static size_t column = 0;

static uint8_t color = 0x0A;

static uint16_t entry(char c)
{
    return (uint16_t)c | ((uint16_t)color << 8);
}

void terminal_clear(void)
{
    for (size_t i = 0; i < WIDTH * HEIGHT; i++)
        VGA[i] = entry(' ');

    row = 0;
    column = 0;
}

static void scroll(void)
{
    if (row < HEIGHT)
        return;

    for (size_t y = 1; y < HEIGHT; y++)
    {
        for (size_t x = 0; x < WIDTH; x++)
        {
            VGA[(y - 1) * WIDTH + x] =
                VGA[y * WIDTH + x];
        }
    }

    for (size_t x = 0; x < WIDTH; x++)
        VGA[(HEIGHT - 1) * WIDTH + x] = entry(' ');

    row = HEIGHT - 1;
}

void terminal_putchar(char c)
{
    if (c == '\n')
    {
        column = 0;
        row++;
        scroll();
        return;
    }

    if (c == '\r')
    {
        column = 0;
        return;
    }

    VGA[row * WIDTH + column] = entry(c);

    column++;

    if (column >= WIDTH)
    {
        column = 0;
        row++;
        scroll();
    }
}

void terminal_write(const char* text)
{
    while (*text)
        terminal_putchar(*text++);
}

void terminal_backspace(void)
{
    if (column > 0)
    {
        column--;
    }
    else if (row > 0)
    {
        row--;
        column = WIDTH - 1;
    }
    else
    {
        return;
    }

    VGA[row * WIDTH + column] = entry(' ');
}

void terminal_initialize(void)
{
    terminal_clear();
}
