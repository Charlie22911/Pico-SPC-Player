# Building and flashing

## Requirements

- Raspberry Pi Pico SDK 2.2.0 with submodules
- CMake 3.20 or newer
- Ninja
- Arm GNU embedded toolchain with RP2350/Cortex-M33 support
- Python 3, used by the Pico SDK build tools

Clone the SDK recursively or initialize its submodules after checkout:

```sh
git clone --branch 2.2.0 --recurse-submodules https://github.com/raspberrypi/pico-sdk.git
```

## Public build

The supported target is the RP2350's Arm Cortex-M33 cores (`rp2350-arm-s`).
The default configuration embeds no music:

```sh
cmake -S . -B build -G Ninja \
  -DPICO_SDK_PATH=/opt/pico-sdk \
  -DPICO_PLATFORM=rp2350-arm-s -DCMAKE_BUILD_TYPE=Release
cmake --build build --target pico_spc_player
```

The resulting firmware is `build/Pico-SPC-Player.uf2`.

Public releases also provide checksums and a `Pico-SPC-Player-*-relink.tar.gz`
archive containing matching object files, libraries, linker scripts, emulator
source, and relinking instructions. The complete corresponding project source
is available at the release tag.

From WSL, both the repository and SDK may use Linux paths:

```sh
cd /mnt/c/path/to/Pico-SPC-Player
cmake -S . -B build -G Ninja \
  -DPICO_SDK_PATH=$HOME/pico-sdk \
  -DPICO_PLATFORM=rp2350-arm-s -DCMAKE_BUILD_TYPE=Release
cmake --build build --target pico_spc_player
```

## Emulator code placement

The DSP loop and SPC interpreter execute from internal SRAM by default.
The SDK copies their code from flash at boot. Configure with
`-DPICO_SPC_EMULATOR_IN_SRAM=OFF` to keep these functions in flash. Host builds
use their normal code placement. See [the Arm optimization results](aram-optimization-build.md)
for the rendering, scheduling, display palette, and SRAM placement changes and results.

## Clock configuration

The release configures both system and peripheral clocks to 250 MHz. Flash uses divider 2
for 125 MHz; PSRAM uses divider 3 for approximately 83.33 MHz in linear mode. The display's
PIO clock divider is 1, with two instruction cycles per serial clock, giving 125 MHz.
PSRAM is checked at startup but is not used for runtime audio or ARAM-map storage.

Startup diagnostics report the requested clocks, SDK values, measurements against the
12 MHz reference clock, and QMI timing registers. Playback starts only after the system,
peripheral, and flash clock checks pass.

## Optional private embedded track

An embedded track is useful when testing without an SD card. Configure a separate build tree and
pass an absolute path:

```sh
cmake -S . -B build-embedded -G Ninja \
  -DPICO_SDK_PATH=/opt/pico-sdk \
  -DPICO_SPC_EMBEDDED_FILE=/home/user/music/test.spc
cmake --build build-embedded --target pico_spc_player
```

The embedded entry appears only when no SD card is detected. This repository does not provide or
download SPC files. SPC and WAV files are ignored by Git.

## Flashing

1. Hold the board's BOOTSEL button while connecting or resetting it.
2. Wait for the RP2350 mass-storage volume to appear.
3. Copy `Pico-SPC-Player.uf2` to that volume.
4. The board reboots when the copy finishes.

## Serial log

UART logging is enabled and USB logging is disabled. UART0 uses GP0 for TX and GP1 for RX
at 115200 baud, 8 data bits, no parity, and 1 stop bit. Connect a 3.3 V USB-to-UART adapter
with a shared ground. UART is initialized after the final peripheral clock configuration.

The release prints startup checks and performance reports every five seconds. See
[UART diagnostics](uart-diagnostics.md) for the fields and comparison guidance. Configure
with `-DPICO_SPC_UART_PERFORMANCE=OFF` to disable the periodic reports; startup checks remain.

## Clean reconfiguration

CMake caches the board, SDK path, and optional embedded-file setting. Remove the build directory
or choose a new one when changing configurations:

```sh
cmake -E remove_directory build
cmake -S . -B build -G Ninja -DPICO_SDK_PATH=/opt/pico-sdk
```

## Host tools and focused checks

The host renderer can convert a local SPC file to unfiltered 32 kHz stereo WAV data:

```sh
cmake -S host -B build-host-tools -G Ninja
cmake --build build-host-tools
build-host-tools/spc_render input.spc output.wav 20
```

The existing deterministic host checks are built separately:

```sh
cmake -S tests -B build-host -G Ninja
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```
