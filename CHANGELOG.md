# Changelog

## 0.1.2 - 2026-09-30

- Converted Activity bitmaps eight addresses at a time while preserving colors and fading.
- Optimized the full-screen 422x310 scaler with packed pixel pairs and copies of repeated rows,
  preserving the existing nearest-neighbor output and clipping behavior.
- Consumed ARAM publications independently of the general UI refresh timer, with request and
  playback-generation checks for both Activity and Data snapshots.
- Added five-second UART performance reports covering audio, display, map scheduling, storage,
  and copied DSP echo state, including interval deltas and rendering maxima.
- Initialized UART after configuring the peripheral clock and added measured clock, flash/PSRAM
  timing, boot memory, and stack diagnostics.
- Kept the release at 250 MHz, with flash and display at 125 MHz and linear PSRAM at 83.33 MHz.
  Increased the display/UI core's stack to 4 KiB for the diagnostics.
- Added rendering-equivalence, consumer-transition, UART, and board-clock regression checks.
- I measured approximately 60 Hz in stable home Activity and full-screen Data windows using
  SNEStronizer, with zero silence frames or underruns. See
  [the optimization results](docs/aram-optimization-build.md) for the comparison and its limits.
- Retained the SRAM emulator placement and unchanged playback calculations from 0.1.1.
- Provided the public UF2, checksums, and matching relinking materials.

## 0.1.1 - 2026-09-30

- Made Arm Cortex-M33 (`rp2350-arm-s`) the explicit default and supported release target.
- Disabled unused ARAM Activity recording while Data mode is selected, with fresh tracking when
  returning to Activity, including rapid mode changes and track changes.
- Gave routine home-map updates a single palette lookup path by transferring the map interior
  separately from its border; retained correct mixed-palette transfers and touch feedback.
- Moved the DSP loop and SPC interpreter into internal SRAM by default. Flash placement remains
  available with `PICO_SPC_EMULATOR_IN_SRAM=OFF`.
- Added regression checks for Activity recording transitions, palette conversion, and map borders.
- Recorded an Arm stress run with zero silence frames and underruns and a minimum audio queue of
  1,536 frames (48 ms). See [the optimization results](docs/aram-optimization-build.md).
- Added the release version to firmware metadata. Playback calculations and ARAM-map algorithms
  remain unchanged.
- Provided checksums and matching relinking materials alongside the public UF2.

## 0.1.0 - 2026-09-10

- Added software-emulated SPC700 and S-DSP playback at 32 kHz stereo.
- Added read-only FAT SD library browsing with ID666 titles and marquee scrolling.
- Added the 450x600 finger-operated touch interface and playback controls.
- Added audio, display, buffer, and visualization diagnostics.
- Added voice summaries/details and decoded/raw DSP views.
- Added full-address ARAM Activity visualization.
- Added the ARAM Data heatmap with a zero-distinct 16-color palette.
