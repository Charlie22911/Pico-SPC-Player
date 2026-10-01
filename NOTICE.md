# Third-party notices

Pico-SPC-Player project-owned source is available under the MIT License in `LICENSE`.

## snes_spc

The firmware includes Shay Green's `snes_spc` emulator from commit
`ec8ee2bbe30451614c1d02a83f7af1c97d497d45`, licensed under LGPL-2.1-or-later. Original notices
are retained in `third_party/snes_spc/`; a copy of the license is in
`LICENSES/LGPL-2.1.txt`.

Local changes add copy-only DSP/voice snapshots and compile-time optional ARAM activity and Data
heatmap hooks, plus optional SRAM placement of the DSP loop and SPC interpreter in Pico firmware.
The modifications do not expose live emulator storage. Binary redistributors must
retain the applicable notices and satisfy the LGPL source and relinking requirements.

## FatFs

The firmware includes ChaN's FatFs R0.15 patch 1, configured for one read-only FAT12/16/32 volume
with 512-byte sectors, code page 437, bounded long filenames, and no exFAT. Its permissive notice
is retained in each imported source and in `LICENSES/FatFs.txt`.

## Waveshare board drivers

The AMOLED, touch, board, and QSPI sources were imported from the RP2350 Touch AMOLED integration
in `Charlie22911/RP2350-Remote-Display` at commit
`813e35c75544e116bd1541e1dd09e0d9cba484f6`. Those files retain Waveshare's MIT-style permission
notice, also copied in `LICENSES/Waveshare-MIT.txt`. Local integration adapts the drivers to
asynchronous palette expansion and the project's Core 1 display ownership, and initializes
UART after the final peripheral clock configuration.

## Raspberry Pi I2S PIO

`src/audio/i2s_tx.pio` is derived from Raspberry Pi's `pico-extras` I2S PIO program at commit
`52fd7a786ce47c989afeaee67e0c6aebee56ed2d`. It is licensed under BSD-3-Clause; the license is in
`LICENSES/BSD-3-Clause-Raspberry-Pi.txt`. The entry point is positioned at the left-channel
boundary for deterministic startup and uses 32-bit channel slots.

Exact imported file paths and SHA-256 hashes are recorded in `third_party/manifest.json`.
