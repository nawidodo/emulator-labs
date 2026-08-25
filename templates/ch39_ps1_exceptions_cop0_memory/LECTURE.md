# Chapter 39 — PS1 Exceptions, Coprocessor 0 and Memory

## Lecture

Study:

```text
exceptions
COP0
status
cause
EPC
KSEG0
KSEG1
RAM
scratchpad
BIOS
memory control
```

Primary machine reference: nocash PSX-SPX, "CPU" chapters —
<https://problemkaputt.de/psx-spx.htm#cpuexceptionsvectors>.

---

## 1. Why the R3000A looks the way it does

The PlayStation's CPU is a MIPS **R3000A** (33.868 MHz): a 32-bit MIPS I
core with **no virtual memory** in practice, no TLB entries behind the
architecture's TLB instructions, and a coprocessor 0 (COP0) that owns the
exception machinery. Everything the "operating system" is — the BIOS kernel
at `0xBFC00000`, its syscall table, its interrupt dispatcher — stands on
three COP0 registers (`SR`, `CAUSE`, `EPC`), one instruction (`RFE`), and a
memory map whose entire address translation is a single AND with
`0x1FFFFFFF`.

That last sentence is only a slight exaggeration, and this chapter builds
each piece of it.

## 2. The memory map: segments, mirrors, cacheability

### 2.1 The four segments

| Segment   | Range                  | Size | Privilege   | Caches? |
|-----------|------------------------|------|-------------|---------|
| KUSEG     | `00000000`-`7FFFFFFF`  | 2 GB | Kernel/User | Yes     |
| KSEG0     | `80000000`-`9FFFFFFF`  | 512M | Kernel      | Yes     |
| KSEG1     | `A0000000`-`BFFFFFFF`  | 512M | Kernel      | **No**  |
| KSEG2     | `C0000000`-`FFFFFFFF`  | 1 GB | Kernel      | No code |

On real MIPS, KSEG2 is TLB-mapped kernel space; on the PSX it contains
exactly one thing worth knowing — the cache-control I/O ports at
`FFFE0000h`. Everything else faults.

Physical memory occupies the first 512 MB, so all three lower segments
alias it:

```text
physical = vaddr & 0x1FFFFFFF     // the whole PSX "MMU"
```

The low 29 bits *are* the physical address. `00001000h`, `80001000h` and
`A0001000h` are three doors to the same byte.

### 2.2 What lives where

| Physical base | Mirrors at            | Device                                        |
|---------------|-----------------------|-----------------------------------------------|
| `00000000`    | KUSEG/KSEG0/KSEG1     | 2048 KB Main RAM                              |
| `1F000000`    | `9F000000`/`BF000000` | 8192 KB Expansion Region 1                    |
| `1F800000`    | `9F800000` (**no KSEG1**) | 1 KB Scratchpad (D-cache as fast RAM)     |
| `1F801000`    | `9F801000`/`BF801000` | I/O ports (memory control, timers, ...)       |
| `1FC00000`    | `9FC00000`/`BFC00000` | 512 KB BIOS ROM                               |

Details that matter for an emulator:

- **RAM mirrors through 8 MB.** RAM_SIZE (below) defaults to a decode
  window of 8 MB holding four images of the physical 2 MB: `paddr & 0x1FFFFF`
  after decoding `paddr < 0x800000`. Beyond the window: locked → exception.
- **The scratchpad has NO KSEG1 mirror** (note the empty column above).
  `BF800000h` does not decode. This is a favourite accuracy trap.
- **Cacheability is a property of the segment, not the device.** KSEG1 is
  uncached — which is why the BIOS executes from `BFC00000h`: boot code must
  not depend on cache state. KUSEG/KSEG0 fetches are cached; self-modifying
  code there needs cache flushes (the BIOS does exactly this dance).
- **BIOS writes are silently lost** (ROM). Reads return what was programmed.

### 2.3 Memory control ports (`1F801000`-`1F80103F`)

Reset values documented by PSX-SPX (the BIOS rewrites several during init):

