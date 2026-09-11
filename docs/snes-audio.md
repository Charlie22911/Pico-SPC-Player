# SNES audio and SPC files

## The SNES audio subsystem

The Super Nintendo Entertainment System has a sound subsystem that operates largely
independently of its main CPU. A game uploads a sound driver, music sequence data, and compressed
samples into the subsystem's 64 KiB of audio RAM, commonly called ARAM. Two main components then
cooperate to produce the soundtrack:

- The **S-SMP** contains the 8-bit SPC700 processor that runs the game's sound driver. It
  interprets sequence data, schedules notes, and updates the DSP registers.
- The **S-DSP** is the digital signal processor. It mixes eight sampled voices into a 32 kHz
  stereo output and provides pitch control, ADSR or GAIN envelopes, BRR sample decoding, noise,
  pitch modulation, and an eight-tap FIR echo effect.

The eight voices are shared by music and sound effects. Each voice selects BRR-compressed sample
data from ARAM and has its own pitch, envelope, and left/right volume settings. The sound driver
changes these settings over time to perform the music.

```text
Game CPU
   |
   | uploads driver, sequences, and samples
   v
64 KiB ARAM <--> SPC700 sound driver
                       |
                       | updates registers
                       v
                 S-DSP: 8 voices, BRR, envelopes, echo
                       |
                       v
                 32 kHz stereo output
```

## What an SPC file contains

An SPC file is a saved state of the SNES sound subsystem. It is not a conventional audio
recording such as a WAV or MP3. It contains the state needed to resume the captured music in an
emulator, including:

- the complete 64 KiB ARAM image;
- SPC700 CPU registers and execution state;
- S-DSP registers and supporting state; and
- optional ID666 metadata such as track title, game, artist, and dumper.

Because playback resumes from machine state, an SPC emulator recreates the audio rather than
decoding a prerecorded waveform. Pico-SPC-Player runs a software SPC700 and S-DSP emulator on the
RP2350 and sends the resulting 32 kHz stereo samples to an external I2S DAC.

Some files have incomplete or absent ID666 tags. In that case the player may need to display a
filename where tagged title or game information is unavailable.

## Why the internal state can be visualized

Software emulation gives the player access to the same state used to generate the sound. The UI
can therefore show ARAM accesses, byte values, DSP registers, pitch settings, voice envelopes,
BRR addresses, echo configuration, and other live information without changing playback.

Visualization remains lower priority than audio generation. Snapshots are copied through bounded,
nonblocking queues, and an update may be dropped when the UI is busy. This preserves the audio
deadline while keeping the displayed state closely synchronized with the music.
