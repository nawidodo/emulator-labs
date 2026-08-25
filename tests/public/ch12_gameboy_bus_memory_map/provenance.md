# provenance — tests/public/ch12_gameboy_bus_memory_map

This chapter has no binary goldens. Every fixture is deterministic and
compiled into the exercise binaries:

* cart/boot images are generated arithmetically inside the sources
  (`makeTestCart`, `BootRom::makeSyntheticBoot`, `makeRom`,
  `makeFourBankRom`) — no commercial ROM content, nothing to commit;
* the hidden manifest (tests/hidden/ch12_gameboy_bus_memory_map/) is
  pure labstest filter cases plus one optional Mooneye reference
  (`roms/gb/mooneye/mem_timing.bin`, upstream Mooneye Test ROMs,
  marked optional so grading skips honestly when absent).

No `.bin` files were hand-created or hand-edited for this chapter.
