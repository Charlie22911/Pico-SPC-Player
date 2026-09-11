# Troubleshooting

## No sound

1. Confirm BCK=GP26, LRCK=GP27, DIN=GP28, and a shared ground.
2. Confirm the powered speaker or amplifier works from another source.
3. On the common GY-PCM5102A module, verify H1=L, H2=L, H3=H, H4=L and ground SCK.
4. Check H3/XSMT closely. An open or floating XSMT can keep the DAC muted.
5. Open Settings. **Audio Failed** means I2S DMA did not initialize. Increasing **Underrun Blocks**
   or **DMA** values indicate that audio service is missing deadlines.

## Card or library is unavailable

- **Insert a FAT32 card**: no card responded during startup.
- **Card mount failed**: a card responded but FatFs could not mount it. Reformat as FAT32 with
  512-byte sectors and check the SPI wiring.
- **No game folders**: create at least one visible root folder.
- **No SPC files**: place `.spc` files directly inside the selected root folder.
- **Folder/track limit reached**: reduce the number of entries. Current limits are defined in
  `src/storage/spc_catalog.h`.
- **Card unavailable/read/open/close error**: reseat the card and check GP18, GP19, GP20, GP23,
  power, and ground.

Nested folders, root-level SPC files, hidden/system entries, exFAT volumes, and overlong paths are
not supported.

## A track will not load

**Invalid SPC file** means the file is too small or failed the SPC signature/metadata checks.
**SPC load failed** means the emulator rejected the staged image. Try the same file with the host
renderer and verify that it is a standard SPC snapshot rather than a compressed archive.

## Audio skips or pops during interaction

Open Settings and watch the counters while reproducing the issue:

- Rising **Underrun Blocks** means the DMA interrupt consumed audio faster than Core 0 supplied it.
- Rising **DMA** means interrupt service itself was late.
- **Buf Min** near zero confirms low audio headroom.
- **Render** shows the longest 256-frame emulator block time.
- **GUI**, **A-Drop**, and **V-Drop** may rise under display load without harming audio; visual work
  is deliberately disposable.

Use 30 Hz visualization when diagnosing marginal hardware or timing. A Data heatmap capture is
deferred until two PCM blocks are already queued.

## Visual data is unavailable

ARAM, voice, and DSP views require a loaded track and a current-generation snapshot. Immediately
after changing tracks or toggling ARAM mode, the old view is cleared until a correctly tagged copy
arrives. This is expected and prevents stale emulator state from being shown.

## Compiler messages from `snes_spc`

GNU C++ may diagnose the upstream emulator's deliberate access to contiguous RAM padding. The
firmware build scopes `-Wno-stringop-overflow` to the vendored `snes_spc` target. Project-owned
sources retain normal warning checks.
