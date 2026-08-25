# SPEC — snapshot text format and rendering contract (binding)

## Grammar

Line-based UTF-8 text. `#` starts a comment (to end of line). Blank lines
are ignored. Every non-empty line is `KEY args...`. Unknown keys, wrong
argument counts and out-of-range values are errors: parsing fails.

```
SIZE <w> <h>                tilemap size in tiles; 1 <= w,h <= 32
MAP <y> <t...>              row y: exactly w hex tile numbers, each 0..255
PAL <i> <hhhh>              CGRAM entry i, 0..255 = 4-hex BGR555 value
TILE <n> <hhhh x8>          tile n, 0..255 = eight 4-hex-digit row words
SCROLL <x> <y>              pixel scroll offsets (decimal)
WINDOW <l> <r> <en> <inv>   l/r decimal 0..255 inclusive; en/inv are 0|1
MATH <add|sub> <half> <on>  half/on are 0|1, e.g. "MATH add 1 1"
BACKDROP <i>                CGRAM index behind everything; default 0
```

Tile rows are 2bpp packed MSB-first: pixel `k` (`k = 0` is the LEFTMOST
texel) of a row word `w` has value `(w >> (14 - 2*k)) & 3`.

## API

```cpp
namespace snesbus {
struct Snapshot { /* fields exactly as in coding.hpp */ };
Snapshot parse_snapshot(const std::string& text);   // provided
void decode_tile_row(uint16_t w, std::array<uint8_t,8>* out);   // implement
bool parse_map_row(Snapshot&, unsigned y,
                   const std::vector<std::string>& tokens);      // implement
uint16_t snapshot_sample(const Snapshot&, int x, int y);        // implement
void snapshot_render(const Snapshot&, std::span<uint8_t> out);  // implement
}
```

## Rendering model

1. One layer. The map covers the screen from (0,0), scaled 8x8 per tile.
2. Scroll wraps WITHIN the map area: sample position
   `((x + scroll_x) mod (w*8), (y + scroll_y) mod (h*8))`.
3. Screen positions outside the map area show the backdrop.
4. The sampled TEXEL VALUE selects a CGRAM entry directly. Value 0 is
   transparent -> backdrop entry shows instead.
5. Window: when enabled (`en=1`) the layer shows ONLY where
   `win_left <= x <= win_r` (inclusive); with `inv=1` that region is
   inverted. Windowed-out pixels show the backdrop.
6. Color math (`on=1`): the surviving color combines with the BACKDROP
   entry per 5-bit channel — add or subtract, then optional halving of
   the intermediate, then saturating clamp to 0..31. Math never applies
   to pixels already showing the backdrop entry itself.
7. Output: 256x224 RGBA8888, byte order R,G,B,A per pixel, alpha $FF;
   channels expand via `(v << 3) | (v >> 2)`.

## Example (used verbatim by the public suite)

```
# SPEC example
SIZE 4 2
PAL 0 3800
PAL 1 03E0
PAL 2 7C00
PAL 3 7FFF
TILE 0 0000 0000 0000 0000 0000 0000 0000 0000
TILE 1 5555 5555 5555 5555 5555 5555 5555 5555
TILE 2 AAAA AAAA AAAA AAAA AAAA AAAA AAAA AAAA
TILE 3 1B1B 1B1B 1B1B 1B1B 1B1B 1B1B 1B1B 1B1B
MAP 0 1 0 2 3
MAP 1 3 3 1 0
SCROLL 0 0
WINDOW 16 47 1 0
MATH add 1 1
BACKDROP 0
```

Worked probes for this example:

* Pixel (20,4): inside window; map texel = tile 2 (solid value 2) ->
  entry `$7C00` (blue). Math add-half vs backdrop `$3800` (blue channel
  14): blue = (31+14)/2 = 22 -> BGR555 `22 << 10`.
* Pixel (100,4): outside window -> pure backdrop `$3800`.
