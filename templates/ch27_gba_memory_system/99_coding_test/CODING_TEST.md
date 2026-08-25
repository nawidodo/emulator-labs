# Coding Test — ch27: Unseen Region Timing

A new cartridge chip appears on an extended bus. Its datasheet lines:

```text
FLASHCART window : 0x0F000000 - 0x0FFFFFFF (mirrors every 64 KB)
bus width        : 16 bits
non-sequential   : 8 cycles
sequential       : 3 cycles
first access after any pipeline refill: always non-sequential
```

## Task

Implement the FLASHCART support in `coding_bus.hpp`:

1. `flash_present(addr)` — true iff the address lives in the FLASHCART
   window (top byte 0x0F).
2. `flash_offset(addr)` — canonical offset inside one 64 K mirror page.
3. `burst_total(count)` — exact cycle total of `count` adjacent halfword
   accesses from offset 0, starting right after a pipeline refill.

The hidden grader runs the chapter report tool and hashes its output plus
unseen burst totals against this reference implementation.
