# Debugging Exercise — The Case of the Sliding Window and the Sideways Screen

Your chapter 9 machine boots, runs, and draws — but two things look
wrong. Two bugs are seeded in `90_debug/` (marked `BUG(1)` / `BUG(2)` in
the generated skeleton; the comments vanish in your fixed copy).

## Symptoms

1. **Shifted results from IN 3.** Every shift-register read returns the
   window one bit position away from what the hardware contract says,
   and at amount 7 the answer wraps around to something absurd. Write
   `0xDE`, `0xAD`, set amount 3: a correct board reads `0xBB`.
2. **The picture is scrambled.** Not shifted, not mirrored — every
   8-pixel column of VRAM shows up as an 8-pixel-tall row band. Vertical
   strokes render horizontal. Diagonal content still lines up, which is
   exactly why it looks "almost right" at a glance.

## Your task

1. Reproduce both failures with `ctest -R ch09_90_debug` (RED on
   skeleton).
2. For each bug record:
   ```text
   bug:
   root cause:
   first observable divergence:   (test / pixel / value)
   fix:
   regression test:
   ```
3. Fix the code until the suite is GREEN.
4. Commit `bug-report.md` next to this file's directory in your solution.

## Hints

- Bug 1 is a classic off-by-one hiding inside a mask that *looks* like it
  is there to protect you. Ask what the window should be when amount == 7.
- Bug 2 is about the byte index only. The bit-in-byte decode is already
  correct; do not touch it. The framebuffer is COLUMN-major: byte
  `(col*32 + y/8)` holds column `col`, rows `y&~7 .. y|7`.

Curriculum §54: hunt the FIRST divergence, not the last visible symptom.
