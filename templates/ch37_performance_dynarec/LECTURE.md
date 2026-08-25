# Chapter 37 — Performance and Dynamic Recompilation

An emulator's first job is correctness. Its second job is not being slow.
This chapter is about the second job, done without ever endangering the
first.

We work on **rx8**, a toy 8-register RISC core small enough to hold in your
head but shaped exactly like real guest ISAs: fixed 4-byte encodings,
byte-addressable memory, absolute branch targets, load/store architecture,
and an output port. Every technique below is implemented on rx8 at lab
scale — and every one of them maps 1:1 onto the same techniques in real
emulators.

## Dispatch overhead

The naive interpreter is `fetch → decode → execute` for every single guest
instruction, forever. On real ISAs decoding is the expensive part:
variable-length prefixes, modrm bytes, register tables. Even in rx8, where
decode is trivial, the *dispatch machinery itself* — bounds checks, switch
branches, pc bookkeeping — dominates. Exercise 01 measures this honestly:
the score of a workload is its **executed-instruction count**, never wall
time. Wall time depends on the host; instruction counts are physics.

## Decode caching

If a program runs a loop ten million times, decoding its body ten million
times is waste. A **decode cache** stores decoded instructions keyed by pc:
first execution decodes and inserts, every later execution hits. This is
the cheapest big win in interpreter engineering, and it introduces the
central responsibility of all dynamic translation: **the cache must track
what it caches**. When memory changes under a cached entry, the entry dies.
Exercise 02 builds precise range invalidation; exercise 90 shows exactly
how silently wrong things get when you skip it.

## Computed dispatch

Real interpreters choose between dispatch strategies: a `switch` per
instruction, threaded code (each handler ends with the next fetch),
**computed dispatch** (a table indexed by opcode holding handler addresses,
so dispatch is one indirect jump), or direct-threaded copies of hot traces.
rx8 uses a switch because it compiles to a computed-goto table anyway —
the lecture point is that the *shape* of dispatch decides what the CPU can
predict, and that none of these choices change architectural behavior.

## Basic blocks

Optimizing across instructions requires knowing where control flow can
enter. A **basic block** is a straight-line run with one entry (the top)
and one exit (the terminator: a branch, jump, or halt). Everything after
this point in the chapter is phrased over blocks, not instructions.
Finding them on rx8 is exact: mark leaders (entry, branch targets,
post-terminator addresses), cut blocks at terminators. Real ISAs make this
harder (delay slots, variable lengths) but identical in principle.

## IR

Executing guest semantics directly from decoded instructions still couples
"what the machine does" to "how the machine says it". A tiny
**host-independent IR** — here one `IrInsn` per guest instruction — sits
between: translation lowers guest→IR once, execution consumes IR only.
Host independence means the same correct pipeline runs everywhere; shape
stability gives optimizers something to rewrite. The cost model becomes
**executed IR ops**, still deterministic, still wall-time-free.

## Code cache

Translating eagerly would waste effort on cold paths. A **code cache**
translates lazily per block and remembers translations keyed by entry
address — the block-granularity descendant of the decode cache. Our
`IrEngine` is a code cache: miss = translate now, hit = execute translated
IR, and the cache lives only as long as the bytes it was built from.

## Invalidation and self-modifying code

Here is where dynamic recompilation earns its difficulty reputation. A
store that lands inside translated code makes some translations LIES.
Emulators handle this by invalidating conservatively (flush everything on
any store into code), precisely (drop only entries overlapping the written
bytes), or cleverly (hardware-style page dirty bits). rx8 uses byte-range
precision against word-aligned instructions. Miss the invalidation and
nothing crashes: the machine simply executes the OLD program with total
confidence. Exercise 90 seeds exactly this bug; the SMC fixture proves
sensitivity both ways.

## Dynamic recompilation

Put the pieces together and you have a **dynarec**: guest code → decode →
basic blocks → IR → optimization → execute, with a code cache and
invalidation discipline keeping the pipeline honest. A native JIT —
emitting host machine code instead of interpreting IR — changes only the
back end. It is optional here deliberately: the structure above IS dynamic
recompilation, and every production emulator (RetroArch dynarecs, RPCS3,
Dolphin) is this pipeline with progressively fancier back ends.

## Optimization

The gate pass in exercise 04 runs classic peepholes over the IR:

- identity folding (`add/sub/or` with r0 → mov, xor-self → constant),
- copy propagation through known register equivalences,
- test+jump fusion (`addi rd,rd,i ; bnez rd` → one fused op),
- liveness-driven dead-code elimination.

Reduction is measured on executed op counts and must clear **20%** on the
benchmark while observable dumps stay byte-identical. Note what "observable"
means: OUT log plus memory, NOT registers. That contract is what makes
deleting register-only computation legal — the same reason real emulators
may reorder invisible state as long as visible state matches.

## Study checklist

```text
dispatch overhead        decode caching         computed dispatch
basic blocks             IR                     code cache
dynamic recompilation    invalidation           self-modifying code
```
