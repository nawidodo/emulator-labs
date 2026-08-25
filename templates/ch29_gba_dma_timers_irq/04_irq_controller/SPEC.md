# 04_irq_controller — SPEC

Implement the GBA interrupt flow.

1. `raise` — IF latches sources regardless of IE/IME.
2. `acknowledge` — write-1-to-clear.
3. `pending` — IME && (IE & IF).
4. `should_wake_halt` — same condition as delivery.
5. `next_service_bit` — lowest set bit of IE & IF (BIOS dispatch order),
   -1 when idle.

Acceptance: masking, gating, selective acks and dispatch order exact.
