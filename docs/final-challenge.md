# Final Challenge — MiniConsole-32

You receive a complete specification for a fictional machine, **MiniConsole-32**, plus
everything reusable you built during the course: your loader, CPU, bus, scheduler, GPU,
APU and test-harness libraries, and the public test programs shipped with this challenge
(`tests/public/final_challenge/`). Nothing about the machine is implemented for you.

Your task: make the machine reach the defined milestones in §10. After that, grading runs
**unseen cartridge ROMs** against your emulator. The unseen ROMs test whether you learned
**emulator engineering** — specification reading, architecture, cycle accounting,
differential testing — not memorized PS1 code.

Every number in this document is normative and self-consistent. Implement exactly what is
written; do not improvise timings, encodings or formats.

---

## 1. Overview

MiniConsole-32 is a fully synthetic console inspired by every system in the course:

```text
32-bit RISC CPU          16 registers, branch delay slots, ~MIPS flavor
2 MB RAM                 unified memory, no private VRAM
memory-mapped GPU        tilemap layer + solid-color triangles + fill rectangles
DMA controller           4 channels, cycle-counted, stalls the CPU
2 countdown timers       prescaled, IRQ-capable
interrupt controller     5 IRQ lines, fixed priority, write-1-to-clear
4-channel audio          streamed PCM from RAM, fractional stepping, 50 kHz mix
gamepad                  8 buttons, latched per frame
ROM cartridge            64-byte header + PRG payload, checksummed
```

Global model:

```text
CPU clock        21,000,000 cycles/second (nominal; the emulator counts cycles, not seconds)
Frame            350,000 cycles  → exactly 60 frames/second
VBlank           cycles 345,000..349,999 of every frame (5,000 cycles)
Audio mix tick   every 420 cycles → exactly 50,000 mixed samples/second
Endian           little-endian everywhere, including instruction fetch
```

All timing is expressed in CPU cycles. There are no wait states, no caches and no
nondeterminism (see §11).

---

## 2. CPU / Instruction Set

### 2.1 Programmer-visible state

```text
r0..r15   sixteen 32-bit general registers; r0 is HARDWIRED ZERO
          (writes to r0 are discarded, reads return 0x00000000)
pc        32-bit program counter
EFLAGS    bit 0 = GIE (global interrupt enable); all other bits read 0
```

Software convention (loaders and public ROMs rely on it):

```text
r0        zero           r8..r12   caller-saved scratch
r1..r12   general        r13       frame pointer (optional, unused by fixtures)
r14       ra — return address (set by JAL/JALR)
r15       sp — stack pointer (loaded from cartridge header at reset)
```

There are **no arithmetic flags**. Comparisons materialize into registers (`SLT`/`SLTU`),
branches compare two registers directly.

### 2.2 Instruction encoding

All instructions are exactly 32 bits, little-endian in memory. Three formats:

```text
FORMAT-I   op[31:26] a[25:22] b[21:18] imm[17:0]

FORMAT-R   op[31:26] a[25:22] b[21:18] c[17:14] z[13:0]
           c is the third register; z must be assembled as 0 and is ignored by the CPU

FORMAT-J   op[31:26] tgt[25:0]
```

Field meanings per class:

```text
3-reg ALU (FORMAT-R)   a = rd (dest), b = rs, c = rt
shift-by-imm           a = rd, b = rs, imm[4:0] = shamt (imm[17:5] ignored)
ALU-immediate          a = rd, b = rs, imm = 18-bit (signed for ADDI/SLTI/SLTIU,
                       zero-extended for ANDI/ORI/XORI)
LUI                    a = rd, imm = 18-bit; rd = imm << 14 (any 32-bit constant =
                       LUI hi18 then ORI lo18)
loads/stores           a = base register, b = data register (rt), imm = signed offset
                       address = rs + sext(imm)
branches               a, b = registers compared, imm = SIGNED word offset;
                       target = (pc + 4) + (sext(imm) << 2)   [±512 KiB window]
J / JAL (FORMAT-J)     target = tgt << 2 (absolute, reaches the whole 256 MB map)
JR                     target = value of register b
JALR                   target = register b; link register = a (a must not be r0)
```

### 2.3 Opcode table