| Port        | Name              | Reset        | Notes                          |
|-------------|-------------------|--------------|--------------------------------|
| `1F801000`  | EXP1_BASE         | `1F000000h`  | Expansion region 1 base        |
| `1F801004`  | EXP2_BASE         | `1F802000h`  | Expansion region 2 base        |
| `1F801008`  | EXP1_DELAY/SIZE   | `0013243Fh`  | 512 KB, 8-bit bus              |
| `1F80100C`  | EXP3_DELAY/SIZE   | `00003022h`  |                                |
| `1F801010`  | BIOS_DELAY/SIZE   | `0013243Fh`  | 512 KB, 8-bit bus              |
| `1F801014`  | SPU_DELAY         | `200931E1h`  |                                |
| `1F801018`  | CDROM_DELAY       | `00020843h`  | (or `00020943h`)               |
| `1F80101C`  | EXP2_DELAY/SIZE   | `00070777h`  | 128 bytes, 8-bit bus           |
| `1F801020`  | COM_DELAY         | `00031125h`  | (or `0000132Ch`/`00001325h`)   |
| `1F801060`  | RAM_SIZE          | `00000B88h`  | see below                      |
| `1F801070`  | I_STAT            | —            | interrupt status (ch40)        |
| `1F801074`  | I_MASK            | —            | interrupt mask   (ch40)        |

`RAM_SIZE` bits 11:9 select the RAM decode window:
`4 = 2MB + 6MB locked` (the electrically correct setting),
`5 = 8MB` (what the BIOS actually programs, i.e. `00000888h` after init).
The reset value `00000B88h` already carries bits 11:9 = 5.

## 3. COP0 register file

Accessible only via `mfc0`/`mtc0` (and only in kernel mode unless
`SR.CU0=1`). Encoding:

```text
mfc0 rt, rd : 0 4 00 rt rd 000000   -> 0100 00 00000 ttttt fffff 000000
              word = 40000000h | (rt<<16) | (rd<<11)
mtc0 rt, rd : same but rs field = 00100
              word = 40800000h | (rt<<16) | (rd<<11)
rfe         : 42000010h             (opcode COP0, rs=10000, funct=10000)
```

Worked examples: `mfc0 $k0,$13` = `401A6800h`; `mtc0 $t0,$12` = `40886000h`.

Registers we model (PSX numbering):

| rd | Name  | Access          |
|----|-------|-----------------|
| 12 | SR    | R/W             |
| 13 | CAUSE | R (bits 9:8 W)  |
| 14 | EPC   | R/W*            |
| 15 | PRID  | R               |

\* hardware treats EPC as read-only for handlers, but the BIOS *itself*
writes EPC when emulating syscalls — our model allows `mtc0`.

### 3.1 SR — status register (r12)

```text
 31 30 29 28 ... 23 22 21 ... 17 16 15 ............ 8 7 6 5 4 3 2 1 0
 CU3 CU2 CU1 CU0  -  BEV TS SwC PZ CM PE IsC   Im[7:0]   - - KUo IEo KUp IEp KUc IEc
```

| Bit(s) | Field | Meaning                                                        |
|--------|-------|----------------------------------------------------------------|
| 0      | IEc   | current interrupt enable                                       |
| 1      | KUc   | current mode (0 = kernel, 1 = user)                            |
| 2      | IEp   | previous IE                                                    |
| 3      | KUp   | previous mode                                                  |
| 4      | IEo   | old IE                                                         |
| 5      | KUo   | old mode                                                       |
| 8-15   | Im    | per-source interrupt mask (bit 8 masks CAUSE.IP8, etc.)        |
| 16     | IsC   | isolate D-cache: loads/stores hit the scratchpad, not RAM      |
| 17     | SwC   | swap I/D caches                                                |
| 22     | BEV   | boot exception vectors: 0 = RAM/KSEG0, 1 = ROM/KSEG1           |
| 28-31  | CU0-3 | coprocessor usability (CU2 = GTE)                              |

