# SPEC — 99_coding_test: unseen map "Tetra-8"

You have never seen this console; the spec below (CODING_TEST.md, next
to `coding_map.hpp`) is complete. Implement `t8::Tetra8Map` — the three
`TODO` blocks — against the same `Map` interface shape as the challenge.

Different rules from CourseBoy-II on purpose:

* a **banked ROM window** (register-selected, zero wraps to one) instead
  of a static mirror;
* a **NUL shadow**: dead slots read `$00` ("the data bus idles low"),
  and ROM reads past the image pad with `$00` too — the opposite of the
  challenge's `$FF` open bus.

Acceptance: visible `TEST(t8, ...)` suites green, and the grader-only
`TEST(hidden, ...)` corners pass against the same document.
