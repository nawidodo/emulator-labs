#define LABSTEST_MAIN
#include <vector>
#include "labstest.hpp"
#include "snapshot.hpp"

using namespace gba;

TEST(snapshot, roundtrip_preserves_render) {
    PpuMemory m;
    m.wr16(PpuMemory::kIoBase, 0x0483);
    m.wr16(PpuMemory::kPalBase, 0x001F);
    for (int i = 0; i < 64; ++i)
        m.wr16(0x06000000 + u32(i) * 2, u16(0x7FFF - i));
    m.oam[0] = 10;

    std::vector<u8> blob;
    save_snapshot(m, blob);

    PpuMemory back;
    EXPECT_TRUE(load_snapshot(blob.data(), blob.size(), back));
    std::vector<u32> a(kScreenW * kScreenH), b(kScreenW * kScreenH);
    compose_frame(m, a.data());
    compose_frame(back, b.data());
    EXPECT_EQ(fnv64(a.data(), a.size() * 4),
              fnv64(b.data(), b.size() * 4));
}

TEST(snapshot, rejects_bad_magic_and_truncation) {
    PpuMemory m;
    std::vector<u8> blob;
    save_snapshot(m, blob);

    std::vector<u8> bad = blob;
    bad[0] = 'X';
    EXPECT_FALSE(load_snapshot(bad.data(), bad.size(), m));

    EXPECT_FALSE(load_snapshot(blob.data(), blob.size() / 2, m));

    // Wrong declared length field.
    std::vector<u8> wrong_len = blob;
    wrong_len[12] = 0x05;
    EXPECT_FALSE(load_snapshot(wrong_len.data(), wrong_len.size(), m));
}
