# Documentation images

The interface screenshots are rendered by the same indexed-color drawing code used by the
firmware. Build the host checks, then regenerate the images from the built preview executable:

```sh
python3 tools/render_doc_images.py
```

Pass `--spc /path/to/track.spc` to use that file's ID666 metadata and 64 KiB ARAM image. The SPC
is read locally and is never copied into this directory. Without that option, the renderer uses a
small deterministic fixture.

`wiring.svg` is maintained directly because its labels describe physical connections rather than
runtime output.