The **triple shadow** (current/previous/old) is the R3000's nesting
hardware: two exception levels can be taken before information is lost.
Reset value on the PSX: `BEV=1`, everything else cleared — kernel mode,
interrupts disabled, vectors in ROM. PRID reports `00000001h`
(CXD8530BQ/CXD8530CQ) or `00000002h` (CXD8606CQ).

### 3.2 CAUSE — cause register (r13)

```text
 31 30 29 28 ... 15 ......... 8 7 ......... 2 1 0
 BD   CE[1:0]    -    IP[7:0]    -  ExcCode[4:0] -
```

| Bits | Field   | Meaning                                                     |
|------|---------|-------------------------------------------------------------|
| 6:2  | ExcCode | what happened (table below)                                 |
| 15:8 | IP      | interrupt pending. Bits 9:8 are R/W software interrupts;    |
|      |         | an IP bit causes an interrupt iff the matching Im bit is set|
| 29:28| CE      | coprocessor number of a COP fault                           |
| 31   | BD      | faulting instruction was in a branch delay slot             |

ExcCode values (PSX-relevant subset):

| Code | Name | Cause                                            |
|------|------|--------------------------------------------------|
| 00h  | Int  | interrupt (IP & Im, IEc)                         |
| 04h  | AdEL | misaligned/unmapped load or instruction fetch    |
| 05h  | AdES | misaligned/unmapped store                        |
| 06h  | IBE  | bus error on instruction fetch                   |
| 07h  | DBE  | bus error on data access                         |
| 08h  | Sys  | `syscall` instruction                            |
| 09h  | BP   | `break` instruction                              |
| 0Ah  | RI   | reserved/unknown opcode                          |
| 0Bh  | CpU  | unusable coprocessor (e.g. GTE with CU2=0)       |
| 0Ch  | Ov   | arithmetic overflow                              |

(TLB codes 01h-03h exist architecturally; the PSX has no usable TLB.)

### 3.3 EPC (r14)

Where to resume. If `CAUSE.BD=0`, EPC = address of the faulting
instruction. If `BD=1`, EPC = address of **the branch** — the slot address
is not saved anywhere, so the handler knows resuming at `EPC+4` would be
the slot again and at `EPC+0` re-executes the branch. Policy belongs to the
handler:

```text
interrupt (Int):        return to EPC+0   (retry; re-executes the branch,
                                          required since the branch target
                                          is lost otherwise)
syscall / break:        usually EPC+4     (skip the emulated instruction)
faults (AdEL etc.):     fix up, or skip:  EPC+4 normally, EPC+8 if BD
GTE-in-delay-slot bug:  BIOS checks opcode at EPC and bumps by 4 — see
                        PSX-SPX "Interrupts vs GTE Commands"
```

## 4. Exception entry — a state transition, not a jump

The reference design (and real silicon) treats entry as updating
architectural state; the fetch redirect falls out of it:

```text
function take_exception(req):
    bd = req.in_delay_slot
    COP0.EPC    = bd ? req.branch_pc : req.pc
    CAUSE.ExcCode = req.code
    CAUSE.CE    = req.coprocessor
    CAUSE.BD    = bd
    SR.IEo,KUo := SR.IEp,KUp        # shadow pairs slide down one level
    SR.IEp,KUp := SR.IEc,KUc
    SR.IEc,KUc := 0,0               # kernel mode, interrupts disabled
    vector = SR.BEV ? BFC00180h : 80000080h
    PC := vector
```

Exception vectors (PSX-SPX):

| Exception    | BEV=0       | BEV=1       |
|--------------|-------------|-------------|
| Reset        | `BFC00000h` | `BFC00000h` |
| UTLB miss    | `80000000h` | `BFC00100h` |
| COP0 Break   | `80000040h` | `BFC00140h` |
| **General**  | `80000080h` | `BFC00180h` |

(The PSX BIOS uses the BEV=0 general vector after copying handlers into
RAM; our synthetic stub keeps BEV=1 and lives entirely in ROM.)

