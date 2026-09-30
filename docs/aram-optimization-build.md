# Arm Data-view optimization results

Release 0.1.1 targets the Waveshare RP2350 Touch AMOLED 2.41's Arm Cortex-M33
cores at 250 MHz. The public build uses Pico SDK 2.2.0, Arm GCC 13.2.1,
Release `-O3`, no LTO, and no embedded SPC music.

## Changes

1. Core 0 disables all three Activity recording hooks while the requested ARAM
   mode is Data. Switching back clears the producer maps and discards unread
   Activity publications before restoring the hooks. A bank held by Core 1 is
   never cleared. Track changes in Data keep recording disabled, and rapid
   Data/Activity requests also restart tracking even if Core 0 only observes
   the final request.
2. Routine home-map redraws transfer the 256-by-256 interior. Border presses and
   cancellations explicitly redraw the surrounding two-pixel outline. Display
   transfers are classified once as primary, secondary, or mixed. Uniform
   rectangles use one palette LUT without region checks inside the pixel-pair
   loop; mixed rectangles preserve the existing palette selection.
3. `SPC_DSP::run` and `SNES_SPC::run_until_` execute from internal SRAM. The SDK
   copies their `.time_critical.spc` section from flash at boot. Configure with
   `-DPICO_SPC_EMULATOR_IN_SRAM=OFF` to restore flash placement. Host builds use
   normal placement.

Playback calculations, echo processing, sample rate, audio block size,
ARAM value conversion, Activity-map rendering, and scaling algorithms are
unchanged. PSRAM placement is unchanged.

## Reported hardware result

The user compared the Arm baseline and optimization images using SNEStronizer
with the home ARAM Data view set to 60 Hz. The baseline run lasted approximately
45 seconds; the follow-up was reported as the same comparison after the changes.

| Diagnostic | Before | After |
| --- | ---: | ---: |
| Silence frames | 2,304 | 0 |
| Underrun blocks | 9 | 0 |
| Minimum queued audio frames | 0 | 1,536 (48 ms) |
| Maximum audio render time | 9,569 us | 5,435 us |
| Maximum GUI slice time | 23,944 us | 18,870 us |
| Late DMA blocks | 0 | 0 |
| Audio status queue drops | 20 | 20 |
| DSP/voice snapshot drops | 1,198 | 0 |

These are observations from one comparison, not an isolated measurement of
any individual optimization. Render and GUI diagnostics are recorded maxima.
MAP is excluded here because it counts completed full-screen ARAM transfers
and retains its last value on the home screen.

The measurements used the Arm optimization image before release version
metadata was added. Release 0.1.1 rebuilds the same playback and visualization
code with an explicit Arm default and firmware version metadata.

## Verification

The clean Arm v0.1.1 build and all 18 host checks passed, including real SPC
CPU/echo writes across mode and track changes, consumer-held bank ownership,
rapid mode requests, palette boundaries/output, and map-border rendering.
The existing audio tests ran with AddressSanitizer and UndefinedBehaviorSanitizer.

A host render of 60 seconds of the pinned SNEStronizer track produced
1,920,000 stereo frames bit-identical to the saved baseline library. Recording
was toggled every five seconds in the new render and remained enabled in the
baseline. PCM SHA-256:
`3b5f8aff9864dc59a171dd35963016fcc22fe3bb78b301f0468c057a37ceb464`.
This checks audio equivalence; it does not measure board performance.

Linked GCC disassembly confirms a uniform palette lookup loop without
per-pair region tests. The firmware's ELF symbols place both selected emulator
functions in SRAM. After removing debug information, the compiled emulator
library is byte-identical to the library in the tested optimization image.

## SRAM budget

The verified Arm v0.1.1 image uses 379,844 bytes of the 524,288-byte main
SRAM region, including SRAM code. Its heap region is 144,444 bytes, leaving
75,948 bytes after the 68,496-byte emulator backend and before other heap use.
Each core's configured 2 KiB stack is in a separate 4 KiB scratch bank.
This is a linker-based budget; library allocations and allocator overhead
reduce the remaining heap.

## Building and flashing

Follow [Building and flashing](building.md) for a clean Arm Release build.
Download `Pico-SPC-Player.uf2` from the
[v0.1.1 release](https://github.com/Charlie22911/Pico-SPC-Player/releases/tag/v0.1.1)
and copy it to the board's BOOTSEL volume. The release includes firmware
checksums. No SPC music is embedded or distributed.
