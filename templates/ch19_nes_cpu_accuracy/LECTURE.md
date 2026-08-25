# Chapter 19 — NES CPU Accuracy

A CPU that "works" and a CPU that is *accurate* are different artifacts.
The first executes programs; the second produces the same bus transcript,
register states, and cycle counts as a real 2A03 — which is what lets you
diff your emulator against reference logs instead of vibes.

## The four ways off the instruction path

```text
RESET  vector $FFFC/$FFFD. S -= 3. I forced set. 7 cycles.
BRK    vector $FFFE/$FFFD. Pushes P WITH B SET (bit 4). 7 cycles.
IRQ    vector $FFFE/$FFFF. Level-sensitive, honored only while I = 0.
NMI    vector $FFFA/$FFFB. EDGE-LATCHED on quiet->high; ignores I.
```

The stacked copy of P is the fingerprint: BRK sets B, hardware interrupts
leave it clear. Everything else — two dummy opcode reads, PCH/PCL/P push
order, I being raised, the vector fetch — is shared.

NMI latching deserves emphasis: the PPU asserts /NMI during vblank and
deasserts at the next read of $2002. A request is captured on the
*transition*, so holding the line high for a thousand cycles yields one
interrupt, and the game code that reads $2002 to clear the line is really
arming the NEXT edge.

## Cycles are bus transactions

Every 6502 cycle is a read or a write except the rare internal ticks.
Three consequences ch18's core already hinted at, made observable here:

1. **RMW double-write.** `INC $40` reads $40, writes $40 back with the OLD
   value, then writes the new one. That middle write is why RMW costs one
   more cycle than you'd guess — and why devices that snoop the bus can
   see transient values.
2. **Speculative indexed accesses.** `STA $20FE,X` with X=2 performs a
   READ at $20FE before correcting to $2100 — always, crossed page or
   not. Plain indexed loads only pay it when base+index crosses a page.
3. **Internal penalties.** zp,X/zp,Y/(zp,X) address arithmetic burns a
   cycle with no observable access at all.

## The unofficial opcodes

About 1/3 of the opcode space decodes on a real 6502 even though MOS never
documented it. Games used them. We implement a documented subset:

```text
NOPs (many shapes, all real timings)   LAX  A = X = mem
SAX  mem = A & X (no flags)            DCP  DEC mem, then CMP A
ISB  INC mem, then SBC A               SLO  ASL mem, then ORA A
RLA  ROL mem, then AND A
```

Each is wired into the decode table like an official opcode — because on
the die, it is one.

## Traces as ground truth

nestest's log is the community's Rosetta stone: PC, raw bytes, disassembly,
registers, cycle count per instruction. Chapter exercises rebuild that
shape (`pc=… op=… cyc=…` canonical lines plus nestest-style columns), and
the coding test builds the tool every emulator author eventually writes:
find the FIRST diverging line in a 50k-instruction pair.

## Study references

- https://www.nesdev.org/6502_cpu.txt — the definitive cycle-by-cycle doc
- https://www.nesdev.org/wiki/CPU_interrupts
- https://www.nesdev.org/wiki/Emulator_tests — nestest and friends
