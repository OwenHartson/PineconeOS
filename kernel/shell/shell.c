#include "../include/terminal.h"
#include "../include/keyboard.h"
#include <stddef.h>


#define COMMAND_SIZE 128

static char command[COMMAND_SIZE];

static int string_equal(const char* a, const char* b)
{
    while (*a && *b)
    {
        if (*a != *b)
        return 0;

        a++;
        b++;
    }

    return *a == *b;
}

static void shell_prompt(void)
{
    terminal_write("pinecone> ");
}

static void shell_help(void)
{
    terminal_write("\nAvailable commands:\n\n");

    terminal_write("help   - Show commands\n");
    terminal_write("clear  - Clear terminal\n");
    terminal_write("about  - PineconeOS information\n");
    terminal_write("echo   - Display a message\n");
    terminal_write("halt   - Halt the processor\n\n");
}

static void shell_about(void)
{
    terminal_write("\n");
    terminal_write("PineconeOS\n");
    terminal_write("Version: 0.2.0\n");
    terminal_write("Architecture: x86\n");
    terminal_write("Kernel: Monolithic\n");
    terminal_write("Bootloader: GRUB\n");
    terminal_write("Status: Running\n\n");
}

static void shell_execute(char* cmd)
{
    if (string_equal(cmd, "help"))
    {
        shell_help();
    }
    else if (string_equal(cmd, "clear"))
    {
        terminal_clear();
    }
    else if (string_equal(cmd, "about"))
    {
        shell_about();
    }
    else if (string_equal(cmd, "halt"))
    {
        terminal_write("\nPineconeOS shutting down CPU...\n");

        asm volatile ("cli");

        while (1)
        {
            asm volatile ("hlt");
        }
    }
    else if (cmd[0] == 'e' &&
    cmd[1] == 'c' &&
    cmd[2] == 'h' &&
    cmd[3] == 'o' &&
    cmd[4] == ' ')
    {
        terminal_write("\n");
        terminal_write(cmd + 5);
        terminal_write("\n\n");
    }
    else if (cmd[0] != '\0')
    {
        terminal_write("\nUnknown command: ");
        terminal_write(cmd);
        terminal_write("\nType 'help' for commands.\n\n");
    }
}

void shell_run(void)
{
    size_t length = 0;

    terminal_write("\n");
    terminal_write("====================================\n");
    terminal_write("       PINECONEOS COMMAND SHELL\n");
    terminal_write("====================================\n");
    terminal_write("Type 'help' to see available commands.\n\n");

    shell_prompt();

    while (1)
    {
        char c = keyboard_getchar();

        if (c == '\n')
        {
            terminal_putchar('\n');

            command[length] = '\0';

            shell_execute(command);

            length = 0;

            shell_prompt();
        }
        else if (c == '\b')
        {
            if (length > 0)
            {
                length--;
                terminal_backspace();
            }
        }
        else if (length < COMMAND_SIZE - 1)
        {
            command[length++] = c;
            terminal_putchar(c);
        }
    }
}