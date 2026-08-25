# Lecture — the SM83 bus and memory map

## Why device-object routing

The naive emulator writes `Bus::read` as a giant switch (or worse, one
flat 64 KiB array). Three problems compound:

1. **Side effects have nowhere to live.** A write to FF50 must be
   observable by boot-remap logic; a flat array just stores a byte.
2. **The map exists twice.** Read and write switches drift apart —
   the classic source of "reads work, saves vanish" bugs.
3. **Overlays fight the structure.** A boot ROM shadowing 0000-00FF is
   an *exception* in a switch but *the normal case* in an ordered table.

The design used in this chapter is an ordered range table:

```cpp
struct RangeEntry { uint16_t lo, hi; Device* device; };
```

`Bus::read`/`Bus::write` scan it once and delegate to the first entry
whose inclusive range `[lo, hi]` contains the address. Priority is
*table order*: front entries win. That single rule gives us overlays,
echo windows, and per-device side effects with no special cases.

Ranges are **inclusive on both ends**. Half-open ranges are the classic
bus off-by-one: FE9F must hit OAM and FEA0 must not.

## The authoritative Game Boy map

| Range         | Region        | Device in this chapter |
|---------------|---------------|------------------------|
| 0000-7FFF     | cartridge ROM | `CartRom` (read-only image) |
| 8000-9FFF     | VRAM          | `Ram` |
| A000-BFFF     | external RAM  | `Ram` |
| C000-DFFF     | work RAM      | `Ram` |
| E000-FDFF     | echo RAM      | `EchoWindow` onto WRAM, translation `- $2000` |
| FE00-FE9F     | OAM           | `Ram` |
| FEA0-FEFF     | unusable      | *unattached* — documented gap policy |
| FF00-FF7F     | I/O registers | stub register array |
| FF80-FFFE     | HRAM          | `Ram` |
| FFFF          | IE register   | `IeLatch` |

### Echo RAM

The address decoder duplicates chip-selects to save a pin: E000-FDFF
maps the **lower** 0x1E00 bytes of the WRAM bank. Translation is exact:
`addr - 0x2000`, so E000 -> C000 and FDFF -> DDFF are the endpoints.
DFFF has no echo counterpart, and nothing at or above FE00 aliases.
Model it as a window device that translates before delegating — never
as a second buffer kept "in sync".

### The unusable page and open-bus policy

FEA0-FEFF decodes to nothing. This chapter documents one policy and
enforces it everywhere: **gaps read `$00` and drop writes**. Real
hardware floats (and on DMG, incrementing OAM into this region corrupts
sprites), but a deterministic documented policy beats folklore. Note
that other consoles choose differently — CourseBoy-II in the challenge
ties its strobes high (`$FF`), Tetra-8 idles low.

### Boot ROM mapping

On reset a 256-byte internal ROM sits at 0000-00FF, in front of the
cartridge. In our model that is literally table order: the boot entry
is attached in front of the cart entry, and the first-match scan hits
it. **Any** write to FF50 unmaps — the value is ignored; the address is
the message.

Hardware nuance worth knowing: a real DMG *also* unmaps when the
program counter crosses `$0050` during execution, because the boot
ROM's own jump-out path hands control to the game. Our bus-level model
has no CPU to observe, so it keys on the FF50 write only. If you later
attach a CPU, decide explicitly which trigger(s) you honor and write
the choice down.
