# ch49 ROM fixtures — provenance

Synthetic programs only (no commercial content). `boot_handshake.bin` is
33 little-endian words hand-assembled from `boot_handshake.asm.txt`
against the mini-ISA encoding documented in
`templates/ch49_ps1_system_scheduling/02_mini_devices/core.hpp`.
Re-assembling the listing reproduces the binary byte-for-byte; the
program's construction is described in
`templates/ch49_ps1_system_scheduling/91_challenge/CHALLENGE.md`.

`scripts/dma_kick.script` is an example device script in the grammar of
`templates/ch49_ps1_system_scheduling/03_boot_runner/SPEC.md`.
