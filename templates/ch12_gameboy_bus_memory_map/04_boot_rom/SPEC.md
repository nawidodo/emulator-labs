# SPEC — 04_boot_rom

Overlay the 256-byte boot image at 0000-00FF and take it back off on
the FF50 handshake (three `TODO` blocks in `boot_bus.hpp`):

1. `BootRom::read` — serve bytes from the committed boot image.
2. `Ff50Unmapper::write` — ANY write whose address is $FF50 (value
   ignored) detaches the boot device from the bus and clears the
   `bootMapped` flag.
3. `attachBootOverlay` — put the FF50 trap and the boot entry IN FRONT
   of everything already attached (`attachFront`); appended entries
   never win a first-match scan against the cartridge.

Documented divergence from silicon (also in LECTURE.md): a real DMG
additionally unmaps when execution crosses $0050 internally; this model
has no CPU, so it keys on the FF50 write only.

Acceptance: `ch12_04_boot_tests` green — boot shadows the cart at
reset, any-value FF50 write reveals it, double unmap is harmless,
decoy writes keep the overlay armed, echo keeps aliasing throughout.
