# SPEC — 02_hex_dumper

## Output contract

One line per 16 input bytes, in the style of `hexdump -C`:

```
00000000  23 21 2f 75 73 72 2f 62 69 6e 2f 70 79 74 68 6f  |#!/usr/bin/pytho|
00000010  6e 33                                            |n3|
00000011
```

Exact layout per data row:

1. row offset, 8 lowercase hex digits (rows start at multiples of 16)
2. two spaces
3. sixteen 3-character hex columns: `XX ` per present byte; a missing byte
   is three spaces so every column lines up
4. one space, then the ASCII gutter: `|`, one character per byte position —
   printable ASCII (`0x20..0x7e`) verbatim, anything else `.` — padded with
   spaces to width 16, then `|`
5. `\n`

After the last data row: the total byte count as 8 lowercase hex digits and
`\n`. Empty input produces only that final line (`00000000\n`).

Line endings are plain `\n`; there is no trailing whitespace beyond the
specified spaces.

## CLI

```
ch01_02_hex_dumper [--file PATH | PATH] [--output PATH]
ch01_02_hex_dumper --help
```

Reads the file as raw bytes, writes the dump to stdout or `--output`.
Exit 0 on success; 1 with a message on stderr for usage or IO errors.

## Why this exercise

A hex dumper is the emulator engineer's microscope: cartridge headers,
save states and trace binaries are all debugged through it later. The
implementation also rehearses span slicing and stream formatting without
any allocation beyond the output buffer.

## Acceptance

- `ctest -R ch01_02_hex_dumper` green (unit tests capture the dump into a
  `std::ostringstream` and compare exact strings).
- Running the CLI over `tests/hidden/ch01_lab_infrastructure/fixtures/sample.bin`
  reproduces the hash recorded in that chapter's hidden manifest.
