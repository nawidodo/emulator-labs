# Debugging exercise — breakpoint never fires (or fires on the wrong line) (ch36)

## Symptom

`debug_bp.breakpoint_counts_hits` fails. Worse, the failure looks
nondeterministic from the user's chair: a breakpoint set on address $03
sometimes appears to stop execution at BOTH $02 and $03 — and one set on
an even address stops one instruction EARLY. Hit counts are inflated.

Any emulator whose breakpoints round or mask addresses is unusable for
step-level debugging: you cannot trust the one tool that is supposed to
be ground truth.

## Reproduce

```bash
LABS=ch36_emulator_debugger_tools/90_debug make skels && make build && \
  ctest --test-dir build -R ch36_90_debug --output-on-failure
```

## Hints (progressive)

1. Set a breakpoint on an ODD address in a program where pc passes
   through both neighbors; watch which steps report a hit.
2. The comparator is doing something to addresses before comparing.
   Hardware PCs do not get rounded.
3. Ask what `& 0xFE` does to an odd byte.

## Your task

Fix `cpudebug.hpp`, then write `bug-report.md`:

```text
bug:
root cause:
first divergence:  <the first step where check_breakpoints() returns a
                    hit for a breakpoint that should not have fired>
fix:
regression test:   <which TEST() guards it>
```
