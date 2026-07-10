#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <stdint.h>

#define FRAMEBUFFER_WIDTH 640
#define FRAMEBUFFER_HEIGHT 480

int framebuffer_init(void);
int framebuffer_is_ready(void);
int framebuffer_status(void);
void framebuffer_clear(uint32_t color);
void framebuffer_put_pixel(unsigned int x, unsigned int y, uint32_t color);
void framebuffer_fill_rect(unsigned int x,
                           unsigned int y,
                           unsigned int width,
                           unsigned int height,
                           uint32_t color);
void framebuffer_draw_test_pattern(void);

#endif
