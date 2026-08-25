# Provenance — ch22 public goldens

Reference-solution renders (run twice, identical) of the ch21 public
scenes through the ch22 scrolled renderer:

    ch22_03_sprites_runner --rom <scene>.nesf --headless --frames 2

| Scene | FNV64 |
|---|---|
| scene_grid   | 69F4AFF694879225 |

(The ch22 renderer applies loopy scrolling, sprites and priority; the
grid scene carries no OAM content, so its ch21/ch22 hashes differ only
through scroll handling of fine X = 0.)
