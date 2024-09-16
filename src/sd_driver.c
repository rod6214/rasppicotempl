#include <pico/stdlib.h>
#include <pico/binary_info.h>
#include <hardware/spi.h>
#include "sd_driver.h"

static uint8_t _reading;
static uint16_t _offset;
static uint8_t _status;
static uint8_t _block;
static uint8_t _partialBlock;
static uint8_t _type;

// SD card commands
#define CMD0        0x00
#define CMD8        0x08
#define CMD9        0x09
#define CMD10       0x0A
#define CMD13       0x0D
#define CMD17       0x11
#define CMD24       0x18
#define CMD25       0x19
#define CMD32       0x20
#define CMD33       0x21
#define CMD38       0x26
#define CMD55       0x37
#define CMD58       0x3A
#define ACMD23      0x17
#define ACMD41      0x29

/** SD status */
#define R1_READY_STATE  0x00
#define R1_IDLE_STATE  0x01
#define R1_ILLEGAL_COMMAND  0x04
#define DATA_START_BLOCK  0xFE
#define STOP_TRAN_TOKEN  0xFD
#define WRITE_MULTIPLE_TOKEN  0xFC
#define DATA_RES_MASK  0x1F
#define DATA_RES_ACCEPTED  0x05

// card types
/** Standard capacity V1 SD card */
#define SD_CARD_TYPE_SD1 1
/** Standard capacity V2 SD card */
#define SD_CARD_TYPE_SD2 2
/** High Capacity SD card */
#define SD_CARD_TYPE_SDHC 3

// SD card errors
/** timeout error for command CMD0 */
#define SD_CARD_ERROR_CMD0  0x1
/** CMD8 was not accepted - not a valid SD card*/
#define SD_CARD_ERROR_CMD8  0x2
/** card returned an error response for CMD17 (read block) */
#define SD_CARD_ERROR_CMD17  0x3
/** card returned an error response for CMD24 (write block) */
#define SD_CARD_ERROR_CMD24  0x4
/**  WRITE_MULTIPLE_BLOCKS command failed */
#define SD_CARD_ERROR_CMD25  0x05
/** card returned an error response for CMD58 (read OCR) */
#define SD_CARD_ERROR_CMD58 0x06
/** SET_WR_BLK_ERASE_COUNT failed */
#define SD_CARD_ERROR_ACMD23 0x07
/** card's ACMD41 initialization process timeout */
#define SD_CARD_ERROR_ACMD41 0x08
/** card returned a bad CSR version field */
#define SD_CARD_ERROR_BAD_CSD 0x09
/** erase block group command failed */
#define SD_CARD_ERROR_ERASE = 0x0A
/** card not capable of single block erase */
#define SD_CARD_ERROR_ERASE_SINGLE_BLOCK 0x0B
/** Erase sequence timed out */
#define SD_CARD_ERROR_ERASE_TIMEOUT 0x0C
/** card returned an error token instead of read data */
#define SD_CARD_ERROR_READ 0x0D
/** read CID or CSD failed */
#define SD_CARD_ERROR_READ_REG = 0x0E
/** timeout while waiting for start of read data */
#define SD_CARD_ERROR_READ_TIMEOUT 0x0F
/** card did not accept STOP_TRAN_TOKEN */
#define SD_CARD_ERROR_STOP_TRAN = 0x10
/** card returned an error token as a response to a write operation */
#define SD_CARD_ERROR_WRITE = 0x11
/** attempt to write protected block zero */
#define SD_CARD_ERROR_WRITE_BLOCK_ZERO 0x12
/** card did not go ready for a multiple block write */
#define SD_CARD_ERROR_WRITE_MULTIPLE = 0x13
/** card returned an error to a CMD13 status check after a write */
#define SD_CARD_ERROR_WRITE_PROGRAMMING 0x14
/** timeout occurred during write programming */
#define SD_CARD_ERROR_WRITE_TIMEOUT 0x15
/** incorrect rate selected */
#define SD_CARD_ERROR_SCK_RATE 0x16