```text
op     mnemonic   format  semantics
--------------------------------------------------------------------------------------------
0x00   SYS        I       fn = imm[5:0] (imm[17:6] must be 0):
                            0x00        NOP (canonical NOP is the whole word 0x00000000)
                            0x01  SRD   a ← sysreg[b]        (read system register)
                            0x02  SSRV  sysreg[a] ← b        (write system register)
                            0x03  RTE   EFLAGS ← ESTSAV; pc ← EPC
0x01   ADD        R       a ← b + c                (wraps mod 2^32)
0x02   SUB        R       a ← b − c
0x03   MUL        R       a ← low 32 bits of b × c (signed, irrelevant for low bits)
0x04   DIV        R       a ← b ÷ c signed, truncate toward zero; c = 0 → a ← 0, no trap
0x05   AND        R       a ← b & c
0x06   OR         R       a ← b | c
0x07   XOR        R       a ← b ^ c
0x08   SLT        R       a ← (signed b < signed c) ? 1 : 0
0x09   SLTU       R       a ← (unsigned b < unsigned c) ? 1 : 0
0x0A   SHL        R       a ← b << (c & 31)
0x0B   SHR        R       a ← b >> (c & 31)              (logical)
0x0C   SAR        R       a ← b ~> (c & 31)              (arithmetic)
0x0D   ADDI       I       a ← b + sext(imm)
0x0E   ANDI       I       a ← b & zext(imm)
0x0F   ORI        I       a ← b | zext(imm)
0x10   XORI       I       a ← b ^ zext(imm)
0x11   SLTI       I       a ← (signed b < sext(imm)) ? 1 : 0
0x12   SLTIU      I       a ← (unsigned b < sext(imm)) ? 1 : 0   (note: imm is sign-
                                                                 extended THEN compared unsigned)
0x13   LUI        I       a ← imm << 14
0x14   SHLI       I       a ← b << imm[4:0]
0x15   SHRI       I       a ← b >> imm[4:0]              (logical)
0x16   SARI       I       a ← b ~> imm[4:0]              (arithmetic)
0x17   LB         I       a ← sext(byte  [b + sext(imm)])
0x18   LBU        I       a ← zext(byte  [b + sext(imm)])
0x19   LH         I       a ← sext(half  [b + sext(imm)])    address must be even
0x1A   LHU        I       a ← zext(half  [b + sext(imm)])    address must be even
0x1B   LW         I       a ← word  [b + sext(imm)]          address % 4 == 0
0x1C   SB         I       byte  [a + sext(imm)] ← b[7:0]
0x1D   SH         I       half  [a + sext(imm)] ← b[15:0]    address must be even
0x1E   SW         I       word  [a + sext(imm)] ← b          address % 4 == 0
0x1F   BEQ        I       if b == c: pc ← target (delay slot still executes)
0x20   BNE        I       if b != c: pc ← target
0x21   BLT        I       if signed b <  signed c: pc ← target
0x22   BGE        I       if signed b >= signed c: pc ← target
0x23   BLTU       I       if unsigned b <  unsigned c: pc ← target
0x24   BGEU       I       if unsigned b >= unsigned c: pc ← target
0x25   J          J       pc ← tgt << 2
0x26   JAL        J       r14 ← pc + 8; pc ← tgt << 2
0x27   JR         I       pc ← b
0x28   JALR       I       a ← pc + 8; pc ← b
0x29   SYSCALL    I       exception, cause 3 (imm must assemble to 0)
0x2A   HALT       I       stop the CPU immediately (see below)
other             —       reserved instruction → exception, cause 1
```

### 2.4 Execution rules

```text
Delay slots        EVERY branch, J, JAL, JR, JALR has ONE delay slot: the instruction
                   at pc+4 executes BEFORE the transfer takes effect, whether or not
                   the branch is taken. JAL/JALR link value = pc + 8 (past the slot).
Branch in a slot   a branch/jump in a delay slot is a reserved-instruction
                   exception (cause 1).
Load delay         NONE — results are interlocked; usable by the next instruction.
Cycles             every instruction costs 1 cycle, EXCEPT:
                     taken branch/jump (incl. its slot scheduling): 2 cycles total
                     exception or interrupt entry: 3 cycles
                     RTE: 3 cycles
                     GPU command writes: see §4.1 (stall the CPU)
                     DMA-active periods: CPU stalled, see §8
HALT               retires like a normal instruction (emits its trace line), then the
                   CPU stops permanently. Further --cycles are ignored. Exit code 0.
```

### 2.5 System registers

Accessed only via `SRD`/`SSRV`:

```text
num  name      access  meaning
0    EPC       rw      pc associated with the last exception/interrupt (see §7)
1    CAUSE     ro      exception/IRQ code (§7); writes ignored
2    CYC_LO    ro      global cycle counter, bits 31:0
3    CYC_HI    ro      global cycle counter, bits 63:32
4    EFLAGS    rw      bit 0 = GIE. After reset GIE = 1.
5    ESTSAV    rw      shadow of EFLAGS saved by hardware on entry, restored by RTE
```

Writes to read-only registers are ignored. `SSRV` cannot set undefined EFLAGS bits.

### 2.6 Pseudo-instructions (assembler conveniences, expand as shown)

```text
NOP          → ADD r0, r0, r0 (word 0x04000000); word 0x00000000 is also a NOP
MOV rd, rs   → ADD rd, rs, r0
LI rd, imm32 → LUI rd, imm32>>14 ; ORI rd, rd, imm32 & 0x3FFF
NEG rd, rs   → SUB rd, r0, rs
B label      → BEQ r0, r0, label
RET          → JR r14
```

---

## 3. Memory Map

