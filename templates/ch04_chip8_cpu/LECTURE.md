# Chapter 4 — The Complete CHIP-8 CPU

Reference reading: Tobias Langhoff's "CHIP-8 architecture" guide. Testing
philosophy: curriculum sections 52-59 (trace-first debugging, test pyramid,
disassembler and stepping contracts).

## The instruction families

A CHIP-8 CPU is small enough to hold in your head, and every family below
gets its own exercise:

| Family | Opcodes | Subtlety |
|---|---|---|
| Jumps | 1NNN, BNNN | BNNN adds V0 - the one opcode nobody remembers |
| Calls | 2NNN, 00EE | fetch() already advanced PC; push that address |
| Conditionals | 3XNN, 4XNN, 5XY0, 9XY0 | skipping = one extra PC advance |
| Immediate ALU | 6XNN, 7XNN | 7XNN wraps WITHOUT touching VF |
| Register ALU | 8XY0-8XY7 (+E) | VF rules differ per sub-opcode |
| Shifts | 8XY6, 8XYE | source operand is a QUIRK, not a rule |
| Random | CXNN | VX = RNG byte AND NN |
| Memory | ANNN, FX29, FX33, FX55, FX65 | I-update behavior is a quirk |
| Keypad | EX9E, EXA1 | skip-if-key / skip-if-not-key |

## Flags are a contract, not an afterthought

Three different flag conventions coexist in one ISA:

- `8XY4`: VF = carry **out** (sum did not fit in 8 bits).
- `8XY5` / `8XY7`: VF = **NOT borrow** ("the subtraction fit"). Note that the
  two opcodes subtract in opposite directions but share the semantics.
- `8XY6` / `8XYE`: VF = the bit shifted **out**, captured *before* the shift.

The classic bugs are all here: copying 8XY5's comparison to 8XY7, reading the
shifted-out bit after the shift (always 0), or computing flags from the wrong
operand. The 90_debug exercise seeds exactly these.

## Quirks: divergence as configuration

CHIP-8 ran on different machines with divergent behavior. Rather than
sprinkling `if (old_machine)` through the decoder, this chapter's reference
solution routes every divergence through one struct:

```cpp
struct Chip8Quirks {
    bool shift_uses_vy;       // COSMAC VIP vs CHIP-48 shift source
    bool load_store_leaves_i; // FX55/FX65 I advance
    bool vf_reset;            // FX55/FX65 clobber VF?
    bool wrapping;            // address arithmetic wraps at 0x1000?
};
```

Default profile = COSMAC VIP ("classic"); `--quirks modern` flips to the
CHIP-48/HIP-8 values. Exercise 03 proves both settings; the challenge runs
the whole flags suite under both.

## Trace-first debugging

From this chapter on you never debug by staring at pixels:

```
$ ./runner --rom prog.bin --headless --cycles 300 --trace t.log
pc=0202 op=2208 V0=03 ... cyc=1
```

Compare traces from a known-good run against yours with
`tools/labs/compare_trace.py expected.log yours.log` - it reports the FIRST
divergence, which is where the bug is, not where the symptom shows up.

## Disassembly and stepping are mandatory interfaces

Every CPU in this course exposes:

- `uint16_t step();` - execute exactly one instruction,
- `std::string disassemble(uint16_t addr);` - mnemonic view of memory.

Both exist in this chapter's reference machine (see `06_flags_suite/chip8.hpp`)
because testing infrastructure needs them more than debuggers do.

## Determinism or it did not happen

Randomness (CXNN) is real, but tests never tolerate unseeded RNG. The
reference machine carries an LCG with fixed-seed injection (`seed_rng()`), so
`CXNN` sequences are reproducible byte-for-byte. Every golden file in this
chapter was generated twice and compared.
