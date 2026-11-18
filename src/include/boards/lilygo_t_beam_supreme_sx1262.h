#pragma once

// LILYGO T-Beam Supreme ESP32-S3 / SX1262 / L76K (N8R8).
// Pin source: LILYGO LilyGo-LoRa-Series V3.1 schematic and Factory/utilities.h.
// Prefer the explicit board macro; generic ESP32S3 selection is retained for
// compatibility with existing CDP projects.
#if defined(ARDUINO_LILYGO_T_BEAM_SUPREME) || defined(ARDUINO_lilygo_t_beam_supreme) || defined(ARDUINO_ESP32S3_DEV)

#define CDP_BOARD_NAME "LilyGO T-Beam Supreme ESP32-S3 SX1262"
#define CDPCFG_RADIO_SX1262

// Global SPI belongs to the radio. IMU and SD use a separate SPIClass(HSPI).
#define CDPCFG_PIN_LORA_SCK      12
#define CDPCFG_PIN_LORA_MISO     13
#define CDPCFG_PIN_LORA_MOSI     11
#define CDPCFG_PIN_LORA_CS       10
#define CDPCFG_PIN_LORA_RST      5
#define CDPCFG_PIN_LORA_DIO0     -1
#define CDPCFG_PIN_LORA_DIO1     1
#define CDPCFG_PIN_LORA_BUSY     4

// AXP2101 PMU / PCF8563 RTC: Wire1. Radio power is ALDO3.
// Use PMU battery readings; no GPIO2 divider or GPIO48 onboard LED is assumed.
#define CDPCFG_PMU_AXP2101
#define CDPCFG_PIN_PMU_SDA       42
#define CDPCFG_PIN_PMU_SCL       41
#define CDPCFG_PIN_PMU_IRQ       40
#define CDPCFG_PIN_RTC_IRQ       14

// L76K UART from the ESP32 host's perspective; GNSS power is ALDO4.
#define CDPCFG_GPS_RX            9
#define CDPCFG_GPS_TX            8
#define CDPCFG_PIN_GPS_WAKE      7
#define CDPCFG_PIN_GPS_PPS       6

// SH1106 / BME280 / magnetometer: Wire, powered by shared ALDO1.
#ifndef ENABLE_DISPLAY
#define ENABLE_DISPLAY
#endif
#define CDPCFG_PIN_OLED_CLOCK    18
#define CDPCFG_PIN_OLED_DATA     17
#define CDPCFG_PIN_OLED_RESET    -1
#define CDPCFG_PIN_OLED_ROTATION U8G2_R0
#define CDPCFG_OLED_CLASS U8G2_SH1106_128X64_NONAME_F_HW_I2C

// Shared peripheral SPI: keep all unselected devices' CS lines high.
#define CDPCFG_PIN_PERIPHERAL_SCK  36
#define CDPCFG_PIN_PERIPHERAL_MISO 37
#define CDPCFG_PIN_PERIPHERAL_MOSI 35
#define CDPCFG_PIN_SD_CS          47
#define CDPCFG_PIN_IMU_CS         34
#define CDPCFG_PIN_IMU_INT        33
#define CDPCFG_PIN_BUTTON         0

// External ring DIN allocation, not an onboard LED. GPIO44 is exposed as
// U0RXD on CNC1/PM1. Use USB CDC for the console and reserve this pin from
// UART0. Ring supply and any data-level interface are external hardware.
#ifndef CDPCFG_PIN_LED_RING
#define CDPCFG_PIN_LED_RING       44
#endif

#endif
