# Debugging exercise — stale decodes after self-modifying stores (ch37)

## Symptom

`smc.cached_matches_switch_interpreter` fails while every other test in the
suite passes. The failure is nasty because it hides: ordinary programs
(including the benchmark) produce byte-identical dumps between the cached
pipeline and the plain switch interpreter, hit rates look great, and nothing
faults. Only a program that *writes over its own instructions* diverges —
the output log keeps reporting values from the OLD program.

## Reproduce

```bash
LABS=ch37_performance_dynarec/90_debug make skels && make build && \
  ctest --test-dir build -R ch37_90_debug --output-on-failure
```

The failing assertion reports `out.size() == 3` where the switch reference
produced exactly one entry.

## Hints (progressive)

1. This is NOT an execution-semantics bug: `rx8::execute` is shared with
   the (correct) interpreter and never runs differently.
2. Trace the smc fixture by hand: pass 1 executes the OUT at 0x08 and
   caches its decode; later passes should see a DIFFERENT instruction word
   at 0x08.
3. Ask for each step(): "can the decoded instruction I just served be
   stale?" What is the ONLY guest operation that can make it stale, and
   where does the code react to that operation?

## Your task

Fix `decode_cache.hpp`, then write `bug-report.md` in this directory:

```text
bug:            <one line>
root cause:     <why the cache serves stale decodes and which missing
                maintenance lets that happen>
first divergence:<the first executed instruction whose behavior differs
                from the switch interpreter>
fix:            <what you changed>
regression test:<which TEST() now guards it — name the one that failed>
```
