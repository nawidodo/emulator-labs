# 13.04 — Interrupt delivery

Close the loop: timer overflow raises IF; IE + IME dispatch through the
copied ch11 interrupt controller into a real ISR; priority arbitration;
HALT wake rules.

## Contract

* Dispatch priority is IF bit order: VBlank > STAT > Timer > Serial >
  Joypad. One dispatch = clear IF bit, push PC, PC=vector ($40/$48/$50/
  $58/$60), IME=0, 20 cycles.
* EI lands after the following instruction retires (ch11 semantics); RETI
  restores IME immediately.
* HALT wakes when IE & IF != 0. With IME=0 it resumes WITHOUT servicing:
  the IF bit survives, execution continues at the next instruction.
* Machine ordering (pinned by machine.hpp, consumed by the 91 runner):
  execute instruction -> tick divider by its cycles -> service interrupt ->
  tick the divider through the 20 entry cycles too.

## Target

`ch13_04_irq_tests`