```text
Base         Limit       Size      Region
0x00000000   0x001FFFFF  2 MiB     RAM (mirrors: NONE — nothing mirrors)
0x00200000   0x002FFFFF  1 MiB     Cartridge ROM window (PRG payload; see §9)
0x01000000   0x010000FF  256 B     GPU registers          (§4.1)
0x01100000   0x011000FF  256 B     DMA registers          (§8)
0x01200000   0x012000FF  256 B     Timer registers        (§4.3)
0x01300000   0x013000FF  256 B     INTC registers         (§4.4)
0x01400000   0x014000FF  256 B     Audio registers        (§6)
0x01500000   0x015000FF  256 B     Gamepad registers      (§4.5)
0x01600000   0x016000FF  256 B     Control registers      (below)
```

The framebuffer lives in RAM (unified memory — there is no private VRAM):

```text
FRAMEBUFFER_BASE = 0x001DA800
FRAMEBUFFER_SIZE = 320 × 240 × 2 = 153,600 bytes = 0x25800
                 (occupies 0x001DA800 .. 0x001FFFFF, the top of RAM)
Format           16 bpp RGB565, little-endian halfwords, tightly packed,
                 row stride 640 bytes. Pixel (x, y) at FB + 2*(320*y + x).
```

Fixed RAM addresses used by fixtures:

```text
0x00000080   exception vector base (§7)
0x00001000   default stack region grows DOWN from the header-supplied initial sp
```

Access rules:

```text
Unaligned LH/SH      address must be even, else address-error exception (cause 2)
Unaligned LW/SW      address must be %4 == 0, else address-error exception (cause 2)
Byte accesses        never misaligned
Unmapped address     reads return 0xFFFFFFFF, writes are silently dropped (no trap)
ROM window past      reads return 0xFF (e.g. smaller cartridge in the 1 MiB window)
loaded image size
Device registers     byte/half accesses to device registers behave as the aligned
                     word access at address & ~3 (little-endian lane)
Fetches              always word-aligned; a misaligned pc is address-error (cause 2)
```

Control registers:

```text
0x01600000  ID         ro  0x4D433332 ("MC32")
0x01600004  FRAME      ro  completed-frame counter (increments at each frame rollover)
0x01600008  CYCLES_LO  ro  global cycle counter, low word
0x0160000C  CYCLES_HI  ro  global cycle counter, high word
```

---

## 4. Register Maps

### 4.1 GPU (base 0x01000000)

```text
Offset  Name     R/W  Meaning
0x00    STATUS   ro   bit0 BUSY   1 while a command is executing (during its stall)
                      bit1 VBLANK 1 during cycles 345000..349999 of the frame
                      bits15:2 reserved, read 0
0x04    CTRL     rw   reserved; reads 0, writes ignored (this revision)
0x08    CMD      w    command trigger; executes IMMEDIATELY on write (see costs below)
0x0C    P0       rw   parameter 0
0x10    P1       rw   parameter 1
0x14    P2       rw   parameter 2
0x18    P3       rw   parameter 3
```

Commands (value written to CMD; rendering rules in §5):

```text
0  NONE       no-op, 0 cycles
1  TILEMAP    P0 = tilemap base (byte address in RAM, 2400 bytes: 40×30 u16 entries)
              P1 = tile data base (256 tiles × 32 bytes = 8192 bytes)
              P2 = [15:0] scroll_x  [31:16] scroll_y   (pixel units, wrapping)
              P3 = palette base (512 bytes: 16 palettes × 16 RGB565 colors)
              cost: 1200 cycles (one per tile)
2  TRIANGLE   P0 = (v0y << 16) | (v0x & 0xFFFF)   coordinates are SIGNED 16-bit
              P1 = (v1y << 16) | (v1x & 0xFFFF)
              P2 = (v2y << 16) | (v2x & 0xFFFF)
              P3 = [15:0] RGB565 color, [31:16] must be 0
              cost: 1 cycle per framebuffer pixel covered (after clipping)
3  FILLRECT   P0 = (dst_y << 16) | (dst_x & 0xFFFF)   dst_x, dst_y unsigned
              P1 = (height << 16) | (width & 0xFFFF)  width, height unsigned
              P2 = RGB565 fill color
              P3 = must be 0
              cost: 1 cycle per framebuffer pixel written (after clipping)
```

The write to CMD itself stalls the CPU for the command cost; the write retires only
after the last affected pixel is committed. Parameters are latched from P0..P3 at the
moment CMD is written.

### 4.2 Cycle-event ordering (normative)

At each cycle boundary N (the counter having just become N), process in this order:

```text
1. timers decrement according to their prescalers (§4.3)
2. audio mixer tick if N % 420 == 0 (§6)
3. video: VBLANK set at N % 350000 == 345000; VBLANK cleared and FRAME incremented
   (frame rollover, frame hash captured) at N % 350000 == 0
4. DMA engine consumes its units for this cycle (§8)
5. interrupt sample: if GIE=1 and an enabled IRQ is pending, take the highest-priority
   one BEFORE fetching the next instruction (§7)
```

