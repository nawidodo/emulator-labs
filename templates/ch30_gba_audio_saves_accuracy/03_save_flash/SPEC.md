# 03_save_flash — SPEC

Implement SRAM and a state-exact flash command machine.

1. ID mode: AA 55 90 enters, reads return mfg/device by address parity,
   AA 55 F0 leaves. Prefixes must match exactly.
2. Erase: AA 55 80 arms; 10 = chip erase, 30 @addr erases the containing
   4 KiB sector. Erased memory reads FF.
3. Program: AA 55 A0 then data write — AND semantics only (flash clears
   bits, never sets).
4. Banks: AA 55 B0 selects bank 0/1 on 128 KiB devices only; addressing is
   bank<<16 | (addr & mask).

SRAM is a plain 32 KiB byte array with overwrite semantics.
