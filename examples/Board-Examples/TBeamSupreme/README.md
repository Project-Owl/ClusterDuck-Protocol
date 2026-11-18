# T-Beam Supreme L76K / SX1262 bring-up

Build from the CDP repository root:

```sh
pio run -e local_lilygo_t_beam_supreme_sx1262
```

The named environment selects this example, the explicit Supreme board macro,
8 MB flash/QSPI PSRAM settings and XPowersLib. The example powers the radio's
AXP2101 ALDO3 rail before CDP setup, then runs a MamaDuck. It does not initialize
the tracker application, GPS, logger, display, IMU or LED ring. Set a unique
eight-byte ID before using multiple copies. Queue admission is not delivery;
compile success is not a hardware validation result.

| Function | Pins / ownership |
| --- | --- |
| Radio | Global SPI: SCK12, MISO13, MOSI11, CS10; RESET5, DIO1=1, BUSY4 |
| PMU / RTC | Wire1: SDA42, SCL41; PMU IRQ40, RTC IRQ14 |
| L76K | Host RX9, TX8; wake7, PPS6; GNSS power ALDO4 |
| OLED / BME / magnetometer | Wire: SDA17, SCL18; SH1106 128x64; shared ALDO1 |
| SD / IMU | Separate SPIClass(HSPI): SCK36, MISO37, MOSI35; SD CS47, IMU CS34/IRQ33 |
| BOOT | GPIO0, active low; holding during reset enters the ROM downloader |
| External ring DIN | GPIO44, CNC1 pin4 / PM1 pin12; reserve it from UART0 |

In the tracker, create `SPIClass peripheralSpi(HSPI)`, enable sensor ALDO2 and
SD BLDO1 before driving their I/O, then start that bus with the peripheral
pins. Use `SD.begin(CDPCFG_PIN_SD_CS, peripheralSpi)` and pass the same SPI
instance to the IMU driver. Keep unselected chip selects high. Do not call
global `SPI.begin()` on the SD/IMU pins: global SPI is owned by the radio.
The radio-only example leaves SD/IMU pins untouched to avoid back-powering
unpowered peripherals. The application owns all accessory power policy.

Battery measurements come from AXP2101; no GPIO2 ADC divider or GPIO48 onboard
RGB LED is assumed. The ring pin does not enable or power a ring. Keep USB CDC
as the console and do not initialize UART0 on GPIO43/44 when GPIO44 is DIN.
Verify ring supply and logic levels; the connector's 3.3 V pin is not an
assumed 5 V ring supply.

Sources: [LILYGO pin/power table](https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series/blob/master/docs/en/t_beam_supreme/t_beam_supreme_hw.md),
[V3.1 schematic](https://github.com/Xinyuan-LilyGO/LilyGo-LoRa-Series/blob/master/schematic/T-Beam-S3-Supreme/T-Beam-S3-Supreme-V3.1.pdf).
