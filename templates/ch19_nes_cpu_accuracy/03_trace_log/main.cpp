#define LABSTEST_MAIN
#include "labstest.hpp"

#include <cstdint>
#include <string>

#include "cpu.hpp"

using namespace nes6502;

namespace {

struct Rig {
    FlatRam ram;
    Cpu cpu{.bus = &ram};

    void prog(std::initializer_list<uint8_t> bytes, uint16_t base = 0x0600) {
        cpu.load_program(base, bytes.begin(), bytes.size());
    }
    void prog(const uint8_t* bytes, size_t n, uint16_t base = 0x0600) {
        cpu.load_program(base, bytes, n);
    }
};

std::string dis(Rig& r, uint16_t pc) { return disassemble_at(r.ram, pc); }

}  // namespace

TEST(trace, disassembles_every_addressing_shape) {
    Rig r;
    const uint8_t prog[] = {
        0xA9, 0x05,                    // LDA #$05
        0xA5, 0x40,                    // LDA $40
        0xB5, 0x40,                    // LDA $40,X
        0xB6, 0x40,                    // LDX $40,Y
        0xAD, 0x34, 0x12,              // LDA $1234
        0xBD, 0x34, 0x12,              // LDA $1234,X
        0xB9, 0x34, 0x12,              // LDA $1234,Y
        0xA1, 0x40,                    // LDA ($40,X)
        0xB1, 0x40,                    // LDA ($40),Y
        0x6C, 0x50, 0x06,              // JMP ($0650)
        0x9D, 0xFE, 0x21,              // STA $21FE,X
        0x0A,                          // ASL A
        0xE8,                          // INX
        0x00,                          // BRK
    };
    r.prog(prog, sizeof prog);
    EXPECT_EQ(dis(r, 0x0600), "LDA #$05");
    EXPECT_EQ(dis(r, 0x0602), "LDA $40");
    EXPECT_EQ(dis(r, 0x0604), "LDA $40,X");
    EXPECT_EQ(dis(r, 0x0606), "LDX $40,Y");
    EXPECT_EQ(dis(r, 0x0608), "LDA $1234");
    EXPECT_EQ(dis(r, 0x060B), "LDA $1234,X");
    EXPECT_EQ(dis(r, 0x060E), "LDA $1234,Y");
    EXPECT_EQ(dis(r, 0x0611), "LDA ($40,X)");
    EXPECT_EQ(dis(r, 0x0613), "LDA ($40),Y");
    EXPECT_EQ(dis(r, 0x0615), "JMP ($0650)");
    EXPECT_EQ(dis(r, 0x0618), "STA $21FE,X");
    EXPECT_EQ(dis(r, 0x061B), "ASL A");
    EXPECT_EQ(dis(r, 0x061C), "INX");
    EXPECT_EQ(dis(r, 0x061D), "BRK");
}

TEST(trace, disassembles_unofficial_subset) {
    Rig r;
    const uint8_t prog[] = {
        0x1A,                          // NOP (implied)
        0x04, 0x40,                    // NOP $40
        0xA7, 0x40,                    // LAX $40
        0x87, 0x41,                    // SAX $41
        0xC7, 0x42,                    // DCP $42
        0xE7, 0x43,                    // ISB $43
        0x07, 0x44,                    // SLO $44
        0x27, 0x45,                    // RLA $45
    };
    r.prog(prog, sizeof prog);
    EXPECT_EQ(dis(r, 0x0600), "NOP");
    EXPECT_EQ(dis(r, 0x0601), "NOP $40");
    EXPECT_EQ(dis(r, 0x0603), "LAX $40");
    EXPECT_EQ(dis(r, 0x0605), "SAX $41");
    EXPECT_EQ(dis(r, 0x0607), "DCP $42");
    EXPECT_EQ(dis(r, 0x0609), "ISB $43");
    EXPECT_EQ(dis(r, 0x060B), "SLO $44");
    EXPECT_EQ(dis(r, 0x060D), "RLA $45");
}

TEST(trace, unknown_opcodes_render_as_question_marks_len_one) {
    Rig r;
    r.ram.mem[0x0600] = 0x02;      // JAM row: not in the table
    EXPECT_EQ(dis(r, 0x0600), "???");
    EXPECT_EQ(disasm_len(0x02), 1);
}

TEST(trace, instruction_lengths_match_the_mode_families) {
    EXPECT_EQ(disasm_len(0xEA), 1);  // implied
    EXPECT_EQ(disasm_len(0x0A), 1);  // accumulator
    EXPECT_EQ(disasm_len(0xA9), 2);  // imm
    EXPECT_EQ(disasm_len(0xA5), 2);  // zp
    EXPECT_EQ(disasm_len(0xB1), 2);  // (zp),Y
    EXPECT_EQ(disasm_len(0xAD), 3);  // abs
    EXPECT_EQ(disasm_len(0xBD), 3);  // abs,X
    EXPECT_EQ(disasm_len(0x6C), 3);  // ind
}

TEST(trace, peek_trace_captures_pc_opcode_operands_and_text) {
    Rig r;
    r.prog({0xBD, 0x34, 0x12});    // LDA $1234,X at $0600
    const TraceRow row = peek_trace(r.ram, 0x0600);
    EXPECT_EQ(row.pc, 0x0600);
    EXPECT_EQ(row.op, 0xBD);
    EXPECT_EQ(row.b1, 0x34);
    EXPECT_EQ(row.b2, 0x12);
    EXPECT_EQ(row.text, "LDA $1234,X");
}

TEST(trace, trace_line_matches_the_column_contract) {
    Rig r;
    r.cpu.a = 0x05;
    r.cpu.x = 0x00;
    r.cpu.y = 0x00;
    r.cpu.p = FU | FZ;             // 0x22 after Z set? FU|FZ = 0x22
    r.cpu.sp = 0xFD;
    r.cpu.cycles = 7;
    TraceRow row;                  // hand-built row for LDA #$05 at $0600
    row.pc = 0x0600;
    row.op = 0xA9;
    row.b1 = 0x05;
    row.b2 = 0x00;
    row.text = "LDA #$05";
    // Layout: "%04X  " + op slot + operand slots (blank when absent) + " "
    // + text in 10 columns + registers.
    EXPECT_EQ(trace_line(r.cpu, row),
              "0600  A9 05     LDA #$05   A:05 X:00 Y:00 P:22 SP:FD CYC:7");
}

TEST(trace, trace_line_blank_operand_slots_keep_columns_aligned) {
    Rig r;
    r.cpu.a = 0x11;
    r.cpu.p = 0x24;
    r.cpu.sp = 0xFA;
    r.cpu.cycles = 35874;
    TraceRow row;                  // implied INX: no operand slots
    row.pc = 0xC002;
    row.op = 0xE8;
    row.text = "INX";
    EXPECT_EQ(trace_line(r.cpu, row),
              "C002  E8        INX        A:11 X:00 Y:00 P:24 SP:FA CYC:35874");
}
