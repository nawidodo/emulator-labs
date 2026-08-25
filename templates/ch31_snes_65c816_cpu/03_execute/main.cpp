#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdio>
#include <string>
#include <vector>

#include "exec.hpp"

namespace {

std::vector<uint8_t> read_bytes(const std::string& path) {
    std::vector<uint8_t> data;
    FILE* f = std::fopen(path.c_str(), "rb");
    EXPECT_TRUE(f != nullptr);
    if (f == nullptr) return data;
    uint8_t buf[512];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        data.insert(data.end(), buf, buf + n);
    }
    std::fclose(f);
    return data;
}

std::string read_text(const std::string& path) {
    const auto bytes = read_bytes(path);
    return std::string(bytes.begin(), bytes.end());
}

// Runs rom bytes from $00:$0000 until halt/cycle cap, returning trace text.
std::string run_trace(const std::vector<uint8_t>& rom, uint64_t max_cycles) {
    snescpu::Mem mem;
    snescpu::Cpu cpu;
    mem.load(0x00, 0x0000, rom.data(), rom.size());
    std::string out;
    while (cpu.cycles < max_cycles) {
        const uint16_t pc0 = cpu.pc;
        const uint8_t op = mem.read(cpu.k, cpu.pc);
        const int n = snescpu::step(cpu, mem);
        if (n < 0) break;
        cpu.cycles += static_cast<uint64_t>(n);
        out += snescpu::trace_line(cpu, pc0, op);
        out += '\n';
    }
    return out;
}

}  // namespace

TEST(exec, halt_on_brk) {
    snescpu::Mem m;
    snescpu::Cpu c;
    const uint8_t prog[] = {0xEA, 0x00, 0xEA};
    m.load(0, 0, prog, sizeof(prog));
    const int n_nop = snescpu::step(c, m);
    EXPECT_EQ(n_nop, 2);                 // NOP
    const int n_brk = snescpu::step(c, m);
    EXPECT_EQ(n_brk, -1);                // BRK halts
    EXPECT_EQ(c.pc, 2);                  // PC stopped past the BRK byte
}

TEST(exec, immediate_length_follows_width) {
    snescpu::Mem m;
    // REP #$30 ; LDA #$1234 ; BRK
    const uint8_t wide[] = {0xC2, 0x30, 0xA9, 0x34, 0x12, 0x00};
    m.load(0, 0, wide, sizeof(wide));
    snescpu::Cpu c;
    c.e = false;
    const int n_rep = snescpu::step(c, m);
    EXPECT_EQ(n_rep, 3);
    EXPECT_FALSE(snescpu::a_is_8bit(c));
    const int n_lda = snescpu::step(c, m);
    EXPECT_EQ(n_lda, 2);
    EXPECT_EQ(c.a, 0x1234);
    EXPECT_EQ(c.pc, 5);  // consumed a 16-bit immediate

    // SEP #$20 ; LDA #$78 ; BRK -> one immediate byte only
    const uint8_t narrow[] = {0xE2, 0x20, 0xA9, 0x78, 0x00};
    m.load(0, 0, narrow, sizeof(narrow));
    snescpu::Cpu c2;
    c2.e = false;  // keep native so REP/SEP behave predictably
    c2.p |= snescpu::FM;
    const int n_sep = snescpu::step(c2, m);
    EXPECT_EQ(n_sep, 3);
    EXPECT_TRUE(snescpu::a_is_8bit(c2));
    c2.a = 0xABCD;
    const int n_imm = snescpu::step(c2, m);
    EXPECT_EQ(n_imm, 2);
    EXPECT_EQ(c2.a, 0xAB78);  // high byte preserved by an 8-bit load
    EXPECT_EQ(c2.pc, 4);
}

