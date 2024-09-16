#include "graphics.h"

#define SLAVE_ADDRESS 0x3C
#define DISPLAY_OFF 0xAE
#define DISPLAY_ON 0xAF
#define RAM_ENTERY_DISPLAY_ON 0xA4
#define ENTERY_DISPLAY_ON 0xA5
#define SET_STARTLINE_ADDRESS 0x40

static const uint I2C_MASTER_SDA_PIN = 6;
static const uint I2C_MASTER_SCL_PIN = 7;

void initDisplay1306_v1() {
    gpio_init(I2C_MASTER_SCL_PIN);
    gpio_set_function(I2C_MASTER_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_MASTER_SCL_PIN);
    
    gpio_init(I2C_MASTER_SDA_PIN);
    gpio_set_function(I2C_MASTER_SDA_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_MASTER_SDA_PIN);
    i2c_init(i2c1, 400000); 

    uint8_t cmds[] = {
        DISPLAY_OFF,
        SET_STARTLINE_ADDRESS,
        RAM_ENTERY_DISPLAY_ON,
        DISPLAY_ON
    };

    sendCommands(cmds, count_of(cmds));
}

void sendCommands(uint8_t* buffer, int commandLens) {
    uint8_t temp[2] = {0x80, 0};
    uint8_t* ptr = buffer;
    for (int i = 0; i < commandLens; i++) {
        temp[1] = *ptr++;
        int count = i2c_write_blocking(i2c1, SLAVE_ADDRESS, temp, 2, false);
        hard_assert(count == 2);
    }
}

void renderPixels(uint8_t x, uint8_t y, uint8_t width, uint8_t heitgh, uint8_t* pimg, uint8_t rep) {
    int i = 0, j = 0, resolution = width * heitgh;
    uint8_t page = y;
    uint8_t* ptr = pimg;

    int max = width * (heitgh);

    setRawPixelPos(page++, 0 + x);

    if (max < resolution) {
        resolution = max; 
    }

    do {
        if (!ptr) {
            renderHorizontal(rep);
        }
        else {
            renderHorizontal(*ptr++);
        }
        j++;
        if (j>width) {
            setRawPixelPos(page++, 0 + x);
            j=0;
        }
        i++;
    } while(i<resolution);
}

void renderHorizontal(uint8_t value) {
    uint8_t linebar[2] = {0xC0, value};
    i2c_write_blocking(i2c1, 0x3C, linebar, 2, false);
}

void setRawPixelPos(uint8_t page, uint8_t column) {
    uint8_t ch = column>>4;
    uint8_t cl = column&0xf;
    uint8_t setPageAddress[2] = {0x80,(0xB0 | page)};
    uint8_t setColumnAddressL[2] = {0x80, cl};
    uint8_t setColumnAddressH[2] = {0x80,(0x10 | ch)};
    i2c_write_blocking(i2c1, 0x3C, setPageAddress, 2, false);
    i2c_write_blocking(i2c1, 0x3C, setColumnAddressL, 2, false);
    i2c_write_blocking(i2c1, 0x3C, setColumnAddressH, 2, false);
}

static uint8_t reversed[sizeof(font)] = {0};

uint8_t reverse(uint8_t b) {
   b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
   b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
   b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
   return b;
}

void fillReversedCache() {
    // calculate and cache a reversed version of fhe font, because I defined it upside down...doh!
    for (int i=0;i<sizeof(font);i++)
        reversed[i] = reverse(font[i]);
}

inline int getFontIndex(uint8_t ch) {
    if (ch >= 'A' && ch <='Z') {
        return  ch - 'A' + 1;
    }
    else if (ch >= '0' && ch <='9') {
        return  ch - '0' + 27;
    }
    else return  0; // Not got that char so space.
}

void writeChar(uint8_t *buf, int16_t x, int16_t y, uint8_t ch) {
    if (reversed[0] == 0) 
        fillReversedCache();

    // if (x > SSD1306_WIDTH - 8 || y > SSD1306_HEIGHT - 8)
    //     return;

    // For the moment, only write on Y row boundaries (every 8 vertical pixels)
    y = y/8;

    ch = toupper(ch);
    int idx = getFontIndex(ch);
    int fb_idx = y * 128 + x;

    for (int i=0;i<8;i++) {
        buf[fb_idx++] = reversed[idx * 8 + i];
    }
}

void writeString(uint8_t *buf, int16_t x, int16_t y, char *str) {
    // Cull out any string off the screen
    // if (x > SSD1306_WIDTH - 8 || y > SSD1306_HEIGHT - 8)
    //     return;

    while (*str) {
        writeChar(buf, x, y, *str++);
        x+=8;
    }
}
