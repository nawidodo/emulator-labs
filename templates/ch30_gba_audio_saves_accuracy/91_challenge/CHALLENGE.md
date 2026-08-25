# CHALLENGE — ch30: deterministic audio hashing

Real audio stacks are full of host dependencies: sample rates, float math,
wall-clock timing. This challenge proves our audio path is none of those:
a scripted scene (PSG duty channel with envelope, LFSR noise, two Direct
Sound FIFOs through volume/bias stages) renders 256 PCM frames whose FNV-64
digest is committed as the acceptance constant in `main.cpp`.

Acceptance: `ctest -R ch30_91_challenge` passes — the digest matches AND a
second render is byte-identical.

For the milestone's "pass external suites" goal, the mGBA suite's audio and
save tests gate behind `requires_rom` + `optional` entries in the hidden
manifest (student-supplied ROMs; never committed).
