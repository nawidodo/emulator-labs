# 05 — Cartridge runner

`ch16_05_cart_runner` is the chapter's integration surface: it loads a
synthetic cart image, lets `CartridgeController::makeMapper` pick the
strategy from header type $147, and replays an **op script** against the
mapper.

| piece | contract |
|-------|----------|
| `cart.hpp` | self-contained copies of ROM_ONLY/MBC1/MBC3/MBC5/MBC-X behind one `Mapper` interface; @LABS blocks mirror exercises 01-04 and 99 |
| factory | type $00/unknown -> RomOnly, $01-$03 -> Mbc1, $0F/$10/$12/$13 -> Mbc3, $19-$1E -> Mbc5, $BE -> MbcX |
| reads | every byte of physical bank *k* equals *k* (except the bank-0 header hole), so a read identifies its bank |

## Acceptance

`ch16_05_cart_runner_tests` passes: factory dispatch for all five
families, MBC1 bank identity through the factory, MBC3 tick+latch via
bus writes, MBC5 nine-bit select wrapping into a small image, and the
MBC-X spec examples over a real image. `ch16_05_cart_runner --help`
exits 0 (runner CLI shape).

See the chapter README for the full op-script grammar — `W`/`R`/`T`
lines are this chapter's documented CLI extension.
