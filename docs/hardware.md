# Hardware and wiring

## Supported board

The firmware targets the Waveshare RP2350 Touch AMOLED 2.41 with its integrated 450x600 display
and FT6336U touch controller.

## PCM5102A audio

| Function | RP2350 GPIO |
| --- | ---: |
| PCM5102A BCK | GP26 |
| PCM5102A LRCK | GP27 |
| PCM5102A DIN | GP28 |

Connect ground between the RP2350 board, DAC, and downstream audio equipment. Connect the DAC's
SCK input to ground because the PCM5102A derives its internal clock from BCK.

The common purple GY-PCM5102A module requires these hardware-mode straps:

| Jumper | Setting | Function |
| --- | --- | --- |
| H1 | L | Normal filter |
| H2 | L | De-emphasis disabled |
| H3 | H | XSMT unmuted |
| H4 | L | Standard I2S format |

On a three-pad H/L jumper, bridge the center pad to only the indicated side. An open H3 leaves
XSMT undefined and can mute the analog output. Modules exposing pins instead of solder jumpers
use FLT, DEMP, and FMT at ground and XSMT at 3.3 V. Do not drive these logic inputs with 5 V.

Power the DAC according to the markings and regulator fitted to the exact module. Breakout boards
sold under the same name do not always have the same supply arrangement.

## microSD card

| Function | RP2350 GPIO |
| --- | ---: |
| SD SCK | GP18 |
| SD MOSI | GP19 |
| SD MISO | GP20 |
| SD CS | GP23 |

GP21 and GP22 receive pull-ups for the unused DAT1 and DAT2 lines. The firmware uses SPI mode,
mounts one volume, and never writes to or formats the card.

Use a FAT12, FAT16, or FAT32 card with 512-byte sectors. FAT32 is the recommended format for
ordinary microSD cards. exFAT is not enabled.

## Fixed onboard connections

The custom Pico SDK board definition uses UART0 on GP0/GP1 and I2C1 on GP6/GP7. The integrated
display, touch controller, QSPI transport, and optional PSRAM use the Waveshare board wiring and
must not be reassigned without updating the matching driver and board definition together.
