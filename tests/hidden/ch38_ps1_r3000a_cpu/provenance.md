# Provenance — ch38 hidden fixtures

## roms/hidden_alu.bin

Hand-assembled from `hidden_alu.asm.txt` (this directory) with a throwaway
MIPS I assembler script. 28 instructions at 0x80010000, halting via
`syscall`. Deliberately overlaps NO public fixture code: exercises slt/sltu
sign vs unsigned compares, sra/xori/nor chains, an swr+swl unaligned store
verified by an lwr+lwl readback, a bne countdown loop through its delay
slot, and divu.

Golden: running the reference solution runner twice with `--cycles 2000
--hash-frame` produced identical `fnv64=17539618F243DEA5` payloads.
