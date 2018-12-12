#ifndef SdFatConfig_h
#define SdFatConfig_h
#include <stdint.h>
#ifdef __arm__
#define USE_SEPARATE_FAT_CACHE 1
#else  // __arm__
#define USE_SEPARATE_FAT_CACHE 0
#endif  // __arm__
#if defined(RAMEND) && RAMEND < 3000
#define USE_MULTI_BLOCK_SD_IO 0
#else
#define USE_MULTI_BLOCK_SD_IO 1
#endif
#if defined(__arm__) && defined(CORE_TEENSY)
#define USE_NATIVE_MK20DX128_SPI 1
#else
#define USE_NATIVE_MK20DX128_SPI 0
#endif
#if defined(__arm__) && !defined(CORE_TEENSY)
#define USE_NATIVE_SAM3X_SPI 1
#else
#define USE_NATIVE_SAM3X_SPI 0
#endif
#define USE_SD_CRC 0
#define USE_MULTIPLE_CARDS 0
#define DESTRUCTOR_CLOSES_FILE 0
#define USE_SERIAL_FOR_STD_OUT 0
#define ENDL_CALLS_FLUSH 0
#define ALLOW_DEPRECATED_FUNCTIONS 0
#define FAT12_SUPPORT 0
#define SPI_SD_INIT_RATE 11
#define MEGA_SOFT_SPI 0
#define LEONARDO_SOFT_SPI 0
#define USE_SOFTWARE_SPI 0
uint8_t const SOFT_SPI_CS_PIN = 10;
/** Software SPI Master Out Slave In pin */
uint8_t const SOFT_SPI_MOSI_PIN = 11;
/** Software SPI Master In Slave Out pin */
uint8_t const SOFT_SPI_MISO_PIN = 12;
/** Software SPI Clock pin */
uint8_t const SOFT_SPI_SCK_PIN = 13;
#endif  // SdFatConfig_h
