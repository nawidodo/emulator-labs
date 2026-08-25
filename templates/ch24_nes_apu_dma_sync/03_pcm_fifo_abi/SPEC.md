# 03 — PCM FIFO ABI (host/audio boundary)

Foundations-track Phase 6 applied to the NES APU: the mixer owns emulated
samples, the host owns playback. The contract between them is a
fixed-width, standard-layout ring with partial-accept back-pressure.

Contract highlights (full details in `pcm_fifo.hpp`):

* `PcmFifoConfig` carries `struct_size` + `abi_version` — a newer host must
  be rejected with `PCM_ERR_SIZE` / `PCM_ERR_VERSION`, never guessed at.
* `pcm_push` accepts **at most** the free space and reports how many landed.
* `pcm_pop` drains in FIFO order across ring wrap; empty pop drains 0.
* Level accounting is exact: pushed − popped == level at all times.