TEST(exec, lda_abs_uses_db_bank) {
    snescpu::Mem m;
    snescpu::Cpu c;
    c.e = false;
    c.p &= uint8_t(~snescpu::FM);       // widen A
    c.db = 0x7E;
    m.write(0x7E, 0x0010, 0xCD);
    m.write(0x7E, 0x0011, 0xAB);
    const uint8_t prog[] = {0xAD, 0x10, 0x00};  // LDA $0010
    m.load(0, 0, prog, sizeof(prog));
    const int n = snescpu::step(c, m);
    EXPECT_EQ(n, 4);
    EXPECT_EQ(c.a, 0xABCD);
}

TEST(exec, sta_long_targets_explicit_bank) {
    snescpu::Mem m;
    snescpu::Cpu c;
    c.e = false;
    c.p &= uint8_t(~snescpu::FM);
    c.a = 0xBEEF;
    c.db = 0x99;  // must be ignored by long addressing
    const uint8_t prog[] = {0x8F, 0x00, 0x20, 0x00};  // STA $002000
    m.load(0, 0, prog, sizeof(prog));
    const int n_sta = snescpu::step(c, m);
    EXPECT_EQ(n_sta, 5);
    EXPECT_EQ(m.read(0x00, 0x2000), 0xEF);
    EXPECT_EQ(m.read(0x00, 0x2001), 0xBE);
}

TEST(exec, absx_charges_page_cross_cycle) {
    snescpu::Mem m;
    snescpu::Cpu c;
    c.e = false;
    c.p &= uint8_t(~(snescpu::FM | snescpu::FX));
    c.x = 0x00FF;
    const uint8_t prog[] = {0xBD, 0x90, 0x01};  // LDA $0190,X
    m.load(0, 0, prog, sizeof(prog));
    const int n_cross = snescpu::step(c, m);
    EXPECT_EQ(n_cross, 5);  // $0190+$00FF crosses into page $02

    snescpu::Cpu c2 = c;
    c2.pc = 0;
    c2.x = 0x0001;  // stays inside page $01: no penalty
    const int n_flat = snescpu::step(c2, m);
    EXPECT_EQ(n_flat, 4);
}

TEST(exec, jml_switches_program_bank) {
    snescpu::Mem m;
    snescpu::Cpu c;
    m.write(0x01, 0x8000, 0xEA);
    const uint8_t prog[] = {0x5C, 0x00, 0x80, 0x01};  // JML $018000
    m.load(0, 0, prog, sizeof(prog));
    const int n_jml = snescpu::step(c, m);
    EXPECT_EQ(n_jml, 4);
    EXPECT_EQ(c.k, 0x01);
    EXPECT_EQ(c.pc, 0x8000);
}

TEST(trace, line_shape) {
    snescpu::Cpu c;
    c.pc = 0x0100;
    const std::string line = snescpu::trace_line(c, 0x0100, 0xFB);
    EXPECT_TRUE(line.find("pc=0100 op=FB k=00 db=00 ") == 0);
    const auto cyc = line.find(" cyc=");
    EXPECT_NE(cyc, std::string::npos);
    EXPECT_EQ(line.rfind("cyc=", line.size() - 1),
              cyc + 1);  // cyc is the final key=value pair
}

TEST(disasm, width_aware_lengths) {
    EXPECT_EQ(snescpu::insn_len(0xA9, true, false), 2);   // LDA # (8-bit A)
    EXPECT_EQ(snescpu::insn_len(0xA9, false, false), 3);  // LDA # (16-bit A)
    EXPECT_EQ(snescpu::insn_len(0xA2, false, true), 2);   // LDX # (8-bit X)
    EXPECT_EQ(snescpu::insn_len(0xAF, false, false), 4);  // long operand
    EXPECT_EQ(std::string(snescpu::mnemonic(0xFB)), "XCE");
}

TEST(golden, demo_trace_matches_committed_golden) {
    const std::string base = std::string(LABS_SRC_DIR);
    const auto rom = read_bytes(base + "/roms/demo.bin");
    const std::string got = run_trace(rom, 1000);
    const std::string want = read_text(base + "/golden/demo.trace");
    EXPECT_EQ(got, want);
}
