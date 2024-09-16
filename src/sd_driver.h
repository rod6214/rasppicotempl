#ifndef __SD_DRIVER_H
#define __SD_DRIVER_H

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define PIN_MISO 4
#define PIN_CS   5
#define PIN_SCK  2
#define PIN_MOSI 3

#define chip_select_high() gpio_put(PIN_CS, TRUE)
#define chip_select_low() gpio_put(PIN_CS, FALSE)

#ifdef __cplusplus
extern "C" {
#endif
uint8_t init_sd_core();
void flush();
uint8_t cardCommand(uint8_t cmd, uint32_t arg);
uint8_t cardAcmd(uint8_t cmd, uint32_t arg);
uint8_t readData(uint32_t block, uint16_t offset, uint16_t count, uint8_t* dst);
uint8_t waitStartBlock();
uint8_t waitNotBusy(unsigned int timeoutMillis);
uint8_t writeData(uint8_t token, const uint8_t* src);
uint8_t writeBlock(uint32_t blockNumber, const uint8_t* src, uint8_t blocking);
uint8_t readBlock(uint32_t block, uint8_t* dst);
#ifdef __cplusplus
}
#endif


#endif