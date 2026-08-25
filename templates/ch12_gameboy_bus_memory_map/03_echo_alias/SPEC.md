# SPEC — 03_echo_alias

Model echo RAM the way the silicon does: not as a second buffer that
must be kept in sync, but as a *window* onto the same WRAM device
(three `TODO` blocks in `echo_bus.hpp`):

1. `EchoWindow::read` — translate `addr - $2000`, then read the target.
2. `EchoWindow::write` — translate, then write through.
3. `attachEchoWindow` — register E000-FDFF on the bus.

The translation is exact over 0x1E00 bytes: E000 -> C000 and
FDFF -> DDFF are the endpoints. DE00-DFFF have no echo counterpart,
and nothing at or above FE00 is aliased.

Acceptance: `ch12_03_echo_tests` green — both directions transparent
at every sampled pair, DFFF provably untouched by FDFF traffic.