Priority when multiple conditions coincide (highest first): Reset, AdEL /
AdES / DBE, Ovf, Int, then decode-stage exceptions Sys/BP/RI/CpU, then
fetch-stage AdEL/IBE.

### 4.1 RFE and the canonical return

`rfe` pops exactly one shadow level — `prev→current`, `old→previous`,
old discarded. It jumps **nowhere**; the idiom puts it in the delay slot of
the jump back:

```asm
    mfc0  $k1, $14        ; k1 = EPC
    ; ... inspect CAUSE, decide retry vs skip, maybe adjust EPC ...
    mtc0  $k1, $14
    mfc0  $k1, $14        ; reload (possibly adjusted) return address
    jr    $k1
    rfe                   ; delay slot: shadows restored as jr retires
```

Two subtleties worth memorising:

1. Writing `SR.IEc` from 0→1 via mtc0 does **not** take a pending interrupt
   until after the next instruction; but an `rfe` that sets IEc 0→1 *can*
   fire immediately.
2. A nested handler destroys `$k0/$k1` (there is "no way to leave all
   registers intact", as nocash puts it). Truly reentrant handlers stack
   k0/k1/EPC/SR before re-enabling interrupts.

## 5. Worked example — the boot-mini stub

`91_challenge/fixtures/bios_stub.asm.txt`, annotated flow:

```text
BFC00000  lui   $t0,0x0040        ; SR := 00400000h (BEV=1)
BFC00008  mtc0  $t0,$12
BFC0000C  bal   resume            ; ra = BFC00014
BFC00010    syscall               ; DELAY SLOT!
                                  ;   -> BD=1, EPC=BFC0000C, vec=BFC00180
BFC00180  mfc0  $k0,$13          ; CAUSE = 80000020h (BD | Syscall)
BFC00188  lui   $t0,0x9F80       ; scratchpad via KSEG0
BFC0018C  sw    $k0,0($t0)       ; sp[0] = CAUSE
BFC00194  addiu $k1,$k1,8        ; skip branch AND slot
BFC00198  mtc0  $k1,$14          ; EPC = BFC00014
BFC001A0  jr    $k1
BFC001A4    rfe                  ; pop shadows in the jump's delay slot
BFC00014  nop                    ; execution lands here (EPC was bumped)
BFC00024  ori   $t2,$0,0xC0DE
BFC00028  sw    $t2,12($t1)      ; completion marker in scratchpad
BFC0002C  b     halt             ; self-loop
```

Run headless:

```sh
ch39_91_challenge_runner --rom fixtures/bios_stub.bin --cycles 48 \
    --trace boot_mini.log --hash-frame boot_mini.state
```

The trace shows the syscall line carrying
`exc=syscall bd=1 epc=bfc0000c vec=bfc00180`; the state digest pins the
scratchpad (`sp[0]=80000020h`, `sp[4]=BFC0000Ch`, `sp[12]=0000C0DEh`),
registers and COP0 resume state.

## 6. Timing notes

- Exception entry overhead on real hardware is roughly a handful of cycles
  (pipeline flush plus the state update); emulators usually charge a fixed
  cost. What must be *deterministic* is the architectural result, not the
  cycle count.
- Interrupt sampling: an asserted line is recognised between instructions;
  the pending instruction's delay-slot position decides BD/EPC (this is the
  99_coding_test scenario).
- Uncached (KSEG1) BIOS fetches make early-boot timing reproducible; cached
  regions need the IsC/SwC games documented above to model precisely.

## 7. Study checklist

- [ ] Recite the segment table and the single-AND translation.
- [ ] Write SR/CAUSE bit layouts from memory, including the shadow pairs.
- [ ] Encode `mfc0`/`mtc0`/`rfe` words by hand.
- [ ] State the BD/EPC rule and one retry-vs-skip policy per ExcCode.
- [ ] Explain why the scratchpad has no KSEG1 mirror and why the BIOS runs
      uncached.
- [ ] Trace the boot-mini stub cycle by cycle without looking.
