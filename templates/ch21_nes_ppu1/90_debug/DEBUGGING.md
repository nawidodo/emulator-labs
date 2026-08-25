# Debugging — ch21: the case of the wrong-colored quadrants

## Symptom

Your background renders tiles with the right *patterns* but the wrong
*colors* in a very specific shape:

- Every 2x2-tile quadrant block shows its colors **swapped left/right**:
  the top-left quadrant of each attribute block paints with the palette the
  top-right quadrant should use, bottom-left swaps with bottom-right.
- Tile shapes are pixel-perfect; only the palette selection is wrong.
- Scenes whose attribute bytes are symmetric (all four quadrants equal)
  render correctly — which makes the bug maddeningly intermittent.

The failing test is `nes21dbg.quadrant_map_matches_hardware`.

## Your task

1. Run `ctest` and reproduce the failure.
2. Find the defect in `dbg_render.hpp` (`nes21dbg::attribute_bits`).
3. Write `bug-report.md` in this directory containing exactly:
   - **bug**: one sentence,
   - **root cause**: which token/expression is wrong and why the hardware
     disagrees (see LECTURE.md, attribute byte layout),
   - **first observable divergence**: the first test/pixel that differs,
   - **fix**: the corrected expression,
   - **regression test**: why `quadrant_map_matches_hardware` now pins it.

Hint: resist rewriting the function from scratch. The fix is one token.
