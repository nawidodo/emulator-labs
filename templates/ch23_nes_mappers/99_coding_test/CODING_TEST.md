# Coding test — ch23: implement the MFX-1 mapper from its register spec

At grading time you receive a synthetic cartridge and an op script you
have never seen. Your Chapter 23 build must execute the script through an
MFX-1 implementation that is byte-exact against the reference.

## The MFX-1 (fictional board, mapper number $99)

Cart: PRG ROM in 16 KiB banks (power-of-two count), CHR ROM in 2 KiB
units (power-of-two count; zero units = 8 KiB CHR RAM), plus 8 KiB of
PRG RAM at `$6000-$7FFF`.

### Register file

Four registers R0-R3 live in `$8000-$BFFF`. The write address bits 1-0
select the register; any address inside the window works. Writes to
`$C000-$FFFF` are ignored entirely. Unmapped CPU reads return `0x00`.

| Reg | Meaning |
|---|---|
| R0 | PRG bank for `$8000-$BFFF`, masked with `prg_banks - 1` |
| R1 | mode: bit0 = 0 -> `$C000-$FFFF` shows the LAST bank; bit0 = 1 -> it MIRRORS R0's bank |
| R2 | CHR unit for the "echo window" (below), masked with `chr_units - 1` |
| R3 | one-shot IRQ timer, period = value bits 4-0 |

### Echo CHR

The whole PPU `$0000-$1FFF` space shows ONE 2 KiB unit — `(R2 & mask)` —
replicated four times ("echo CHR": every quadrant mirrors quadrant 0).
With no CHR ROM the board has flat writable CHR RAM instead.

### One-shot IRQ timer

Writing R3 arms the timer: `count = value & 0x1F`, IRQ line cleared.
The timer ticks on every FOURTH register write since boot (a global write
counter `wc`; a tick happens when a register write lands with `wc % 4 == 0`,
counting that write). When `count` reaches 0 through a tick, the timer
disarms and LATCHES the IRQ line until the next R3 write rearms it.

## Op-script grammar (runner CLI)

```
ch23_99_mfx_runner --rom CART.nes --script FILE [--trace OUT]
wr <hexaddr> <hexval>   CPU write through the mapper
rd <hexaddr>            logs "rd <addr>=<hh>"
prd <hexaddr>           logs "prd <addr>=<hh>"
snap                    logs "mfx r0=<hh> r1=<hh> r2=<hh> r3=<hh> wc=<d> irq=<d>"
```

The grader hashes the emitted log (FNV-1a 64) against reference values
generated from the same fixtures. Rehearse with the unit tests in this
directory (`ch23_99_mfx_tests`) — they pin every rule above except the
exact hidden cart/script combination.
