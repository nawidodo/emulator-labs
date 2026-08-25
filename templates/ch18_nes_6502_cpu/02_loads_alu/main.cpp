#define LABSTEST_MAIN
#include "labstest.hpp"

#include "cpu.hpp"

using namespace nes6502;

namespace {

struct Rig {
    FlatRam ram;
    Cpu cpu{.bus = &ram};

    // Run one opcode placed at $0200 with optional operand bytes.
    void exec(std::initializer_list<uint8_t> bytes) {
        cpu.load_program(0x0200, bytes.begin(), bytes.size());
        step(cpu);
    }
};

}  // namespace

TEST(load_store, lda_imm_sets_zn) {
    Rig r;
    r.exec({0xA9, 0x80});
    EXPECT_EQ(r.cpu.a, 0x80);
    EXPECT_TRUE(r.cpu.p & FN);
    EXPECT_FALSE(r.cpu.p & FZ);
    EXPECT_EQ(r.cpu.cycles, 2);
}

TEST(load_store, lda_zero_flag) {
    Rig r;
    r.exec({0xA9, 0x00});
    EXPECT_TRUE(r.cpu.p & FZ);
}

TEST(load_store, sta_abs_writes_memory) {
    Rig r;
    r.cpu.a = 0x42;
    r.exec({0x8D, 0x00, 0x20});
    EXPECT_EQ(r.ram.mem[0x2000], 0x42);
    EXPECT_EQ(r.cpu.cycles, 4);
}

TEST(load_store, sta_absx_bills_penalty_even_without_cross) {
    Rig r;
    r.cpu.a = 0x7F;
    r.exec({0x9D, 0x00, 0x20});  // STA $2000,X with X=0
    EXPECT_EQ(r.ram.mem[0x2000], 0x7F);
    EXPECT_EQ(r.cpu.cycles, 5);  // stores pay the indexed penalty always
}

TEST(load_store, ldx_zpy_wrap_read) {
    Rig r;
    r.cpu.y = 0x90;            // $80,$Y=$90 wraps to page-zero $10
    r.ram.mem[0x0010] = 0xC3;
    r.exec({0xB6, 0x80});
    EXPECT_EQ(r.cpu.x, 0xC3);
    EXPECT_EQ(r.cpu.cycles, 4);
}

TEST(load_store, sta_izy_full_flow) {
    Rig r;
    r.cpu.a = 0x11;
    r.cpu.y = 0x02;
    r.ram.mem[0x40] = 0x50;   // ptr lo
    r.ram.mem[0x41] = 0x30;   // ptr hi -> base $3050 -> target $3052
    r.exec({0x91, 0x40});
    EXPECT_EQ(r.ram.mem[0x3052], 0x11);
    EXPECT_EQ(r.cpu.cycles, 6);
}

TEST(transfers_stack, txs_skips_flags_tsx_sets_them) {
    Rig r;
    r.cpu.x = 0xFF;
    r.exec({0x9A});  // TXS: no Z/N update
    EXPECT_EQ(r.cpu.sp, 0xFF);
    EXPECT_FALSE(r.cpu.p & FN);
    r.exec({0xA9, 0x00});  // clear Z via LDA #0 first
    r.exec({0xBA});        // TSX from sp=$FF sets N
    EXPECT_TRUE(r.cpu.p & FN);
}

TEST(transfers_stack, php_pushes_b_and_u_plp_masks_b) {
    Rig r;
    r.cpu.p = FC | FZ;
    r.exec({0x08});                       // PHP
    EXPECT_EQ(r.ram.mem[0x01FD], FC | FZ | FB | FU);
    r.ram.mem[0x01FD] = FB | FU | FC | FN;  // hostile stacked B bit
    r.exec({0x28});                       // PLP
    EXPECT_EQ(r.cpu.p, FC | FN | FU);     // B stripped, U forced on
    EXPECT_EQ(r.cpu.sp, 0xFD);
}

