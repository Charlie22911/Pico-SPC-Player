# Contributing

Pico-SPC-Player welcomes focused changes that preserve real-time audio behavior and keep the
hardware assumptions explicit. Open an issue or describe the device evidence when changing a
timing, pin, electrical, or display-controller claim.
Development and release validation target the RP2350 Arm Cortex-M33 cores (`rp2350-arm-s`).

## Finding the right code

| Change | Start here |
| --- | --- |
| UI geometry and touch regions | `src/ui/ui_layout.*` and the relevant page module |
| Audio format and block size | `src/audio/pcm_format.h` |
| I2S or SD GPIO assignments | The owning driver in `src/audio/` or `src/storage/` |
| Storage capacities | `src/storage/spc_catalog.h` |
| Cross-core messages | The owning queue/mailbox header and `src/app/` runtime |
| Emulator integration | `src/spc/`; modifications inside `third_party/` require provenance updates |

The full module map is in [docs/architecture.md](docs/architecture.md).

## Code style

- Format project-owned C/C++ with the repository `.clang-format` file.
- Use four spaces and keep implementation helpers `static` in their `.c` file.
- Leave files under `third_party/` in their upstream format.
- Name time, frequency, frame, and capacity constants with units such as `_US`, `_MS`, `_HZ`,
  `_FRAMES`, or `_BYTES`.
- Comments should explain ownership, timing, hardware constraints, packed formats, or a non-obvious
  decision. Avoid comments that restate the code.
- Public APIs must document core/IRQ ownership, blocking behavior, borrowed-buffer lifetime, and
  units when these are not clear from the type and name.

## Validation

Use the smallest validation that proves the change. Extend host checks for deterministic logic,
parsing, queue, formatting, or rendering behavior. Do not create tests that merely duplicate an
implementation or require arbitrary delays. Hardware behavior must be checked on the target board.

For the existing host suite:

```sh
cmake -S tests -B build-host -G Ninja
cmake --build build-host
ctest --test-dir build-host --output-on-failure
```

For a clean public firmware build, follow [docs/building.md](docs/building.md). Do not commit build
products, SPC files, WAV files, or private device logs.

## Third-party changes

Keep imported changes narrow and mark project extensions beside the modified code. Any addition or
change under `third_party/` must update `third_party/manifest.json`, relevant license files, and
`NOTICE.md`. Preserve upstream copyright and license headers.

## Review checklist

- Core or IRQ ownership is explicit for shared mutable state.
- No real-time or interrupt path gains an unbounded wait.
- Borrowed buffer lifetime is documented and respected.
- Numeric units are present in names or API comments.
- Implementation-only helpers remain private.
- UI diagnostic terms agree with the troubleshooting guide.
- Vendored files were not silently reformatted.
- No music, build output, personal path, or development transcript is included.
