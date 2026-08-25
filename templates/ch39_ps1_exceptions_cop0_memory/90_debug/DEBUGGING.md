# Debugging Exercise — ch39: two seeded exception-entry bugs

The COP0 entry path "works for direct faults" but two hardware contracts
were broken. Find both, fix them, and file `bug-report.md`.

| # | Site | Hardware truth (R3000A) |
|---|---|---|
| 1 | `EPC` in a delay-slot fault | EPC must record the BRANCH address, not the slot address; CAUSE.BD=1 marks the condition. The slot address is unrecoverable. |
| 2 | SR shadow push | Every exception pushes IEc/KUc into the previous slots and enters kernel mode with IRQs masked. Skipping the push corrupts privilege state on ERET. |

## Symptoms

1. A syscall inside a delay slot double-faults on return: the handler
   ERETs to the slot address, which the CPU never saved (`debug39.delay_slot_epc_points_at_branch`).
2. After any fault, an ERET pops garbage status: user code resumes with
   stale kernel bits or interrupts left dead
   (`debug39.entry_pushes_kernel_shadow`).

## Workflow

```bash
ctest --test-dir build -R ch39_90_debug --output-on-failure
```

Write `bug-report.md` with bug / root cause / first divergence / fix /
regression test for each of the two bugs. Hidden re-check:
`make grade GRADE_TARGETS=ch39_ps1_exceptions_cop0_memory`.
