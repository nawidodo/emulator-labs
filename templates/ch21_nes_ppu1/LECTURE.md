# Lecture — NES PPU I: The PPU's Own Address Space

The NES PPU (Picture Processing Unit, the 2C02) is a co-processor with its
**own address bus**. CPU memory and PPU memory are two separate 16-bit
spaces; the only bridges are a handful of registers at `$2000-$2007` and the
OAM DMA channel you will meet in Chapter 24. This is the single most common
architectural mistake in first NES emulators: routing PPU fetches through
CPU RAM "because it's simpler". Don't. The reference implementation keeps
`PpuBus` strictly apart from any CPU memory type.

## Study map

```text
pattern tables -> nametables -> attribute tables -> palettes -> scanlines
```

### Pattern tables ($0000-$1FFF)

8 KB of CHR ROM/RAM from the cartridge, organized as tiles: each tile is
16 bytes = 8 rows of two **bitplanes**. Plane bytes for row *y* live at
`tile*16 + y` and `tile*16 + 8 + y`. Combining bit *x* of both planes gives
the 2-bit tile color:

| plane0 | plane1 | color | meaning            |
|--------|--------|-------|--------------------|
| 0      | 0      | 0     | transparent        |
| 1      | 0      | 1     | palette entry %01  |
| 0      | 1      | 2     | palette entry %10  |
| 1      | 1      | 3     | palette entry %11  |

PPUCTRL bit 4 selects whether background tiles come from the left
(`$0000`) or right (`$1000`) half.

### Nametables ($2000-$2FFF)

Each nametable is 960 bytes of tile indices (32x30) plus 64 bytes of
attribute data at offset `$3C0`: one byte per 4x4-tile block, four 2-bit
palette selects packed per quadrant:

```text
attribute byte bits: [BR][BL][TR][TL]   (2 bits each)
quadrant shift = ((coarse_y & 2) << 1) | (coarse_x & 2)
```

The PPU can address **four** logical nametables but most cartridges carry
only 2 KB of VRAM, so the windows mirror each other. The wiring is chosen by
the cartridge ("mirroring"):

```text
Horizontal mirroring ("vertical arrangement", iNES flags6 bit0=0):  A A / B B
    $2000 == $2400   $2800 == $2C00        selected by address bit 11
Vertical mirroring ("horizontal arrangement", flags6 bit0=1):       A B / A B
    $2000 == $2800   $2400 == $2C00        selected by address bit 10
```

Mnemonic that survives interviews: *horizontal* scrolling games need tall,
independent left/right tables → **vertical** arrangement. A few boards
(4-screen) have 4 KB of VRAM and no mirroring at all.

Addresses `$3000-$3EFF` are a pure mirror of `$2000-$2EFF`. Nothing in the
hardware distinguishes them; games simply never go there.

### Palette RAM ($3F00-$3F1F)

32 bytes: 16 background entries (`$3F00-$3F0F`) and 16 sprite entries
(`$3F10-$3F1F`), grouped in fours. Two hardware rules your emulator must
reproduce:

1. **Universal backdrop.** Tile color 0 never reads the group palettes;
   every `xx0` combination shows `$3F00`. Emulate it either by mirroring
   reads or by special-casing color 0 in the renderer (we do the latter —
   see `bg_pipeline.hpp`).
2. **Sprite-palette entry 0 mirrors the backdrop.** Writes to
   `$3F10/$3F14/$3F18/$3F1C` land on `$3F00/$3F04/$3F08/$3F0C`, because
   sprite color 0 means "transparent", not a color.

Palette addresses mirror through `$3FFF` (`addr & 0x1F`). Values are indexes
into the 2C02's fixed output palette; exact DAC tuning varies between
machines, so we ship one deterministic approximation table.

### Scanlines and the frame

An NTSC frame is 262 scanlines of 341 dots: 240 visible, 1 post-render,
20 vblank, 1 pre-render. Chapter 21 renders static scenes, so we only care
about the *result* of the fetch pipeline; Chapter 22 moves to dot-accurate
behavior. Even so, our runner accounts cycles like real hardware: 89342 dots
per even frame, 89341 on odd frames when rendering is enabled (the skipped
dot on the pre-render line).

### The simplified pipeline (and why it is still correct here)

Real hardware fetches one tile ahead of the pixel output using shift
registers and fine-X selection — the famous "loopy" registers. For a static
scene (no mid-frame writes), the observable result equals a straightforward
per-tile loop: pick tile index + attribute from the nametable image, read
two pattern planes, emit 8 pixels. That is what `02_bg_pipeline`
implements, with the simplification documented in its header. When you reach
Chapter 22 the difference becomes visible (mid-scanline scroll changes) and
you will replace this model with the real thing.

## What you build in this chapter

1. `PpuBus` — the PPU address space with exact mirroring rules.
2. Background pixel path — attributes, planar decode, backdrop rule.
3. Frame renderer + headless runner over crafted `.nesf` state snapshots,
   hashed with FNV-1a 64 for golden comparison.

References: NESdev Wiki "PPU rendering", "Mirroring", "PPU palettes".