/** Protect block zero from write if nonzero */
#define SD_PROTECT_BLOCK_ZERO 1
/** init timeout ms */
#define SD_INIT_TIMEOUT 2000
/** erase timeout ms */
#define SD_ERASE_TIMEOUT 10000
/** read timeout ms */
#define SD_READ_TIMEOUT 300
/** write time out ms */
#define SD_WRITE_TIMEOUT 600



static inline void send_spi_data(uint8_t value) {
    uint8_t buffer[] = {value};
    // sleep_ms(10);
    spi_write_blocking(spi0, buffer, 1);
}

static inline uint8_t get_response() {
    uint8_t buffer[] = {0};
    // sleep_ms(10);
    spi_read_blocking(spi0, 0xff, buffer, 1);
    return buffer[0];
}

uint8_t init_sd_core() {

    _partialBlock = _status = _offset = _reading = 0;

    spi_init(spi0, 250 * 1000);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_pull_up(PIN_MISO);
    gpio_pull_up(PIN_SCK);
    gpio_pull_up(PIN_MOSI);
    gpio_init(PIN_CS);
    gpio_set_dir(PIN_CS, GPIO_OUT);

    chip_select_high();

    for (uint8_t i = 0; i < 10; i++) {
        send_spi_data(0xff);
    }

    chip_select_low();

    while((_status = cardCommand(CMD0, 0)) != R1_IDLE_STATE) {
        asm("nop");
    }

    if ((cardCommand(CMD8, 0x1AA)) & R1_ILLEGAL_COMMAND) {
        asm("nop");
        // set here sd card type V1
        _type = SD_CARD_TYPE_SD1;
    }
    else {
        for (uint8_t i = 0; i < 4; i++) {
            _status = get_response();
        }

        if (_status != 0xaa) {
            // throw error here
            asm("nop");
        }

        // set here sd card type V2
        _type = SD_CARD_TYPE_SD2;
    }

    uint32_t arg = (_type == SD_CARD_TYPE_SD2) ? 0x40000000 : 0;

    // int wait_time = 5;
    
    while(((_status = cardAcmd(ACMD41, arg)) != R1_READY_STATE)) {
        asm("nop");
        // wait_time--;
    }

    // if SD2 read OCR register to check for SDHC card
    if (_type == SD_CARD_TYPE_SD2) {
        if (cardCommand(CMD58, 0)) {
            asm("nop");
        }

        if ((get_response() & 0xC0) == 0xC0) {
            // type(SD_CARD_TYPE_SDHC);
            _type = SD_CARD_TYPE_SDHC;
        }
        // discard rest of ocr - contains allowed voltage range
        for (uint8_t i = 0; i < 3; i++) {
            get_response();
        }
    }

    chip_select_high();
    return TRUE;
fail:
    chip_select_high();
    return FALSE;
}

uint8_t cardAcmd(uint8_t cmd, uint32_t arg) {
    cardCommand(CMD55, 0);
    return cardCommand(cmd, arg);
}

uint8_t cardCommand(uint8_t cmd, uint32_t arg) {
    flush();

    chip_select_low();

    waitNotBusy(300);

    uint8_t args[6];
    args[0] = cmd | 0x40;

    for (int8_t s = 24, i = 1; s >= 0; s -= 8, i++) {
        args[i] = (uint8_t)(arg >> s);
    }

    uint8_t crc = 0xff;

    if (cmd == CMD0) {
        crc = 0x95;
    }

    if (cmd == CMD8) {
        crc = 0x87;
    }

    args[5] = crc;
    spi_write_blocking(spi0, args, 6);


    int i = 0;

    do {
        asm("nop");
        i++;
    }

    while((_status = get_response())&0x80 && (i != 0xff));
    return _status;
}

void flush() {
    if (!_reading) return;
    while(_offset++ < 514) {
        get_response();
    }
    chip_select_high();
    _reading = 0;
}

