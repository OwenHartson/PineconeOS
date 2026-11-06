#ifndef PINECONE_KEYBOARD_H
#define PINECONE_KEYBOARD_H

void keyboard_initialize(void);
void keyboard_irq_handler(void);
char keyboard_getchar(void);

#endif
