# Chapter 38 — MIPS R3000A CPU

The PlayStation's heart is a CoreWare CW33300-compatible core implementing
the MIPS I instruction set at 33.8688 MHz. It looks like a textbook RISC,
but two behaviors make it *the* PS1 CPU rather than any generic MIPS:
**branch delay slots** and **non-interlocked multiply/divide**. This chapter
builds an interpreter that gets both right, and a debugging exercise built
from five classic delay-slot bugs.

Primary reference: [PSX-SPX, CPU section](https://problemkaputt.de/psx-spx.htm#cpuspecifications).

## Study

```text
32 GPRs
HI
LO
PC
COP0
load/store
branches
branch delay slots
exceptions
unaligned operations
```

## Architectural state

| State        | Size    | Notes |
|--------------|---------|-------|
| `$zero..$ra` | 32 × 32 | General purpose registers. `$zero` is hardwired to 0; writes are discarded. `$ra` is just GPR 31 — `jal` is an ordinary register write. |
| `HI`, `LO`   | 32 × 2  | Multiply/divide results. `mult` writes both; `div` writes quotient to `LO`, remainder to `HI`. |
| `PC`         | 32      | Byte address. Reset value `0xBFC00000` (uncached BIOS ROM in KSEG1). |
| COP0         | minimal | Status/Cause/EPC/PRID — introduced here, expanded in ch39. |

There is no flags register. Every comparison (`slt`, branches) is computed
from operand values at execution time.

## Instruction formats

MIPS I has three formats. Everything we implement decodes from these fields:

```text
R-type  op(6) rs(5) rt(5) rd(5) shamt(5) funct(6)
I-type  op(6) rs(5) rt(5) imm16(16)
J-type  op(6) target26(6+26)
```

- `op = 0` (SPECIAL): the `funct` field selects ADDU, shifts, JR, SYSCALL…
- `op = 1` (REGIMM): the `rt` field selects BLTZ/BGEZ (and the linking
  variants BLTZAL/BGEZAL — the ch38 coding test).
- Branch displacement is relative **to the delay slot address (`pc + 4`)**,
  not to the branch itself. Getting this wrong by 4 bytes is bug #3 in
  `90_debug`.
- Jump target: `((pc + 4) & 0xF0000000) | target26 << 2` — the upper 4 bits
  come from the *delay slot* address.

## Load/store and alignment

The memory interface is load/store only, little-endian:

| Op            | Alignment | Notes |
|---------------|-----------|-------|
| `lw/sw`       | 4         | Misaligned address raises an AdEL/AdES exception on real hardware (ch39). Our model asserts callers stay aligned. |
| `lh/lhu/sh`   | 2         | Sign extension is the only difference between `lh`/`lhu`. |
| `lb/lbu/sb`   | 1         | |
| `lwl/lwr/swl/swr` | any  | The unaligned access pair — see below. |

### LWL/LWR/SWL/SWR semantics (little-endian PS1)

Let `a` be the effective address, `b = a & 3`, and `word` the aligned word
at `a & ~3`. With `n(a)` meaning byte at address `a`:

- **LWR rt, a** loads the `(4 - b)` low bytes of the word starting at `a`
  into the least-significant bytes of `rt`; remaining high bytes of `rt`
  are preserved.
  ```text
  rt = (rt & mask_keep_high(b)) | (word >> (8*b))
  ```
- **LWL rt, a** loads the `(b + 1)` low bytes of the aligned word into the
  most-significant bytes of `rt`; remaining low bytes preserved.
  ```text
  rt = (rt & mask_keep_low(3 - b)) | (word << (8*(3-b)))
  ```
- **SWR/SWL** store symmetrically (low part / high part of `rt`).

The canonical unaligned word load of the word starting at `x` is:

```text
lwr $t0, 0($base)      ; low (4 - (x&3)) bytes
lwl $t0, 3($base)      ; high (x&3) bytes
```

Worked example — memory holds `78 56 34 12` at 0x100..0x103 (the little-
endian image of word 0x12345678):

```text
lwr $t0, 1($zero)   ; b=1: loads mem[0x101..0x103] = 56 34 12 into the low
                    ; three bytes of $t0, keeps the previous top byte:
                    ; $t0 = 0xPP123456   (P = untouched old byte)
lwl $t0, 3($zero)   ; b=3: loads the whole aligned word:
                    ; $t0 = 0x12345678
```

Verify your implementation against `02_mem_ops` tests instead of hand
tracing; the table above plus the tests pin the behavior exactly.

## Branches and delay slots

MIPS I branches always execute exactly one **delay slot**: the instruction
at `pc + 4` runs before control transfers. There is no "branch likely"
variant on R3000A — the slot executes unconditionally, even for untaken
branches.

The reference interpreter tracks the explicit triple demanded by the
curriculum solution shape:

```text
current_pc     instruction executing now
next_pc        what executes next unless this instruction redirects it
in_delay_slot  true when current_pc is the slot of the previous branch
```

One step advances the window:

```text
execute instr at current_pc
current_pc' = next_pc
next_pc'    = taken ? target : next_pc + 4
```

Note what this makes *impossible*: executing the slot twice (each advance
moves the window), skipping the slot after `jr` (a jump is just a branch
whose target comes from a register), and losing return addresses (`jal`
links `pc + 8`, the instruction *after* the slot).

A branch executed inside a delay slot is UNPREDICTABLE on real silicon. We
document our choice: it behaves as a normal branch (the newest branch wins).
Tests never depend on the case; do not rely on it either.

## Multiply/divide timing notes

On real hardware `mult` occupies the divider roughly 3–10 cycles depending
on operand values, `div` about 35–37 cycles, and — critically — there is **no
interlock**: code that reads HI/LO immediately after issuing `div` reads
stale results unless separated by enough instructions. Compilers insert
filler; emulators typically model completion as immediate with an inflated
cycle cost.

Our model: result visible immediately; cycle cost 1 for ordinary ops, +4 for
`mult`/`multu`, +36 for `div`/`divu`. Division by zero leaves HI/LO
UNPREDICTABLE per the MIPS manual — we leave them unchanged and document it.

## COP0 introduction

COP0 exists but this chapter only touches it in passing via `PRID`
(processor ID, read-only). Exceptions, SR/CAUSE/EPC and `rfe` are chapter
39's subject; `syscall`/`break` halt the interpreter here precisely because
proper handling needs that machinery.

## Exercises

| Dir | Topic |
|-----|-------|
| `01_alu` | Decode + execute ALU/shift group |
| `02_mem_ops` | Loads/stores incl. the LWL/LWR family |
| `03_branch_delay` | The delay-slot window machine, branches/jumps, muldiv |
| `90_debug` | Five seeded delay-slot bugs |
| `91_challenge` | Headless runner over a hand-assembled test program |
| `99_coding_test` | Unseen family: REGIMM link variants (BLTZAL/BGEZAL) |

External CPU conformance suites ([ps1-tests](https://github.com/avocado-ps1/ps1-tests),
[gte-tests](https://github.com/WhistleMaster/gte_tests)) require user-supplied
ROMs and are wired as optional hidden cases — they never ship in-repo.