### 4.3 Timers (base 0x01200000; two timers, stride 0x10)

```text
Offset  Name     R/W  Meaning
+0x00   RELOAD   rw   32-bit reload value
+0x04   CURRENT  ro   32-bit down-counter
+0x08   CTRL     rw   bit0 ENABLE   bit[9:8] PRESCALE: 0=÷1 1=÷4 2=÷16 3=÷64
+0x0C   INT      rw1c bit0 pending; write 1 to clear
```

When enabled, CURRENT decrements once per prescaled CPU clock. On the decrement that
takes CURRENT from 0, CURRENT ← RELOAD and INT.bit0 is set (sticky). Timer INT flags
OR onto IRQ line 1 (code 9). After reset: RELOAD=0, CURRENT=0, CTRL=0.

### 4.4 INTC (base 0x01300000)

```text
Offset  Name     R/W  Meaning
0x00    ENABLE   rw   bit n enables IRQ line n
0x04    STATUS   ro   bit n = pending raw line n (mirror of device stickies)
0x08    ACK      w    write 1 to bit n to clear line n's device sticky flag(s)
```

Line/bit assignment (bit number = line, code = 8 + line):

```text
bit 0  code 8   VBLANK    (edge at cycle 345000 of each frame)
bit 1  code 9   TIMER     (either timer INT flag)
bit 2  code 10  DMA       (channel completion with TRIG_IRQ, §8)
bit 3  code 11  AUDIO     (channel finished, or DMA ch3 completion)
bit 4  code 12  GAMEPAD   (any button press edge detected at frame rollover, §4.5)
```

Lines are level-of-sticky-flag: a line is pending while any contributing device sticky
flag is set. Clearing happens ONLY via the device's own rw1c register or via INTC.ACK.
If a condition persists (e.g. VBLANK window spanning multiple samples) the line does not
re-fire until it was cleared.

### 4.5 Gamepad (base 0x01500000)

```text
Offset  Name      R/W  Meaning
0x00    BUTTONS   ro   live state, updated at every frame rollover:
                       bit0 UP  bit1 DOWN  bit2 LEFT  bit3 RIGHT
                       bit4 A   bit5 B     bit6 START bit7 SELECT
                       pressed = 1. Bits 15:8 read 0.
0x04    LATCH     ro   snapshot of the PREVIOUS frame's BUTTONS (latched at rollover)
```

IRQ line 4 fires at a frame rollover where `(BUTTONS & ~LATCH) != 0` (a new press).

In `--headless` mode button input comes from `--input-file`: line N (0-based, parsed as
hex, low 8 bits significant) is the button mask held during frame N; missing lines mean
0. Without `--input-file` no buttons are ever pressed.

---

## 5. Graphics

### 5.1 Tilemap layer (CMD 1)

Screen is 320×240 = 40×30 tiles of 8×8 pixels. Rendering replaces the framebuffer
region covered by each opaque pixel (color index 0 is TRANSPARENT and leaves the
previous framebuffer content untouched).

Tilemap entry (u16 little-endian):

```text
[7:0]   tile index 0..255
[11:8]  palette select 0..15
[14]    hflip
[15]    vflip
[13:12] must be 0
```

Tile pixel fetch for tile t, in-tile column cx (0..7), row cy (0..7):

```text
byte_addr = tile_base + t*32 + cy*8 + cx/2
nibble    = (cx even) ? (RAM[byte_addr] & 0xF) : (RAM[byte_addr] >> 4)
color     = palette[(palette_sel*16 + nibble) * 2]  (u16 LE RGB565 at palette_base)
```

Scrolling: screen pixel (sx, sy) displays the map/tile pixel at

```text
gx = (sx + scroll_x) mod 320      gy = (sy + scroll_y) mod 240
map entry = map[((gy/8)*40) + (gx/8)]     (row-major, 40 entries per row)
in-tile   = (gx%8, gy%8), flipped by the entry's hflip/vflip bits
```

Iteration order is row-major (sy 0..239 outer, sx 0..319 inner) — matters only for
overlapping opaque pixels, which resolve last-writer-wins.

### 5.2 Triangle rasterization (CMD 2)

Vertices are integer screen coordinates (signed 16-bit unpacked from P0..P2). Pixels
outside `[0,319] × [0,239]` are clipped (per-pixel discard; no scissor registers).

Candidate pixels: every (x, y) in the bounding box of the triangle, intersected with the
screen, iterated y-min→y-max outer, x-min→x-max inner. Pixel center is `(x+0.5, y+0.5)`.

Edge function (all-integer; multiply through by 2 to avoid fractions):

```text
E(a, b; x, y) = (2*x + 1 − 2*a.x)*(b.y − a.y) − (2*y + 1 − 2*a.y)*(b.x − a.x)
```

Normalization: compute `W = (v1.x−v0.x)*(v2.y−v0.y) − (v1.y−v0.y)*(v2.x−v0.x)`.
If `W < 0`, swap v1 and v2. If `W == 0`, the triangle is degenerate: draw NOTHING.

