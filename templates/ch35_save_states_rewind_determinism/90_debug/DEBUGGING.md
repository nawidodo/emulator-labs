# Debugging exercise — save-state field drift (ch35)

## Symptom

`debug_save.round_trip_preserves_everything` fails: after
save → load, the delay timer and sound timer have **swapped values**.
Every other field round-trips fine, so the state hash of a freshly-booted
machine matches — but any program that uses both timers (music + frame
pacing) desyncs the moment it loads a state.

This is the classic *schema drift* failure mode: someone edits the
serializer layout without bumping the version byte, and every previously
written state becomes silently misinterpreted.

## Reproduce

```bash
LABS=ch35_save_states_rewind_determinism/90_debug make skels && \
  make build && ctest --test-dir build -R ch35_90_debug --output-on-failure
```

## Hints (progressive)

1. Diff `state_hash(machine)` before save against after load. Which
   fields differ?
2. The writer's layout and the reader's layout must agree **field for
   field, in order**. Compare them line by line around offset 22–23.
3. What guard SHOULD have caught this automatically even if you never
   spotted it by eye? (Think about what the version byte is for.)

## Your task

Fix `serialize.hpp`, then write `bug-report.md` containing:

```text
bug:        <one line>
root cause: <which two reads drifted, and why the version byte failed to
            protect anyone>
first divergence: <the earliest observable wrong value after load>
fix:        <what you changed>
regression test: <which TEST() now guards it, plus what a real project
            adds (golden state blob hashed in CI)>
```
