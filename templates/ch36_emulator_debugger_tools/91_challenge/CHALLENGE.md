# Challenge — integrated debugger session transcript (ch36)

Build `DebugSession`: a tiny command interpreter over the ch36/01
`CpuDebug` interface, then pin its output as a transcript fixture —
exactly how real emulator debuggers are regression-tested.

## Command set

```text
step <n>       execute n instructions, print one trace line per step
regs           print the register JSON payload
disasm <addr>  disassemble at addr ("disasm pc" uses current PC)
bp <addr>      set a breakpoint; report "bp=<addr> set"
run            step until a breakpoint fires or HALT; print steps taken
watch <addr>   set watchpoint; report hits seen so far when queried
mem <addr>     print "mem[<addr>]=<hex>"
quit           end session
```

Every command echoes one line; every observation comes from the LIVE
machine through the CpuDebug interface — nothing is faked.

## Fixture

`sessions/demo.session` (committed under tests/public/ch36...) drives a
synthetic fx8 program. The acceptance test replays it and requires the
transcript to match byte-for-byte. If you change any observable format,
regenerate the golden and say so in the commit message.

Implement the annotated stubs in `session.hpp`.