uint8_t readData(uint32_t block, uint16_t offset, uint16_t count, uint8_t* dst) {
    if (count == 0) {
        return TRUE;
    }
    if ((count + offset) > 512) {
        goto fail;
    }
    if (!_reading || block != _block || offset < _offset) {
        _block = block;
        // use address if not SDHC card
        if (_type != SD_CARD_TYPE_SDHC) {
            block <<= 9;
        }
        if (cardCommand(CMD17, block)) {
            //   error(SD_CARD_ERROR_CMD17);
            goto fail;
        }
        if (!waitStartBlock()) {
            goto fail;
        }
        _offset = 0;
        _reading = 1;

        // skip data before offset
        for (; _offset < offset; _offset++) {
            get_response();
        }
        // transfer data in batch
        spi_read_blocking(spi0, 0xff, dst, count);

        _offset += count;
        if (!_partialBlock || _offset >= 512) {
            // read rest of data, checksum and set chip select high
            flush();
        }
        return TRUE;

        fail:
        chip_select_high();
        return FALSE;
    }
}

uint8_t readBlock(uint32_t block, uint8_t* dst) {
  return readData(block, 0, 512, dst);
}

uint8_t writeData(uint8_t token, const uint8_t* src) {
    uint8_t temp[] = {0xff,0xff};
    spi_write_blocking(spi0, temp, 2);
    _status = get_response();
    if ((_status & DATA_RES_MASK) != DATA_RES_ACCEPTED) {
        // error(SD_CARD_ERROR_WRITE);
        // chipSelectHigh();
        chip_select_high();
        return false;
    }
    return true;
}


uint8_t writeBlock(uint32_t blockNumber, const uint8_t* src, uint8_t blocking) {
    #if SD_PROTECT_BLOCK_ZERO
    // don't allow write to first block
    if (blockNumber == 0) {
        // error(SD_CARD_ERROR_WRITE_BLOCK_ZERO);
        goto fail;
    }
    #endif  // SD_PROTECT_BLOCK_ZERO

    // use address if not SDHC card
    if (_type != SD_CARD_TYPE_SDHC) {
        blockNumber <<= 9;
    }
    if (cardCommand(CMD24, blockNumber)) {
        // error(SD_CARD_ERROR_CMD24);
        goto fail;
    }
    if (!writeData(DATA_START_BLOCK, src)) {
        goto fail;
    }
    if (blocking) {
        // wait for flash programming to complete
        if (!waitNotBusy(SD_WRITE_TIMEOUT)) {
        // error(SD_CARD_ERROR_WRITE_TIMEOUT);
            goto fail;
        }
        // response is r2 so get and check two bytes for nonzero
        if (cardCommand(CMD13, 0) || get_response()) {
        // error(SD_CARD_ERROR_WRITE_PROGRAMMING);
            goto fail;
        }
    }
    // chipSelectHigh();
    chip_select_high();
    return true;

    fail:
    //   chipSelectHigh();
    chip_select_high();
    return false;
}

uint8_t waitStartBlock() {
//   unsigned int t0 = millis();
unsigned int d = 0;
  while ((_status = get_response()) == 0xFF) {
    asm("nop");
    // unsigned int d = millis() - t0;
    if (d > SD_READ_TIMEOUT) {
    //   error(SD_CARD_ERROR_READ_TIMEOUT);
      goto fail;
    }
    d++;
    sleep_ms(1);
  }
  if (_status != DATA_START_BLOCK) {
    // error(SD_CARD_ERROR_READ);
    goto fail;
  }
  return true;

fail:
  chip_select_high();
  return false;
}

// wait for card to go not busy
uint8_t waitNotBusy(unsigned int timeoutMillis) {
//   unsigned int t0 = millis();
  unsigned int d = 0;
  do {
    if (get_response() == 0xFF) {
      return true;
    }
    sleep_ms(1);
    d++;
    // d = millis() - t0;
  } while (d < timeoutMillis);
  return false;
}
