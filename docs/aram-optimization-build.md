# Arm ARAM optimization results

Release 0.1.2 targets the Waveshare RP2350 Touch AMOLED 2.41's Arm Cortex-M33
cores at 250 MHz. The public build uses Pico SDK 2.2.0, Arm GCC 13.2.1,
Release `-O3`, no LTO, and no embedded SPC music.

## Rendering and scheduling changes in 0.1.2

Activity conversion reads the read/write/execute bitmaps by byte, processing eight
addresses together with small pair-color and fade lookup tables. This reduces bitmap
loads from 196,608 to 24,576 per complete map while preserving the output pixels and
one-publication fade history.

The 422x310 scaler uses source/destination row pointers and packed pixel-pair mappings.
It scales 256 rows and copies the 54 additional output rows whose source row repeats.
Nearest-neighbor sampling, palette indexes, clipping, and the generic scaler remain
equivalent to the previous implementation.

Core 1 consumes ARAM publications independently of the general UI model timer and queues
a map redraw immediately. Activity publications now carry both request and playback-generation
tags, matching Data's stale-result checks. Consumer-held banks retain their ownership.

The map is still scaled into the indexed framebuffer before palette conversion into the
display DMA buffers. Direct generation of RGB565 strips into those buffers has not been
implemented. Playback calculations and echo processing are unchanged, and PSRAM remains
outside the runtime audio and map paths.

## Clock configuration

I kept this release at 250 MHz after observing UI, touch, and UART halts in the 276 MHz
trial while audio continued. The exact cause of those halts remains unconfirmed. I observed
stable operation, including a completed mode transition, in the recorded 250 MHz session.

| Clock | Release setting |
| --- | ---: |
| System and peripheral | 250 MHz |
| Flash | 125 MHz, divider 2, RXDELAY 2 |
| Display | 125 MHz, PIO divider 1, two instruction cycles per serial clock |
| PSRAM | 83.33 MHz, divider 3, RXDELAY 4, linear mode |

Startup checks measure system/peripheral clocks against the 12 MHz reference and report
the QMI timing registers. [UART diagnostics](uart-diagnostics.md) describe the checks and
five-second performance reports.

## Hardware comparison for 0.1.2

I compared the earlier 250 MHz diagnostic baseline and the optimized 250 MHz test build
using [SNEStronizer](https://github.com/ResistanceVault/demo-twistit/blob/master/data/SNEStronizer.spc)
with the map requested at 60 Hz. Flash, display, and PSRAM clocks were the same in both
builds. I used only complete, stable reporting windows from the explicitly identified
250 MHz session for the new results.

| View | Baseline map transfers | Optimized map transfers | Baseline GUI window maximum* | Optimized GUI window maximum* |
| --- | ---: | ---: | ---: | ---: |
| Home Activity | 43.48 Hz | 60.05 Hz | 10.568 ms | 2.892 ms |
| Full-screen Data | 56.30 Hz | 59.99 Hz | 8.130 ms | 5.193 ms |

\* GUI values are medians of five-second window maxima, not average frame times. Map rates
are total completed map transfers divided by the total reporting duration for that view.

The optimized home Activity result covers four windows totaling 20.001 seconds; full-screen
Data covers seven windows totaling 35.003 seconds. Baseline coverage was 140.017 and
40 seconds respectively. Track positions and observation durations differ, so this is an
observed comparison rather than a paired benchmark. The new identified session has no stable
home Data or full-screen Activity window for a matching comparison.

Across the approximately 80.9 seconds of playback in the new session, I recorded zero silence
frames, underruns, late DMA blocks, DSP/voice snapshot drops, display errors, and log drops.
The minimum audio queue remained 1,536 frames (48 ms). Maximum audio render time was
5,158 us, compared with 5,443 us in the diagnostic baseline. Audio status drops remained
at the startup total of 20; one busy map publication was skipped during a view transition.

I collected these board results before adding the 0.1.2 version metadata. The release
source check confirms the same firmware sources and settings apart from that version change.

## Earlier changes retained from 0.1.1

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

These three changes left playback and ARAM rendering algorithms unchanged in 0.1.1.
Release 0.1.2 adds the rendering and scheduling optimizations described above.

## Original 0.1.1 hardware comparison

I compared the Arm baseline and optimized firmware using
[SNEStronizer](https://github.com/ResistanceVault/demo-twistit/blob/master/data/SNEStronizer.spc)
with the home ARAM Data view set to 60 Hz. My baseline run lasted approximately
45 seconds. The table shows the diagnostics before and after the three optimizations.

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

I tested all three optimizations together, so these results show their combined
effect. They do not establish the contribution of each individual change.
Render and GUI diagnostics are recorded maxima. In those images, MAP counted completed full-screen
ARAM transfers and retained its last value on the home screen, so it did not
measure home-view refresh rates. Release 0.1.2's UART `map_tx` counts completed home and
full-screen map transfers.

I collected these measurements with the optimized Arm firmware before the
release version metadata was added. Release 0.1.1 uses the same playback and
visualization code, with an explicit Arm default and firmware version metadata.

## Verification

The clean Arm v0.1.2 build, all 21 host checks, and both board host checks passed, including real SPC
CPU/echo writes across mode and track changes, consumer-held bank ownership,
rapid mode requests, rendering equivalence, palette boundaries/output, map-border rendering,
clock validation, and UART formatting. Audio, renderer, consumer, pipeline, and UART checks
ran with AddressSanitizer and UndefinedBehaviorSanitizer.

A host render of 60 seconds of the pinned SNEStronizer track produced
1,920,000 stereo frames bit-identical to the saved baseline library. Recording
was toggled every five seconds in the new render and remained enabled in the
baseline. PCM SHA-256:
`3b5f8aff9864dc59a171dd35963016fcc22fe3bb78b301f0468c057a37ceb464`.
This checks audio equivalence; it does not measure board performance.

Complete GCC firmware disassembly confirms the reduced bitmap loads, removal of repeated
canvas-base/stride loads from the scaler's inner loop, repeated-row copies, and a uniform
palette lookup loop without per-pair region tests. The firmware's ELF symbols place both selected emulator
functions in SRAM. After removing debug information, the compiled emulator
library is byte-identical to the library in the tested optimization image.

## SRAM budget

The verified Arm v0.1.2 image uses 415,624 bytes of the 524,288-byte main
SRAM region, including SRAM code. Its heap region is 108,664 bytes, leaving
40,168 bytes after the 68,496-byte emulator backend and before other heap use.
Core 0 has a 2 KiB stack and Core 1 has a 4 KiB stack in the separate scratch banks.
This is a linker-based budget; library allocations and allocator overhead
reduce the remaining heap.

## Building and flashing

Follow [Building and flashing](building.md) for a clean Arm Release build.
Download `Pico-SPC-Player.uf2` from the
[v0.1.2 release](https://github.com/Charlie22911/Pico-SPC-Player/releases/tag/v0.1.2)
and copy it to the board's BOOTSEL volume. The release includes firmware
checksums. No SPC music is embedded or distributed.
