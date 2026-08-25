# ch49 Challenge — deterministic synthetic boot

Assemble-and-boot a real program (no test hooks): `boot_handshake.bin`
(33 words, listing in `tests/public/ch49_ps1_system_scheduling/roms/`)
exercises the full cd -> spu -> gpu handshake through the event scheduler:

1. masks INTC lines {GPU=1, CD=2, SPU=9};
2. logs milestone 1, enables the SPU sample-period IRQ (line 9 every 768
   cycles — 26 latches before it is silenced);
3. kicks a CD sector read whose completion lands at cycle 20000 (IRQ2);
4. queues a 1024-pixel GP0 command with IRQ-on-idle enabled (line 1 at
   cycle 695 via the 11/7 pixel ratio);
5. polls I_STAT for each completion, acks, logs milestones 2/3/7, HALT.

Acceptance: `ch49_91_challenge_tests` GREEN — the FNV-1a 64 hash of the
event log equals `kGoldenBootHandshakeFnv64` in `golden.hpp`, generated
by the reference solution and reproducible byte-for-byte (run twice).
The same log is committed as
`tests/public/ch49_ps1_system_scheduling/goldens/boot_handshake.eventlog`.
