#ifndef KEYBOARD_H
#define KEYBOARD_H

#define KEYBOARD_CHAR_HOME 0x01
#define KEYBOARD_CHAR_LEFT 0x02
#define KEYBOARD_CHAR_DELETE 0x04
#define KEYBOARD_CHAR_END 0x05
#define KEYBOARD_CHAR_RIGHT 0x06

void keyboard_init(void);
int keyboard_read_char(char *out);
int keyboard_graphical_ready(void);
const char *keyboard_source(void);

#endif