After normalization, a pixel is INSIDE iff for all three edges (v0,v1), (v1,v2), (v2,v0):

```text
E < 0,  or  E == 0 and the edge is a top-left edge
```

with the top-left rule evaluated on the (possibly swapped) vertices:

```text
top  edge:  b.y == a.y and b.x > a.x
left edge:  b.x == a.x and b.y < a.y
```

(Verified anchor: triangle (0,0),(3,0),(0,3) has W=+9 > 0, no swap; interior pixels have
E < 0 on all edges; pixel (0,0) is drawn — its center lies exactly on both top/left
edges.)

Every inside pixel writes P3's RGB565 color to the framebuffer.

### 5.3 Fill rectangle (CMD 3)

Writes P2's color to every pixel with `x ∈ [dst_x, dst_x+width)` and
`y ∈ [dst_y, dst_y+height)`, clipped to the screen, same iteration order as §5.2.

### 5.4 Frame hashing

At every frame rollover the runner computes FNV-1a-64 (constants in §9.2) over the
153,600 framebuffer bytes in ascending address order and, when `--hash-frame FILE` was
given, finally writes one line `fnv64=<16 lowercase hex digits>\n` (hash of the LAST
completed frame) to FILE.

---

## 6. Audio

Four channels, streamed from RAM. Base 0x01400000, channel stride 0x10:

```text
Offset  Name    R/W  Meaning
+0x00   RATE    rw   playback step per mix tick as Q16.8 in bits [23:0]:
                     bits[23:16] integer part, bits[15:0] fraction
                     (0x010000 = 1.0 = native rate, 0x008000 = half speed)
                     bit24 LOOP     wrap to sample 0 at end instead of stopping
                     bit25 ACTIVE   1 = channel plays (setting it resets position to 0)
                     bits31:26 reserved, must be 0
+0x40   INT     rw1c bit n = channel n finished (one-shot end); write 1 to clear
+0x48   MASTER  rw   master gain, 0..256; values > 256 clamp to 256.
                     256 = unity. Applied once to the summed mix (below).
```

Sample format: **signed 16-bit little-endian mono**, arrays in RAM.

Per-channel playback state: `pos` in Q16.16. On `ACTIVE` 0→1: `pos = 0`. At every mix
tick (every 420 cycles, §4.2), for each ACTIVE channel:

```text
i = pos >> 16
sample = sext16( RAM16[WADDR + 2*i] )        (i < LEN guaranteed below)
pos += (RATE[23:0] << 8)                    (keeps pos in Q16.16)
if (pos >> 16) >= LEN:
    if LOOP:  pos = 0
    else:     ACTIVE = 0; INT.bit(ch) = 1        (sticky → IRQ line 3)
```

Mixing, every tick, in channel order 0→3:

acc   = Σ_active ( sample_ch * VOL_ch ) >> 8     (arithmetic shift, per term)
out   = clamp_s16( (acc * MASTER) >> 8 )
```

The runner appends every mixed `out` (including silent ticks, as little-endian s16) to
an internal stream: exactly 50,000 samples per second of emulated time. `--hash-audio
FILE` computes FNV-1a-64 over the entire stream produced by the run and writes
`fnv64=<16 hex digits>\n`. DMA channel 3 completion also raises IRQ line 3 (§8).

After reset all channels are inactive (RATE=0, VOL=0, WADDR=0, LEN=0) and MASTER=256.

---

## 7. Interrupts and Exceptions

### 7.1 Vectors

All exceptions and IRQs dispatch to a fixed-address table at the bottom of RAM:

```text
vector address = 0x00000080 + 4 * code
```

```text
Code  Kind                Trigger
1     ReservedInstruction undefined opcode, branch in delay slot, malformed SYS
2     AddressError        misaligned LH/SH/LW/SW or fetch
3     SYSCALL             SYSCALL instruction
8     IRQ_VBLANK          INTC line 0
9     IRQ_TIMER           INTC line 1
10    IRQ_DMA             INTC line 2
11    IRQ_AUDIO           INTC line 3
12    IRQ_GAMEPAD         INTC line 4
```

There is no reset vector: reset jumps straight to the cartridge entry pc (§9.3).

### 7.2 Entry sequence (hardware)

```text
ESTSAV ← EFLAGS ; EFLAGS.GIE ← 0
EPC   ← pc of the instruction that triggered the exception (sync causes 1–3)
        pc of the NOT-YET-EXECUTED next instruction      (async IRQs)
