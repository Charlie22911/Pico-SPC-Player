# UART diagnostics

Connect UART0 TX (GP0) to a 3.3 V adapter's RX and connect a shared ground. Use
115200 baud, 8 data bits, no parity, 1 stop bit, and no flow control. GP1 is UART0 RX.
USB serial logging is disabled.

## Startup checks

The startup report includes firmware version, requested and actual UART baud rates,
requested and measured clocks, flash/PSRAM dividers and sampling delays, display clock,
PSRAM capacity and memory check, heap budget, stack sizes, and watchdog reboot status.

The 250 MHz release uses 125 MHz flash and display clocks and approximately 83.33 MHz
linear PSRAM. System and peripheral measurements use the 12 MHz reference clock. A
`CLOCK CHECK: PASS` confirms those measurements within 0.1% and the exact flash divider.
These checks do not establish the stability of a different overclock configuration.

## Performance reports

`PERF` reports are produced every five seconds. Transmission drains a small number of
bytes per UI loop without waiting for UART space. `log_drop` counts reports skipped
because the preceding report was still queued or formatting did not fit.

| Field | Meaning |
| --- | --- |
| `t`, `dt`, `gen` | Uptime, reporting interval, and playback generation. |
| `screen`, `aram`, `requested` | Current page, map mode, and selected map rate. |
| `window_view` | `stable` if the page/mode/rate stayed unchanged; `mixed` if it changed during the interval. |
| `rate` | Completed audio frames per second during the interval; output targets 32,000. |
| `silence`, `underrun`, `late_dma` | Audio fault totals and interval increments in parentheses. |
| `a_drop`, `v_drop` | Audio status and DSP/voice snapshot queue drops; these are separate from audio underruns. |
| `render_max`, `buf_min` | Worst render time and minimum queued audio frames since audio started. A 256-frame block has an 8,000 us budget. |
| `gui_max`, `gui_window_max` | Lifetime and interval maximum UI-loop work times. These are maxima, not average frame times. |
| `transfers`, `maps`, `map_tx` | Completed display transfers, completed map transfers, and map transfer rate over the interval. Map counts cover both home and full-screen views. |
| `last`, `max`, `errors` | Latest and worst display-transfer durations and cumulative display errors. |
| `due`, `pub`, `busy`, `headroom` | Scheduled map opportunities, successful publications, occupied-buffer skips, and insufficient-audio-headroom skips. |
| `consume`, `discard` | Accepted map publications and rejected stale or irrelevant snapshots. |
| `activity_window_max`, `scale_window_max` | Interval maximum Activity conversion and map scaling times. |
| `STORAGE` | Card state, current file, storage error, and log-formatting maximum. |
| `ECHO` | Copied DSP echo state for the current playback generation, including enable mask, address, size, feedback, and output volumes. |

Compare stable intervals using the same track positions and settings. A mixed window's
rate includes more than one view. Counters can include startup events, so use the
parenthesized increments to identify new events. `map_tx` measures completed transfers;
it does not prove that every publication reached the panel or measure optical refresh.
