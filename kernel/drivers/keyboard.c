
#include "../include/keyboard.h"
#include "../include/io.h"

#define BUFFER_SIZE 128

static char buffer[BUFFER_SIZE];

static volatile unsigned int head = 0;
static volatile unsigned int tail = 0;

/* Basic US keyboard scancode map */
static const char keymap[128] = {
    0, 27,
    '1','2','3','4','5','6','7','8','9','0',
    '-','=',
    '\b',
    '\t',
    'q','w','e','r','t','y','u','i','o','p',
    '[',']',
    '\n',
    0,
    'a','s','d','f','g','h','j','k','l',
    ';','\'','`',
    0,'\\',
    'z','x','c','v','b','n','m',
    ',','.','/',
    0,
    '*',
    0,
    ' ',
};

void keyboard_irq_handler(void)
{
    uint8_t scancode = inb(0x60);

    /* Ignore key releases */
    if (scancode & 0x80)
        return;

    if (scancode >= 128)
        return;

    char c = keymap[scancode];

    if (c == 0)
        return;

    unsigned int next = (head + 1) % BUFFER_SIZE;

    if (next != tail)
    {
        buffer[head] = c;
        head = next;
    }
}

char keyboard_getchar(void)
{
    while (head == tail)
    {
        __asm__ volatile ("hlt");
    }

    char c = buffer[tail];

    tail = (tail + 1) % BUFFER_SIZE;

    return c;
}

void keyboard_initialize(void)
{
    head = 0;
    tail = 0;
}