CAUSE ← code
pc    ← 0x80 + 4*code
cost  3 cycles
```

Sync-exception handlers that want to resume AFTER the faulting instruction must bump EPC
by 4 themselves:

```asm
srd   r1, EPC        ; sysreg 0
addi  r1, r1, 4
ssrv  EPC, r1        ; sysreg index in r/a field
rte
```

Async IRQ handlers must NOT bump EPC. IRQ priority when several lines are pending at one
sample point: **lowest code first** (VBLANK > TIMER > DMA > AUDIO > GAMEPAD). IRQs are
sampled only where §4.2 step 5 allows — never mid-stall; a pending IRQ is delivered
immediately after a stall ends, before the next instruction.

### 7.3 RTE

```text
EFLAGS ← ESTSAV ; pc ← EPC ; cost 3 cycles
```

These rules make interrupt latency cycle-deterministic: an IRQ raised at cycle N is
taken before the first instruction that would otherwise retire at cycle > N with GIE=1,
plus the fixed 3-cycle entry cost.

---

## 8. DMA Controller

Base 0x01100000. Four channels, stride 0x10:

```text
ch0  FILL       fill RAM halfwords with a pattern
ch1  COPY       RAM → RAM
ch2  CARTCOPY   cartridge PRG → RAM
ch3  AUDIOFEED  RAM → RAM (identical mechanics to ch1; its completion ALSO raises
                the audio IRQ line, code 11 — used to stage wave data then get told)
```

Per-channel registers:

```text
Offset  Name    R/W  Meaning
+0x00   SRC     rw   byte address (ch2: byte offset into the PRG payload, 0 = first PRG byte)
+0x04   DST     rw   byte address in RAM
+0x08   LEN     rw   length in BYTES
+0x0C   CTRL    rw   write triggers the transfer when bit0 START = 1 (START self-clears)
                     bit1 TRIG_IRQ  raise DMA IRQ line on completion
                     bit2 HALF      move halfwords instead of words (ch1/ch3)
                     bit5 ERR       ro, set on a misaligned configuration; channel
                                    aborts silently, no IRQ
```

Global registers:

```text
0x40    STATUS  ro   bit n = channel n busy
0x44    INT     rw1c bit n = channel n completed with TRIG_IRQ; write 1 to clear
```

Semantics:

```text
Alignment   word mode: SRC, DST, LEN % 4 == 0. half mode (and always ch0): %2 == 0.
            Violation → CTRL.ERR, no transfer.
ch0 FILL    writes the halfword pattern from SRC[15:0] to DST .. DST+LEN
ch1/ch3     copies SRC → DST, forward, unit by unit (overlapping forward regions
            therefore smear — that IS the specified behavior)
ch2         reads PRG[SRC + k]; past end-of-image reads 0xFF
Timing      2 cycles per 4-byte unit; a partial trailing unit also costs 2 cycles.
            Total stall = 2 * ceil(LEN / 4) cycles, during which the CPU cannot
            retire instructions (cycle counter still advances; §4.2 events continue).
Ordering    channels run in channel order; a START written while a channel is busy is
            ignored (check STATUS first).
Completion  INT.bit(n) set if TRIG_IRQ was set; DMA line (code 10) pulses the INTC.
```

---

## 9. Cartridge Format

### 9.1 Header (64 bytes, little-endian fields)

```text
Offset  Size  Field
0x00    4     magic "MC32" (0x4D, 0x43, 0x33, 0x32)
0x04    2     version, u16, must be 1
0x06    2     flags, u16, must be 0
0x08    4     entry, u32 — initial pc at reset
0x0C    4     initial_sp, u32 — loaded into r15 at reset
0x10    32    title, ASCII, NUL-padded (informational)
0x30    4     prg_size, u32 — bytes of PRG payload following the header
0x34    4     checksum, u32 — FNV-1a-32 over header[0x00..0x33] then all PRG bytes
0x38    8     reserved, must be zero
0x40    ...   PRG payload, prg_size bytes
```

### 9.2 Checksum algorithms (exact)

```text
FNV-1a-32:  h = 0x811C9DC5
            for each byte b in sequence:  h = (h XOR b) * 0x01000193  (mod 2^32)
FNV-1a-64:  h = 0xCBF29CE484222325
            for each byte b in sequence:  h = (h XOR b) * 0x100000001B3 (mod 2^64)
```

The cartridge checksum sequence is header bytes 0x00..0x33 (the checksum field itself
excluded), followed by the PRG payload bytes in order. The stored value at 0x34 is
little-endian.

### 9.3 Mapping and reset

```text
PRG byte k maps to address 0x00200000 + k (header is NOT mapped into the window).
Loader rejects (exit code 2) any cart with wrong magic/version/flags or a checksum
mismatch.
Reset state: RAM = 0, r0..r14 = 0, r15 = initial_sp, EFLAGS = 1 (GIE on), CAUSE = 0,
all device registers at their reset values, FRAME = 0, global cycles = 0,
pc = entry. Execution begins at cycle 0 with the fetch of the entry instruction.
```

---

## 9b. Architecture Deliverable (required, before M1)

Before touching code, write `ARCHITECTURE.md` in your submission root. It
must specify, from the spec alone:

```text
CPU state model (registers, delay-slot policy, exception entry)
memory map + bus ownership (which device decodes what; open-bus policy)
device boundaries (GPU/DMA/APU/controller responsibilities and interfaces)
scheduler model (master clock, event ordering, stall accounting)
interrupt routing (vector table, masking, EPC/CAUSE/RTE discipline)
serialization ownership (what state belongs to which device)
test strategy (which milestone fixtures exercise which design decisions)
```

Grading reviews the design note for internal consistency with the code —
architecture is part of the exam, not an optional extra.

## 9c. First-Divergence Debugging Report (required, by M8)

The hidden suite includes at least one subtly faulty fixture. Your
submission must contain `DEBUG-REPORT.md` documenting your investigation
of it, in the same five-field contract every 90_debug chapter teaches:

```text
symptom               what you observed (failing case, wrong golden)
first divergent event the earliest trace line / frame byte / event where
                      your output leaves the expected sequence
