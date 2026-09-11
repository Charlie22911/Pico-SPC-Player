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

The default configuration embeds no music:

```sh
cmake -S . -B build -G Ninja -DPICO_SDK_PATH=/opt/pico-sdk
cmake --build build --target pico_spc_player
```

The resulting firmware is `build/Pico-SPC-Player.uf2`.

From WSL, both the repository and SDK may use Linux paths:

```sh
cd /mnt/c/path/to/Pico-SPC-Player
cmake -S . -B build -G Ninja -DPICO_SDK_PATH=$HOME/pico-sdk
cmake --build build --target pico_spc_player
```

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

UART logging is enabled and USB logging is disabled. UART0 uses GP0 for TX and GP1 for RX at the
Pico SDK default baud rate of 115200. Connect a 3.3 V USB-to-UART adapter with a shared ground.

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
