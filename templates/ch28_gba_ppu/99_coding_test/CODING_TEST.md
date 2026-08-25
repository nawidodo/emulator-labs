# CODING TEST — ch28: render a supplied PPU-state snapshot exactly

You are given a **PPU-state snapshot** file whose binary format is specified
below — but no documentation of the register semantics beyond what is in
`LECTURE.md`. Your task: implement `load_snapshot()` so the tool renders the
snapshot byte-exactly into an RGBA8888 frame.

## Snapshot format (version 1)

```text
offset  size  field
0       8     magic "GBASNP1"
8       4     io_len    = 0x00000100   little-endian u32
12      4     pal_len   = 0x00000400
16      4     vram_len  = 0x00018000
20      4     oam_len   = 0x00000400
24      ...   IO bytes  (mirror of 0x04000000..FF)
        ...   palette bytes (0x05000000..3FF)
        ...   VRAM bytes (96 KiB from 0x06000000)
        ...   OAM bytes (1 KiB)
```

## Deliverable

`load_snapshot(data, len, mem)` in `snapshot.hpp` validates the magic and
lengths and copies each region into `PpuMemory`. The provided tool
(`ch28_snapshot_tool`) then composes one frame with the chapter pipeline:

```bash
ch28_snapshot_tool scene.snap out.rgba
```

Grading hashes `out.rgba` with FNV-64 against a hidden snapshot that
exercises bitmap modes, affine wrap-around, sprite priority and blending.
Any deviation of one pixel fails the case.

## Hints

- All multi-byte fields are little-endian.
- Validate before copying: wrong magic or truncated file => return false,
  exit code 1.
- The renderer itself is already correct; if your output differs, your
  region offsets are wrong.
