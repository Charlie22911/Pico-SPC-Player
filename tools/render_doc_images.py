#!/usr/bin/env python3
"""Render reproducible documentation screenshots with the host GUI preview tool."""

from __future__ import annotations

import argparse
import binascii
import struct
import subprocess
import tempfile
import zlib
from pathlib import Path


SCREENS = (
    ("player.png", 0, 0),
    ("player-data.png", 0, 1),
    ("view-menu.png", 1, 0),
    ("aram-activity.png", 2, 0),
    ("aram-data.png", 2, 1),
    ("voices.png", 3, 0),
    ("voice-detail.png", 8, 0),
    ("dsp.png", 4, 0),
    ("dsp-registers.png", 4, 1),
    ("track.png", 5, 0),
    ("library.png", 6, 1),
    ("settings.png", 7, 0),
)


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    checksum = binascii.crc32(kind)
    checksum = binascii.crc32(payload, checksum) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", checksum)


def convert_bmp_to_png(source: Path, destination: Path) -> None:
    data = source.read_bytes()
    if data[:2] != b"BM":
        raise ValueError(f"{source} is not a BMP file")
    pixel_offset = struct.unpack_from("<I", data, 10)[0]
    width, signed_height = struct.unpack_from("<ii", data, 18)
    bits_per_pixel = struct.unpack_from("<H", data, 28)[0]
    compression = struct.unpack_from("<I", data, 30)[0]
    if width <= 0 or signed_height == 0 or bits_per_pixel != 24 or compression != 0:
        raise ValueError(f"unsupported BMP layout in {source}")

    height = abs(signed_height)
    row_bytes = (width * 3 + 3) & ~3
    scanlines = bytearray()
    for output_y in range(height):
        source_y = output_y if signed_height < 0 else height - 1 - output_y
        row_start = pixel_offset + source_y * row_bytes
        scanlines.append(0)
        for x in range(width):
            blue, green, red = data[row_start + x * 3 : row_start + x * 3 + 3]
            scanlines.extend((red, green, blue))

    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    destination.write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + png_chunk(b"IHDR", header)
        + png_chunk(b"IDAT", zlib.compress(bytes(scanlines), 9))
        + png_chunk(b"IEND", b"")
    )


def main() -> int:
    project_root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--preview",
        type=Path,
        default=project_root / "build-host" / "gui_preview",
        help="path to the built gui_preview executable",
    )
    parser.add_argument(
        "--spc",
        type=Path,
        help="optional local SPC used for metadata and the ARAM Data capture",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=project_root / "docs" / "images",
        help="destination directory",
    )
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="pico-spc-doc-images-") as temporary:
        temporary_path = Path(temporary)
        for filename, screen, variant in SCREENS:
            bitmap = temporary_path / f"{Path(filename).stem}.bmp"
            command = [str(args.preview), str(bitmap), str(screen), str(variant)]
            if args.spc is not None:
                command.append(str(args.spc))
            subprocess.run(command, check=True)
            convert_bmp_to_png(bitmap, args.output / filename)

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
