# Coding test — DXY0 "scroll display" (unseen specification)

This is the chapter's unseen-spec coding test: you are given a specification
for an opcode that does not exist on real CHIP-8 hardware and must implement
it inside `scroll_machine.hpp`. Everything except one function is already in
place — the opcode dispatch (`DXY0` -> `op_scroll`), the VF wiring, and the
CLI probe `ch05_99_dxy0_tool`. Your entire job is `scroll_display_up()`.

## Specification: opcode `DXY0`

`DXYN` with low nibble `n == 0` does NOT draw a sprite in this course.
Instead it is the **scroll-display** pseudo-op:

```text
DXY0: let amount = Vy & 0x1F        ; 0..31, taken from register Vy
                                       (the x nibble of the opcode is ignored)

      1. Shift the ENTIRE framebuffer up by `amount` rows:
         new row y    = old row y + amount   (for y + amount < 32)
      2. Vacated rows at the bottom are cleared.
      3. Content pushed past the top edge disappears.
         Wrapping NEVER applies to scroll, even when the wrap quirk is on.
      4. VF = 1 if any LIT pixel was lost off the top edge, else VF = 0.
```

Edge cases you must get right:

| Case | Expected |
|---|---|
| `amount == 0` | framebuffer unchanged, `VF = 0` |
| `amount >= 32` | masked to `amount & 0x1F`, so e.g. 32 behaves like 0 |
| fully-lit top row, `amount = 1` | row disappears, `VF = 1` |
| empty display | unchanged, `VF = 0` regardless of amount |

## How it is graded

The hidden grader runs your built `ch05_99_dxy0_tool` with patterns and
scroll amounts and checks both the printed `vf=` value and FNV64 frame
hashes of the RGBA8888 expansion. Local sanity checks:

```bash
ch05_99_dxy0_tool --pattern row_top --scroll 3     # vf=1, screen becomes empty
ch05_99_dxy0_tool --pattern row_top --scroll 0     # vf=0, screen unchanged
ch05_99_dxy0_tool --pattern checker --scroll 5     # vf=1, pattern shifted up 5
ch05_99_dxy0_tool --pattern bottom_row --scroll 32 # vf=0, screen unchanged
ch05_99_dxy0_tool --pattern row_top --scroll 3 --quirk wrap  # identical result
```

## Files

- `scroll_machine.hpp` — full CHIP-8 machine + your `scroll_display_up()`
  stub (`TODO(8)`). Implement ONLY that function.
- `dxy0_tool.cpp` — provided CLI probe; do not modify for grading.
