#ifndef KEYBOARD_H
#define KEYBOARD_H

void keyboard_init(void);
int keyboard_read_char(char *out);
int keyboard_graphical_ready(void);
const char *keyboard_source(void);

#endif
