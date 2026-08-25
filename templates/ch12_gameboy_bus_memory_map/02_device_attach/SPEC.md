# SPEC — 02_device_attach

Build the full Game Boy machine map out of device objects (three `TODO`
blocks in `machine.hpp`):

1. `CartRom::read` — serve bytes from the committed image; short images
   pad the top of the 32 KiB window with `$FF`; writes are dropped.
2. `Machine::attachLowDevices` — cartridge ROM 0000-7FFF, VRAM
   8000-9FFF, external RAM A000-BFFF.
3. `Machine::attachHighDevices` — WRAM C000-DFFF, OAM FE00-FE9F,
   I/O FF00-FF7F, HRAM FF80-FFFE, IE latch FFFF.

Provided: `IeLatch` (one byte of state at FFFF), `Machine` wiring,
`makeTestCart` (deterministic pattern image).

FEA0-FEFF is intentionally left UNATTACHED: it falls through to the
documented unusable-page policy from exercise 01.

Acceptance: `ch12_02_attach_tests` green — every device round-trips a
unique marker through the bus and no region ever bleeds into another.
