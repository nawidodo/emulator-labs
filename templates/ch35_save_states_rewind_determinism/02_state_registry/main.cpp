#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include "registry.hpp"

namespace {

// Three fictional "GB-style" devices with distinct state.
struct CpuLike {
    uint16_t pc = 0x0100;
    uint8_t a = 0x11;
};
struct PpuLike {
    uint8_t ly = 0;
    uint8_t scx = 7;
};
struct CartRam {
    uint8_t cells[4] = {1, 2, 3, 4};
};

registry::StateRegistry make_registry(CpuLike& cpu, PpuLike& ppu,
                                      CartRam& ram) {
    registry::StateRegistry r;
    r.add("cpu", 3, [&cpu](uint8_t* d) {
        d[0] = uint8_t(cpu.pc);
        d[1] = uint8_t(cpu.pc >> 8);
        d[2] = cpu.a;
    }, [&cpu](const uint8_t* d) {
        cpu.pc = uint16_t(d[0]) | uint16_t(d[1]) << 8;
        cpu.a = d[2];
    });
    r.add("ppu", 2, [&ppu](uint8_t* d) {
        d[0] = ppu.ly;
        d[1] = ppu.scx;
    }, [&ppu](const uint8_t* d) {
        ppu.ly = d[0];
        ppu.scx = d[1];
    });
    r.add("cart", sizeof(ram.cells), [&ram](uint8_t* d) {
        std::memcpy(d, ram.cells, sizeof(ram.cells));
    }, [&ram](const uint8_t* d) {
        std::memcpy(ram.cells, d, sizeof(ram.cells));
    });
    return r;
}

}  // namespace

TEST(registry, aggregate_round_trip) {
    CpuLike cpu{0x0150, 0xAB};
    PpuLike ppu{0x77, 0x05};
    CartRam ram{{9, 8, 7, 6}};
    auto reg = make_registry(cpu, ppu, ram);
    EXPECT_EQ(reg.section_count(), size_t{3});

    std::vector<uint8_t> blob;
    EXPECT_TRUE(reg.save(blob));

    // Fresh devices prove the data came from the blob.
    CpuLike cpu2{};
    PpuLike ppu2{};
    CartRam ram2{};
    auto reg2 = make_registry(cpu2, ppu2, ram2);
    EXPECT_TRUE(reg2.load(blob));
    EXPECT_EQ(cpu2.pc, uint16_t{0x0150});
    EXPECT_EQ(cpu2.a, uint8_t{0xAB});
    EXPECT_EQ(ppu2.ly, uint8_t{0x77});
    EXPECT_EQ(ppu2.scx, uint8_t{0x05});
    EXPECT_EQ(ram2.cells[3], uint8_t{6});
}

TEST(registry, magic_and_version_in_header) {
    CpuLike cpu{};
    PpuLike ppu{};
    CartRam ram{};
    auto reg = make_registry(cpu, ppu, ram);
    std::vector<uint8_t> blob;
    EXPECT_TRUE(reg.save(blob));
    EXPECT_EQ(std::memcmp(blob.data(), "LBST", 4), 0);
    EXPECT_EQ(blob[4], registry::kBlobVersion);

    blob[4] = 42;  // future schema: must be rejected, not guessed
    EXPECT_FALSE(reg.load(blob));
}

TEST(registry, truncated_blob_rejected) {
    CpuLike cpu{}, cpu2{};
    PpuLike ppu{}, ppu2{};
    CartRam ram{}, ram2{};
    auto reg = make_registry(cpu, ppu, ram);
    std::vector<uint8_t> blob;
    EXPECT_TRUE(reg.save(blob));
    blob.pop_back();
    auto reg2 = make_registry(cpu2, ppu2, ram2);
    EXPECT_FALSE(reg2.load(blob));
}

TEST(registry, section_order_is_part_of_schema) {
    CpuLike cpu{};
    PpuLike ppu{};
    CartRam ram{};
    auto reg = make_registry(cpu, ppu, ram);
    std::vector<uint8_t> blob;
    EXPECT_TRUE(reg.save(blob));

    // A reader registered in a DIFFERENT order must reject the blob —
    // silently mis-parsing sections is the classic save-state bug.
    registry::StateRegistry wrong;
    wrong.add("ppu", 2, [](uint8_t*) {}, [](const uint8_t*) {});
    wrong.add("cpu", 3, [](uint8_t*) {}, [](const uint8_t*) {});
    wrong.add("cart", 4, [](uint8_t*) {}, [](const uint8_t*) {});
    EXPECT_FALSE(wrong.load(blob));
}
