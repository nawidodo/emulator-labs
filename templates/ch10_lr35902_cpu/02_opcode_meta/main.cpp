#define LABSTEST_MAIN
#include <cstring>

#include "labstest.hpp"
#include "opcode_meta.hpp"

TEST(meta, regular_load_block) {
    using gb::opcode_info;
    auto b = opcode_info(0x40);
    EXPECT_TRUE(std::strcmp(b.name, "ld b,b") == 0);
    EXPECT_EQ(b.bytes, 1);
    EXPECT_EQ(b.cycles, 4);

    auto hl = opcode_info(0x66);  // ld h,(hl)
    EXPECT_EQ(hl.cycles, 8);
    auto hl2 = opcode_info(0x77);  // ld (hl),a
    EXPECT_EQ(hl2.cycles, 8);
}

TEST(meta, immediate_and_alu_rows) {
    auto ld = gb::opcode_info(0x06);  // ld b,n
    EXPECT_EQ(ld.bytes, 2);
    EXPECT_EQ(ld.cycles, 8);

    auto add = gb::opcode_info(0x80);  // add a,b
    EXPECT_EQ(add.bytes, 1);
    EXPECT_EQ(add.cycles, 4);

    auto addn = gb::opcode_info(0xC6);  // add a,n
    EXPECT_EQ(addn.bytes, 2);
    EXPECT_EQ(addn.cycles, 8);
}

TEST(meta, pair_ops) {
    EXPECT_EQ(gb::opcode_info(0x01).bytes, 3);   // ld bc,nn
    EXPECT_EQ(gb::opcode_info(0x01).cycles, 12);
    EXPECT_EQ(gb::opcode_info(0x09).cycles, 8);  // add hl,bc
    EXPECT_EQ(gb::opcode_info(0x0B).cycles, 8);  // dec bc
    EXPECT_EQ(gb::opcode_info(0x22).name[0], 'l');  // ldi (hl),a
    EXPECT_EQ(gb::opcode_info(0x22).cycles, 8);
}

TEST(meta, conditional_deltas_explicit) {
    EXPECT_EQ(gb::opcode_info(0x18).cycles, 12);          // jr e
    EXPECT_EQ(gb::opcode_info(0x20).cycles, 8);           // jr nz,e not taken
    EXPECT_EQ(gb::opcode_info(0x20).cycles_alt, 4);       // +4 when taken
    EXPECT_EQ(gb::opcode_info(0xC2).cycles, 12);          // jp nz,nn not taken
    EXPECT_EQ(gb::opcode_info(0xC2).cycles_alt, 4);
    EXPECT_EQ(gb::opcode_info(0xC4).cycles_alt, 12);      // call nz,nn taken +12
    EXPECT_EQ(gb::opcode_info(0xC0).cycles_alt, 12);      // ret nz taken +12
    EXPECT_EQ(gb::opcode_info(0xCD).cycles, 24);          // call nn
    EXPECT_EQ(gb::opcode_info(0xC9).cycles, 16);          // ret
}

TEST(meta, io_and_prefix_rows) {
    EXPECT_EQ(gb::opcode_info(0xE0).cycles, 12);  // ldh (n),a
    EXPECT_EQ(gb::opcode_info(0xFA).bytes, 3);    // ld a,(nn)
    EXPECT_EQ(gb::opcode_info(0xCB).bytes, 1);    // prefix byte itself
}

TEST(meta, base_coverage_complete) {
    for (int op = 0; op < 256; ++op) {
        const gb::Instruction& e = gb::opcode_info(uint8_t(op));
        EXPECT_TRUE(e.name != nullptr);
        EXPECT_NE(e.name[0], '?');
        EXPECT_TRUE(e.bytes >= 1 && e.bytes <= 3);
        EXPECT_TRUE(e.cycles >= 4 && e.cycles <= 24);
    }
}

TEST(meta, cb_page_structure) {
    EXPECT_EQ(gb::cb_info(0x00).bytes, 2);   // rlc b
    EXPECT_EQ(gb::cb_info(0x00).cycles, 8);
    EXPECT_EQ(gb::cb_info(0x06).cycles, 16);  // rlc (hl)
    EXPECT_EQ(gb::cb_info(0x46).cycles, 12);  // bit 0,(hl)
    EXPECT_EQ(gb::cb_info(0x40).cycles, 8);   // bit 0,b
    EXPECT_EQ(gb::cb_info(0x86).cycles, 16);  // res 0,(hl)
    EXPECT_EQ(gb::cb_info(0xC6).cycles, 16);  // set 0,(hl)

    for (int op = 0; op < 256; ++op) {
        const gb::Instruction& e = gb::cb_info(uint8_t(op));
        EXPECT_NE(e.name[0], '?');
        EXPECT_EQ(e.bytes, 2);
        EXPECT_TRUE(e.cycles == 8 || e.cycles == 12 || e.cycles == 16);
    }
}
