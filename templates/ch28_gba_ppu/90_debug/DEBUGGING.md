# DEBUGGING — ch28: sprites vanish behind backgrounds

## Symptom

Games render mostly fine, but HUD elements and character sprites that share
a priority level with a text background disappear: the background wins and
the sprite never shows. Sprites on *higher-priority-value* (numerically
larger) layers are also missing even though they should punch through.
Players report "the status bar is gone but its shadow is visible."

## Reproducing

`ch28_90_debug_tests` encodes real hardware behavior in three tests. Run
them against the unmodified skeleton — `priority.equal_prio_sprite_wins`
and part of `priority.lower_value_wins` fail.

## Your task

1. Find the first observable divergence (which assertion fails first?).
2. Root-cause it in `debug_scene.hpp`. Exactly one relational operator is
   wrong relative to GBATEK's priority rules.
3. Fix it.
4. Write `bug-report.md` next to this file using the template below.

```markdown
# Bug report
**Bug:** <one sentence>
**Root cause:** <operator / line, and why hardware disagrees>
**First divergence:** <first failing test + expression>
**Fix:** <diff in words>
**Regression test:** <what test now guards against reintroduction>
```

The regression tests are already in `main.cpp`; your job is to make them
pass by fixing the root cause, not by editing the tests.
