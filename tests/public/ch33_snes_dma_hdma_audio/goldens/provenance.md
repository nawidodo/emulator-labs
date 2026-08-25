# ch33 public test assets

Mirrors of chapter fixtures and goldens so external tooling can reach them
without walking templates/. Canonical copies live in
templates/ch33_snes_dma_hdma_audio/; if they ever differ, the template side
wins. See that directory's provenance files for generation details.

- roms/gradient.bin — public S33N challenge bundle (+ format listing)
- goldens/gradient_frame.bin — reference 224-byte effect buffer;
  FNV-1a-64 `17F7C28EF777DE45`,
  SHA256 e573a461519ab5d368e8d865beef05f9c87a5701daab1059ee67ec5937f84db7
- goldens/90_debug_gradient_writes.log — corrected-engine HDMA write log
  for the debug exercise (same values embedded in its tests)
- goldens/gradient_table.bin — the debug exercise's brightness table

All data synthetic (pure arithmetic); no commercial ROM content anywhere.