TEST(logic, eor_ora_and) {
    Rig r;
    r.exec({0xA9, 0b1010});               // LDA #$0A
    r.exec({0x09, 0b0101});               // ORA #$05 -> 1111
    EXPECT_EQ(r.cpu.a, 0x0F);
    r.exec({0x49, 0xFF});                 // EOR #$FF -> $F0
    EXPECT_EQ(r.cpu.a, 0xF0);
    EXPECT_TRUE(r.cpu.p & FN);
    EXPECT_FALSE(r.cpu.p & FZ);
    r.exec({0xA9, 0b1000'1100});
    r.exec({0x29, 0b1111'1010});          // AND -> $88
    EXPECT_EQ(r.cpu.a, 0x88);
    EXPECT_TRUE(r.cpu.p & FN);
}

TEST(logic, bit_copies_operand_n_v_not_result_sign) {
    Rig r;
    r.ram.mem[0x0030] = 0xC0;             // bits 7 and 6 set
    r.cpu.a = 0x0F;                       // A & m == 0 -> Z set
    r.exec({0x2C, 0x30, 0x00});           // BIT abs (BIT zp is ex99)
    EXPECT_TRUE(r.cpu.p & FN);
    EXPECT_TRUE(r.cpu.p & FV);
    EXPECT_TRUE(r.cpu.p & FZ);
    EXPECT_EQ(r.cpu.a, 0x0F);             // BIT leaves A alone
}

TEST(arith, adc_binary_carry_and_overflow) {
    Rig r;
    r.exec({0xA9, 0x50});
    r.exec({0x69, 0x50});                 // $50+$50=$A0, V set, no carry
    EXPECT_EQ(r.cpu.a, 0xA0);
    EXPECT_TRUE(r.cpu.p & FV);
    EXPECT_FALSE(r.cpu.p & FC);
}

TEST(arith, adc_uses_carry_in) {
    Rig r;
    r.cpu.p |= FC;                        // seed carry directly
    r.exec({0xA9, 0x01});
    r.exec({0x69, 0x01});                 // 1+1+C = 3
    EXPECT_EQ(r.cpu.a, 0x03);
    EXPECT_FALSE(r.cpu.p & FC);
}

TEST(arith, adc_carry_out_sets_flag) {
    Rig r;
    r.exec({0xA9, 0xFF});
    r.exec({0x69, 0x01});
    EXPECT_EQ(r.cpu.a, 0x00);
    EXPECT_TRUE(r.cpu.p & FC);
    EXPECT_TRUE(r.cpu.p & FZ);
}

TEST(arith, sbc_is_adc_of_complement) {
    Rig r;
    r.cpu.p |= FC;  // no borrow pending
    r.exec({0xA9, 0x05});
    r.exec({0xE9, 0x03});                 // 5-3 = 2 with carry OUT set
    EXPECT_EQ(r.cpu.a, 0x02);
    EXPECT_TRUE(r.cpu.p & FC);            // no borrow

    Rig r2;
    r2.cpu.p |= FC;
    r2.exec({0xA9, 0x03});
    r2.exec({0xE9, 0x05});                // 3-5 borrows: carry clear
    EXPECT_EQ(r2.cpu.a, 0xFE);
    EXPECT_FALSE(r2.cpu.p & FC);
    EXPECT_TRUE(r2.cpu.p & FN);
}

TEST(cycles, table_base_counts_match_step_totals) {
    // Spot-check official cycle counts straight through step().
    struct Case { std::initializer_list<uint8_t> bytes; int want; };
    const Case cases[] = {
        {{0xA9, 0x00}, 2},           // LDA #
        {{0xAD, 0x00, 0x20}, 4},     // LDA abs
        {{0xBD, 0xFF, 0x20}, 5},     // LDA abs,X crossing (X seeded below)
        {{0x9D, 0x00, 0x20}, 5},     // STA abs,X always-penalty
        {{0xB1, 0x40}, 6},           // LDA (zp),Y crossing
    };
    for (const auto& tc : cases) {
        Rig r;
        r.cpu.x = 2;
        r.cpu.y = 1;
        r.ram.mem[0x40] = 0xFF;
        r.ram.mem[0x41] = 0x20;      // (zp),Y base $20FF -> cross
        r.exec(tc.bytes);
        EXPECT_EQ(r.cpu.cycles, tc.want);
    }
}
