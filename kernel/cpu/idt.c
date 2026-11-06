
#include "../include/idt.h"
#include "../include/io.h"
#include "../include/terminal.h"
#include "../include/keyboard.h"

struct idt_entry {
    uint16_t base_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_pointer {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct idt_entry idt[256];
static struct idt_pointer idtr;

extern void idt_load(uint32_t);
extern uint32_t isr_stub_table[48];

static void idt_set_gate(
    uint8_t number,
    uint32_t base,
    uint16_t selector,
    uint8_t flags
) {
    idt[number].base_low = base & 0xFFFF;
    idt[number].base_high = (base >> 16) & 0xFFFF;

    idt[number].selector = selector;
    idt[number].zero = 0;
    idt[number].flags = flags;
}

/* Remap the legacy PIC */
static void pic_remap(void)
{
    outb(0x20, 0x11);
    io_wait();

    outb(0xA0, 0x11);
    io_wait();

    outb(0x21, 0x20);
    io_wait();

    outb(0xA1, 0x28);
    io_wait();

    outb(0x21, 0x04);
    io_wait();

    outb(0xA1, 0x02);
    io_wait();

    outb(0x21, 0x01);
    io_wait();

    outb(0xA1, 0x01);
    io_wait();

    /* Enable keyboard IRQ only */
    outb(0x21, 0xFD);
    outb(0xA1, 0xFF);
}

void idt_initialize(void)
{
    idtr.limit = sizeof(idt) - 1;
    idtr.base = (uint32_t)&idt;

    for (int i = 0; i < 48; i++)
    {
        idt_set_gate(
            i,
            isr_stub_table[i],
            0x08,
            0x8E
        );
    }

    pic_remap();

    idt_load((uint32_t)&idtr);
}

void interrupt_dispatch(uint32_t* frame)
{
    uint32_t interrupt_number = frame[12];

    /* CPU exception */
    if (interrupt_number < 32)
    {
        terminal_write("\n\n");
        terminal_write("PINECONEOS KERNEL EXCEPTION\n");
        terminal_write("Interrupt: ");

        terminal_write("CPU fault detected.\n");
        terminal_write("System halted.\n");

        __asm__ volatile ("cli");

        while (1)
            __asm__ volatile ("hlt");
    }

    /* Keyboard interrupt */
    if (interrupt_number == 33)
    {
        keyboard_irq_handler();
    }

    /* Send end-of-interrupt to the PIC */
    if (interrupt_number >= 40)
        outb(0xA0, 0x20);

    if (interrupt_number >= 32)
        outb(0x20, 0x20);
}
