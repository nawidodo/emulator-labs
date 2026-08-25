# Debugging exercise — four seeded flag defects

`chip8.hpp` in this directory compiles and runs, but FOUR instruction
families carry realistic, historically-accurate defects:

1. **Shift flag source** (8XY6/8XYE): VF reads the shifted-out bit from the
   wrong operand - symptoms appear only when X != Y and the profiles differ.
2. **SUBN borrow direction** (8XY7): the comparison copies 8XY5's instead of
   mirroring it. Results look right; flags lie.
3. **SNE inversion** (4XNN): skips when equal. Loops take phantom branches.
4. **BCD digit order** (FX33): hundreds and ones trade places. Palindromic
   values (like 252) hide the bug completely - pick test values wisely.

The unit tests in `main.cpp` pinpoint each symptom. The stubs also carry
TODO(n) markers pointing you at the FAMILY (never the line).

## Your deliverable: bug-report.md

For EACH of the four bugs, produce a report using exactly this format
(curriculum section 59 - trace-first debugging discipline):

```markdown
## Bug N: <short name>
- Bug: <what misbehaves, observed>
- Root cause: <the actual defective line/logic and why it is wrong>
- First divergence: <the earliest observable difference; cite the test or a
  trace line pair (golden vs actual)>
- Fix: <minimal change made>
- Regression test: <which TEST() now guards it, or the new one you added>
```

Generate a reference trace with any solution-tree runner and diff against a
skeleton-tree run to practice first-divergence hunting:

    tools/labs/compare_trace.py good.log bad.log

Do NOT rewrite families wholesale - the graded artifact is the precision of
the diagnosis, not the diff size.
