# ch48 Coding Test — unseen SIO transaction log

The hidden suite replays **unseen** transaction scripts through the runner
built from `03_card_image` (`ch48_03_sio_runner`) against an unseen memory
card image. Your implementation must produce the exact expected response
bytes; the grade hashes the `--hash-frame` dump with FNV-1a 64.

## Contract

* ROM: a raw 131072-byte `.mcr` card image, mounted into slot 1's card.
* Script: the grammar documented in `03_card_image/runner_main.cpp` and
  `03_card_image/SPEC.md` (`SLOT/PAD/XFER/CARDFILE/CARDSTORE/RUN`).
* Output: concatenated raw response bytes of all XFER commands.
* Determinism: identical input always produces byte-identical output —
  no time, no RNG.

## What is exercised

Everything from this chapter composes: pad ID + button reporting and ACK
framing (ex01), card read/write/getid/erase with XOR checksums, flag
sequences and bad-sector behavior (ex02), directory bad-block scanning,
dual-slot routing behind CTRL bits 12/13, and `.mcr` mount/store (ex03).

## Preparing

1. Implement every `TODO(n)` in exercises 01–03.
2. `LABS=ch48_ps1_controllers_memcards make skels && make build`
3. Rehearse with the public fixture:
   `build/skels/ch48_ps1_controllers_memcards/03_card_image/ch48_03_sio_runner \
      --rom tests/public/ch48_ps1_controllers_memcards/cards/sample.mcr \
      --input-file tests/public/ch48_ps1_controllers_memcards/scripts/card_ops.script \
      --hash-frame /tmp/sio.bin --headless`
4. The printed `fnv64` must equal the golden recorded in
   `tests/public/ch48_ps1_controllers_memcards/goldens/provenance.md`.

The hidden scripts drive both slots, mix pad and card sessions in one log,
and include sectors the reference answers with the bad-sector flag — no
pattern matching will save you, only a protocol-correct device model.

Grading cases live in `tests/hidden/ch48_ps1_controllers_memcards/
manifest.json` (see also that directory's `provenance.md`).
