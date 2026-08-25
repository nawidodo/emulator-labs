# ch43_ps1_dma — fixture provenance

All fixtures are synthetic, hand-constructed word arrays; no commercial or
copyrighted data is embedded.

## chains/pub.chain + pub.chain.txt + pub_walk.trace
Linked-list DMA chain of three packets (2/1/3 payload words) ending in the
exact `0FFFFFFh` sentinel header. The `.txt` file is a commented word dump;
`pub_walk.trace` is the golden walker trace produced by the reference
solution:

    build/skels/ch43_ps1_dma/03_linked_list/ch43_03_ll_runner \
        --chain tests/public/ch43_ps1_dma/chains/pub.chain --madr 0x0 \
        --trace pub_walk.trace

Generated twice; byte-identical both runs (determinism check).

## vram/pub.list (+ golden vram/hash)
GPU command list: two FillRect packets through linked-list DMA into the
64x32 RGB15 challenge VRAM. Golden dump `vram/pub_vram.bin`
(FNV-1a 64 = 58E5A86D6484744D) produced by:

    build/skels/ch43_ps1_dma/91_challenge/ch43_91_challenge_runner \
        --list tests/public/ch43_ps1_dma/vram/pub.list --vram-out pub_vram.bin

Run twice on the reference solution — identical outputs.