root cause            the mechanism, not the symptom
fix                   what changed, and why it cannot regress
regression test       the test that now guards this behavior
```

A submission whose first-divergence section is missing or hand-wavy does
not graduate, regardless of hidden-case count.

## 10. Milestones

Ordered bring-up. Each milestone names its public fixture (under
`tests/public/final_challenge/`) and the acceptance artifact you can check locally.
Goldens are generated by the reference implementation; never trust hand-computed hashes.

```text
M1  CPU smoke        roms/cpu_smoke.mc32
    Run: --rom ... --headless --cycles 20000 --trace smoke.log
    Accept: exit 0; smoke.log byte-equal to traces/smoke.golden.log
    (proves fetch/decode/execute, delay slots, HALT, trace format)

M2  Bus & memory map roms/bus_probe.mc32
    Run: --cycles 50000 --trace bus.log
    Accept: exit 0; trace equals traces/bus.golden.log
    (proves every device decodes at its §3 address, unaligned/unmapped rules)

M3  GPU tilemap      roms/tile_scene.mc32
    Run: --frames 2 --hash-frame tile.txt
    Accept: tile.txt equals goldens/frame_tile.txt
    (proves tile fetch, palettes, flip, scroll, RGB565 layout, frame hashing)

M4  DMA              roms/dma_copy.mc32
    Run: --cycles 100000 --trace dma.log --hash-frame dma.txt
    Accept: both files equal their goldens
    (proves ch0/ch1/ch2 semantics, stall accounting — the trace pins cycles exactly)

M5  Timer IRQs       roms/timer_irq.mc32
    Run: --cycles 1000000 --trace tim.log
    Accept: exit 0; trace equals traces/timer.golden.log
    (proves prescaler math, vector dispatch, EPC/CAUSE/RTE discipline)

M6  Audio mix        roms/audio_scale.mc32
    Run: --cycles 210000 --hash-audio audio.txt
    Accept: audio.txt equals goldens/audio_mix.txt
    (proves Q16.16 stepping, gain/sum/clamp order, 50 kHz cadence)

M7  Gamepad demo     roms/pad_demo.mc32 + input/pad_demo.txt
    Run: --frames 60 --input-file input/pad_demo.txt --hash-frame pad.txt
    Accept: pad.txt equals goldens/frame_pad.txt
    (proves --input-file mapping, LATCH, press-edge IRQ)

M8  Hidden suite     python3 tools/labs/grade.py --repo . final_challenge
    Accept: every case in tests/hidden/final_challenge/manifest.json passes.
```

Debugging guidance: when a golden mismatches, find the FIRST divergent trace line or the
first differing framebuffer byte, not the last visible symptom. Trace lines are
`pc=%08x op=%08x r1=%08x ... r15=%08x cyc=%d` — one per retired instruction, emitted in
retirement order, with `cyc` = the global cycle counter after the instruction's cost.

---

## 11. Hidden-ROM Contract

Grading runs UNSEEN synthetic cartridge ROMs (course fixtures — never commercial
material) against your binary through the standard runner CLI:

```bash
"$EMU" --rom PATH --headless [--cycles N] [--frames N] \
       [--trace FILE] [--hash-frame FILE] [--hash-audio FILE] [--input-file FILE]
```

Therefore your emulator must be:

```text
Deterministic   no wall-clock time, no RNG (or a hard-fixed seed), no threading that
                affects observable results, no uninitialized-memory reads. Running the
                same ROM twice must produce byte-identical traces and hashes.
Self-contained  no network, no host-specific paths beyond the CLI arguments.
Spec-faithful   unseen ROMs only use what THIS document defines; they will exercise
                edge cases the spec pins down (delay slots, wrap-around scrolls,
                degenerate triangles, IRQ nesting rules, DMA stalls).
