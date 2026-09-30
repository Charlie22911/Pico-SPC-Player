# Changelog

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
