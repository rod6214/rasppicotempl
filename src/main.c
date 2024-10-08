#include <ctype.h>
#include <pico/stdlib.h>
#include <hardware/i2c.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include "graphics.h"
#include "sd_volume.h"
#include "sd_file.h"
// #include "sd_driver.h"

// static const uint I2C_MASTER_SDA_PIN = 6;
// static const uint I2C_MASTER_SCL_PIN = 7;

// static uint8_t gray_image[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x40, 0xe0, 0x10, 0xf0, 0xa8, 0x58, 0xe8, 0x54, 0xbc, 0x64, 0xdc, 0xb4, 0xe8, 0xbc, 0x48, 0xf8, 0xd0, 0xb0, 0x60, 0xc0, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x94, 0x29, 0x56, 0x89, 0xb6, 0x03, 0x00, 0xd9, 0x24, 0x54, 0x0c, 0x44, 0xcc, 0x24, 0xc4, 0x1c, 0x14, 0xe0, 0xb9, 0x00, 0x07, 0xfd, 0x5b, 0xa6, 0xff, 0x54, 0xb0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0a, 0x41, 0x14, 0x42, 0x14, 0x40, 0x80, 0x0a, 0x25, 0x18, 0x20, 0x13, 0x12, 0x21, 0x2a, 0x10, 0x2c, 0x17, 0x94, 0x80, 0x60, 0x9f, 0xf5, 0x2a, 0xd7, 0x3d, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x0a, 0x00, 0x04, 0x11, 0x04, 0x09, 0x22, 0x15, 0x00, 0x17, 0x08, 0x13, 0x05, 0x2a, 0x05, 0x0a, 0x03, 0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};

static sd_volume volume;
static sd_file rootdir;

int main() {

    sd_file file = {0};
    
    uint8_t volresult = sd_volume_init(&volume);
    hard_assert(volresult);

    uint8_t rootresult = openRoot(&rootdir, &volume);
    hard_assert(rootresult);

    uint8_t fileresult = sd_open(&rootdir, &file, "boot", O_READ);
    hard_assert(fileresult);

    int16_t res = 0, i = 0;
    char buffer[32] = {0};

    while ((res = sd_read(&file))>-1) {
        buffer[i++] = (char)res;
    }

    buffer[i] = '\0';

    sd_close(&file);

    // uint8_t buffer[1024]={0};
    // init_sd_core();
    // test boot file xD
    // uint8_t result = readData(0x00000,0,512,buffer);
    // hard_assert(result==TRUE);
    // result = readData(10,0,5,buffer);
    // hard_assert(result==TRUE);
    // uint8_t addressingMode[2] = {0x00,0x20};

    // initDisplay1306_v1();

    // renderPixels(2, 0, 200, 8, NULL,0x00);
    // renderPixels(10, 0, 31, 4, gray_image,0);
    // renderPixels(2, 0, 128, 8, NULL,0x00);

    // uint8_t buffer[sizeof(font)] = {0};
    // char str[] = "HOLA MUNDO 2";

    // writeString(buffer, 0,0,str);

    // size_t len = sizeof(str);
    // renderPixels(16, 5, 8*len, 1, buffer, 0);
    
    // renderPixels(2, 0, 200, 8, NULL,0x00);
    return 0;
}