Honest          a mismatch must fail loudly, not papered over with special cases.
```

Exit codes: `0` normal termination (HALT or budget exhausted), `2` invalid cartridge,
`3` usage error. Any crash, hang beyond the requested budget, or nondeterministic
re-run is a failure.

---

## 12. Submission Layout

Expected repository layout and build shape:

```text
your-submission/
├── CMakeLists.txt
├── src/
│   ├── main.cpp          # runner CLI entry point
│   ├── cpu.{hpp,cpp}
│   ├── bus.{hpp,cpp}
│   ├── gpu.{hpp,cpp}
│   ├── dma.{hpp,cpp}
│   ├── apu.{hpp,cpp}
│   └── cart.{hpp,cpp}    # loader + checksum
└── build/mc32            # the produced emulator binary (REQUIRED path)
```

```bash
cmake -S . -B build && cmake --build build -j
# produces ./build/mc32
```

How graders invoke you: the capstone grading manifest references your binary through an
environment-variable placeholder —

```json
{"name": "unseen_case_01",
 "binary": "{{env:LABS_CAPSTONE_BIN}}",
 "args": ["--rom", "{{tmp}}/hidden_01.mc32", "--frames", "5",
          "--hash-frame", "{{tmp}}/f.txt"],
 "expect_file_hash": {"file": "{{tmp}}/f.txt", "fnv64": "..."}}
```

The harness builds your repository with the command above, points
`LABS_CAPSTONE_BIN` at your built `build/mc32`, and runs each hidden case from the repo
root with `{{tmp}}` scratch directories. Anything other than the standard CLI flags, the
required binary path, or deterministic output will surface as failing hidden cases.

---

## Appendix A — Worked Example

A tiny but complete `.asm.txt` program, annotated. It issues one FILLRECT command
(30×10 red rectangle at (20,10)), demonstrates delay-slot discipline with JAL and a
polling branch, then halts.

```text
;; demo.asm.txt — FILLRECT + delay-slot discipline
;;
;; ABI used here: r1 pointer, r2..r5 temporaries, r14 ra, r15 sp.

        lui   r1, 0x400          ; r1 = 0x400 << 14 = 0x01000000  (GPU base)
                               ; encoding: op=0x13 a=1 b=0 imm=0x400 →
                               ; (0x13<<26)|(1<<22)|0x400 = 0x4C400400
        lui   r2, 0x28           ; r2 = 0x28 << 14 = 0x000A0000
        ori   r2, r2, 0x14       ; r2 = 0x000A0014 = (y=10)<<16 | x=20
        sw    r2, [r1+0x0C]      ; GPU.P0  = destination corner
        lui   r3, 0x28           ; r3 = 0x000A0000
        ori   r3, r3, 0x1E       ; r3 = 0x000A001E = (h=10)<<16 | w=30
        sw    r3, [r1+0x10]      ; GPU.P1  = size
        lui   r4, 0x3            ; r4 = 0x3 << 14 = 0x0000C000
        ori   r4, r4, 0xF800     ; r4 = 0x0000F800 = pure red, RGB565
        sw    r4, [r1+0x14]      ; GPU.P2  = color
        addi  r5, r0, 3          ; CMD value 3 = FILLRECT
        sw    r5, [r1+0x08]      ; GPU.CMD: 30*10 = 300 pixels → this write STALLS
                                 ; exactly 300 cycles before it retires
        jal   wait_vblank        ; r14 = pc+8 (skips the delay slot); pc → wait_vblank
                                 ; *** DELAY SLOT *** executes BEFORE wait_vblank,
                                 ; so r10 is already 1 on the handler's first
                                 ; instruction; a naive no-delay-slot core would
                                 ; only set it after the call returned
wait_vblank:
        lw    r6, [r1+0x00]      ; r6 = GPU.STATUS
        andi  r6, r6, 0x2        ; isolate VBLANK (bit1)
        beq   r6, r0, -3         ; still scanning? rewind 3 instructions to lw.
                                 ; offset: target = pc+4 + (-3<<2) = pc - 8 = lw
        sw    r0, [r1+0x08]      ; delay slot of the taken branch runs EVERY trip
                                 ; through the loop (CMD=0, harmless); on the final
                                 ; fall-through it runs too — one store either way
        halt                     ; stop; exit code 0
```

Observable effects (all derivable from this spec — verify your core reproduces them):

```text
1. Encoding check   the first instruction assembles to
                    (0x13<<26)|(1<<22)|(0<<18)|0x400 = 0x4C400400.
2. Delay slot       in the trace, the `addi r10,r0,1` line (pc = jal_pc+4) appears
                    BETWEEN the jal line and the first `lw` line, and r10 is already
                    1 inside wait_vblank.
3. Stall            the `sw r5,[r1+0x08]` (FILLRECT) retires 300 cycles after the
                    preceding store; the following jal's `cyc` reflects that.
4. Polling loop     the loop trips until cycle N where N % 350000 == 345000 (VBLANK
                    rises); each trip costs 1+1+2 cycles (lw, andi, taken beq) plus
                    the slot's store, i.e. 5 cycles per iteration.
5. Frame hash       with a blank framebuffer the rectangle contributes 300 red
                    (0xF800) RGB565 pixels starting at FB + 2*(320*10 + 20);
                    --hash-frame therefore differs from the empty-frame golden.
```
