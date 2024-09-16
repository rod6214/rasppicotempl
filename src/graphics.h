#ifndef __GRAPHICS_H__
#define __GRAPHICS_H__

#include <ctype.h>
#include <pico/stdlib.h>
#include <hardware/i2c.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include "ssd1306_font.h"


#ifdef ___cplusplus
extern "C" {
#endif
void initDisplay1306_v1();
void sendCommands(uint8_t* buffer, int commandLens);
void renderPixels(uint8_t x, uint8_t y, uint8_t width, uint8_t heitgh, uint8_t* pimg, uint8_t rep);
void renderHorizontal(uint8_t value);
void setRawPixelPos(uint8_t page, uint8_t column);
uint8_t reverse(uint8_t b);
void fillReversedCache();
int getFontIndex(uint8_t ch);
void writeChar(uint8_t *buf, int16_t x, int16_t y, uint8_t ch);
void writeString(uint8_t *buf, int16_t x, int16_t y, char *str);

#ifdef ___cplusplus
}
#endif

#endif