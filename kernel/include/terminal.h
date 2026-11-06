#ifndef PINECONE_TERMINAL_H
#define PINECONE_TERMINAL_H

void terminal_initialize(void);
void terminal_clear(void);
void terminal_write(const char* text);
void terminal_putchar(char c);
void terminal_backspace(void);

#endif
