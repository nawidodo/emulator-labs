# Chapter 26 — Thumb, Pipeline and Exceptions

The ARM7TDMI is an ARMv4T core: the **T** means Thumb — a compressed 16-bit
instruction set that halves code size at some performance cost. This chapter
builds the Thumb decoder, makes the 3-stage pipeline explicit, and models
exceptions with their banked registers.

## Thumb vs ARM

- Instructions are **16 bits**; most are two-address forms of their ARM
  counterparts.
- Only branches can be conditional (`B<cond>`), except the combined
  `cond==1110` always form.
- The barrel shifter survives only as format-1 immediate shifts.
- No condition field on data instructions; no S bit — the "group 3"
  arithmetic ops always set flags, MOV does not.
- **PC reads as instruction address + 4** in Thumb state (vs +8 in ARM),
  because the pipeline still prefetches one 16-bit instruction ahead of
  decode.

## Format table (subset implemented)

| Fmt | Encoding top | Instructions |
|-----|--------------|--------------|
| 1   | `000`        | LSL/LSR/ASR Rd, Rm, #imm5 |
| 2   | `00011`      | ADDS/SUBS Rd, Rn, Rm/#imm3 |
| 3   | `001`        | MOV/CMP/ADD/SUB Rd, #imm8 |
| 4   | `010000`     | 16 ALU ops, two registers |
| 5   | `010001`     | ADD/CMP/MOV/BX — high-register forms |
| 6   | `01001`      | LDR Rd, [PC, #imm8*4] — literal pools |
| B1  | `110<cond>`  | B<cond> — 8-bit signed offset, ×2 |
| B2  | `11100`      | B — 11-bit signed offset, ×2 |
| BL  | `11110h, 11111l` | two halfwords, 23-bit signed offset ×2 |
| PUSH/POP | `1011b0` | store/load register list, SP writeback |

## Pipeline

Three stages: **fetch → decode → execute**. While one instruction executes,
the next is decoded and the one after that is fetched. Consequences:

- An executing Thumb instruction sees `PC = own_address + 4`.
- A taken branch flushes decode/fetch: costs 2S + 1N refill cycles (we model
  3 cycles, like taken ARM branches).
- Literal loads (format 6) and ADD Rd, PC use the +4 base — literal pools are
  placed accordingly by compilers.

## Mode switch

`BX Rm` jumps and sets the T bit from Rm bit 0. ARMv4T has no `BLX`, so calls
into Thumb from ARM use the classic veneer:

```asm
    ldr r3, =thumb_entry   ; low bit set
    blx_veneer:
    bx  r3                 ; switch to Thumb
```

Thumb code returns with `BX LR` (T clears because the LR value is even).
Thumb `BL` is two halfwords: first `11110 HHHHHHHHHHH`, second
`11111 LLLLLLLLLLL`; offset = sign_extend(H:L << 12) << 1 relative to the
second halfword's address + 4, and `LR = second_hw_address + 4`.

## Exceptions

Each exception forces the core into a privileged mode, banks the visible
registers, and vectors:

| Vector    | Mode       | LR saved      | IRQ/FIQ disabled |
|-----------|------------|---------------|------------------|
| Reset     | Supervisor | —             | both             |
| UNDEF     | Undefined  | PC+4          | —                |
| SWI       | Supervisor | PC+4          | —                |
| Prefetch abort | Abort | PC+4 (v4: +4) | —                |
| IRQ       | IRQ        | PC+4          | IRQ              |
| FIQ       | FIQ        | PC+4          | IRQ+FIQ          |

Banked per mode (ARMv4): r13, r14, SPSR everywhere; FIQ additionally banks
r8–r12 — which is why fast interrupts can run without saving those.

Return undoes it: `MOVS PC, LR` copies SPSR back to CPSR. Because the
exception LR is `PC+4` (one pipeline slot past the faulting/interrupted
instruction in our 3-stage model), IRQ handlers return with
`SUBS PC, LR, #4` on cores where LR = PC+8-relative; on the ARM7TDMI the
hardware already saved the right value for plain `MOVS PC, LR` after SWI,
while IRQ handlers that re-enable interrupts must account for the reload —
we model the classic offsets: SWI/UNDEF return `MOVS PC, LR`; IRQ/FIQ return
`SUBS PC, LR, #4`.

## Why this matters

GBA games run almost entirely in Thumb (BIOS, game code), calling ARM
routines (in EWRAM/IWRAM) for speed-critical pieces. Your CPU must switch
modes flawlessly and preserve state across exceptions — GBA games hit SWI
sound decompression and IRQ vblank handlers within the first frames.
