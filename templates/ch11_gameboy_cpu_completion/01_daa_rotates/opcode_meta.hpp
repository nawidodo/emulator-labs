#pragma once
#include <array>
#include <cstdint>
#include <string_view>
#include <type_traits>

namespace gb {

// Static per-opcode facts. `cycles` is the minimum cost; `cycles_alt` is the
// extra T-cycles when a branch is taken (0 = no conditional timing).
struct Instruction {
    const char* name;
    uint8_t bytes;
    uint8_t cycles;
    uint8_t cycles_alt;
};

using Table = std::array<Instruction, 256>;

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Encoding-field names: register index order B C D E H L (HL) A; ALU op
// index order ADD ADC SUB SBC AND XOR OR CP.
inline constexpr std::string_view reg_name(int i) {
    switch (i) {
        case 0: return "b";
        case 1: return "c";
        case 2: return "d";
        case 3: return "e";
        case 4: return "h";
        case 5: return "l";
        case 6: return "(hl)";
        default: return "a";
    }
}

inline constexpr std::string_view alu_name(int op) {
    switch (op) {
        case 0: return "add a,";
        case 1: return "adc a,";
        case 2: return "sub ";
        case 3: return "sbc a,";
        case 4: return "and ";
        case 5: return "xor ";
        case 6: return "or ";
        default: return "cp ";
    }
}

inline constexpr std::string_view rp_name(int i) {
    switch (i) {
        case 0: return "bc";
        case 1: return "de";
        case 2: return "hl";
        default: return "sp";
    }
}

inline constexpr std::string_view cond_name(int i) {
    switch (i) {
        case 0: return "nz";
        case 1: return "z";
        case 2: return "nc";
        default: return "c";
    }
}
//@LABS-STUB
// TODO(1): map encoding indices to names. Registers: b c d e h l (hl) a.
// ALU ops: add adc sub sbc and xor or cp. Pairs: bc de hl sp.
// Conditions: nz z nc c.
inline constexpr std::string_view reg_name(int i) {
    (void)i;
    return "?";  // wrong on purpose
}
inline constexpr std::string_view alu_name(int op) {
    (void)op;
    return "?";  // wrong on purpose
}
inline constexpr std::string_view rp_name(int i) {
    (void)i;
    return "?";  // wrong on purpose
}
inline constexpr std::string_view cond_name(int i) {
    (void)i;
    return "?";  // wrong on purpose
}
//@LABS-END

inline void fill_unknown(Table& t) {
    for (auto& e : t) e = {"???", 1, 4, 0};
}

namespace detail {

// Grows names into a fixed pool so Table entries can point at stable C
// strings without heap allocation at startup.
class NamePool {
public:
    template <typename... Parts>
    const char* make(Parts&&... parts) {
        char* begin = &buf_[used_];
        char* w = begin;
        auto append = [&](const auto& p) {
            using P = std::decay_t<decltype(p)>;
            if constexpr (std::is_integral_v<P>) {
                if (p < 10) {
                    *w++ = static_cast<char>('0' + p);
                } else {
                    *w++ = static_cast<char>('0' + p / 10);
                    *w++ = static_cast<char>('0' + p % 10);
                }
            } else {
                for (const char c : std::string_view(p)) *w++ = c;
            }
        };
        (append(parts), ...);
        *w++ = '\0';
        used_ += static_cast<int>(w - begin);
        return begin;
    }

private:
    std::array<char, 16 * 1024> buf_{};
    int used_ = 0;
};

}  // namespace detail

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Block x=1 (0x40..0x7F): LD r[y], r[z]. One byte; 8 T-cycles iff an (HL)
// operand forces an extra memory access, else 4. y==z==6 (0x76) is HALT,
// filled by fill_misc_base().
inline void fill_load_block(Table& t, detail::NamePool& names) {
    for (int op = 0x40; op <= 0x7F; ++op) {
        if (op == 0x76) continue;
        const int y = (op >> 3) & 7;
        const int z = op & 7;
        const bool touches_hl = (y == 6) || (z == 6);
        t[op] = {names.make("ld ", reg_name(y), ",", reg_name(z)), 1,
                 static_cast<uint8_t>(touches_hl ? 8 : 4), 0};
    }
}
//@LABS-STUB
// TODO(2): fill opcodes 0x40..0x7F as "ld <r_y>,<r_z>", bytes=1, cycles=4,
// or 8 when either operand is (HL). Skip 0x76 (HALT).
inline void fill_load_block(Table& t, detail::NamePool& names) {
    (void)t;
    (void)names;
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Block x=2 (0x80..0xBF): ALU A,r[z].
inline void fill_alu_block(Table& t, detail::NamePool& names) {
    for (int op = 0x80; op <= 0xBF; ++op) {
        const int y = (op >> 3) & 7;
        const int z = op & 7;
        t[op] = {names.make(alu_name(y), reg_name(z)), 1,
                 static_cast<uint8_t>(z == 6 ? 8 : 4), 0};
    }
}
//@LABS-STUB
// TODO(3): fill 0x80..0xBF as "<alu_mnemonic><r_z>", bytes=1, cycles=4 or 8
// when the operand is (HL).
inline void fill_alu_block(Table& t, detail::NamePool& names) {
    (void)t;
    (void)names;
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
// Remaining base-page entries that do not fall out of the two regular blocks
// above: x=0 misc plus pair ops. Timing per Pan Docs.
inline void fill_misc_base(Table& t, detail::NamePool& names) {
    auto set = [&t](int op, const char* n, int b, int c, int alt = 0) {
        t[op] = {n, static_cast<uint8_t>(b), static_cast<uint8_t>(c),
                 static_cast<uint8_t>(alt)};
    };
    set(0x00, "nop", 1, 4);
    set(0x08, "ld (nn),sp", 3, 20);
    set(0x10, "stop", 2, 4);
    set(0x18, "jr e", 2, 12);
    set(0x20, names.make("jr ", cond_name(0), ",e"), 2, 8, 4);
    set(0x28, names.make("jr ", cond_name(1), ",e"), 2, 8, 4);
    set(0x30, names.make("jr ", cond_name(2), ",e"), 2, 8, 4);
    set(0x38, names.make("jr ", cond_name(3), ",e"), 2, 8, 4);
    set(0x07, "rlca", 1, 4);
    set(0x0F, "rrca", 1, 4);
    set(0x17, "rla", 1, 4);
    set(0x1F, "rra", 1, 4);
    set(0x27, "daa", 1, 4);
    set(0x2F, "cpl", 1, 4);
    set(0x37, "scf", 1, 4);
    set(0x3F, "ccf", 1, 4);
    set(0x76, "halt", 1, 4);

    for (int i = 0; i < 4; ++i) {
        const int b = i << 4;
        set(0x01 | b, names.make("ld ", rp_name(i), ",nn"), 3, 12);
        set(0x03 | b, names.make("inc ", rp_name(i)), 1, 8);
        set(0x0B | b, names.make("dec ", rp_name(i)), 1, 8);
        set(0x09 | b, names.make("add hl,", rp_name(i)), 1, 8);
    }

    set(0x02, "ld (bc),a", 1, 8);
    set(0x12, "ld (de),a", 1, 8);
    set(0x22, "ldi (hl),a", 1, 8);
    set(0x32, "ldd (hl),a", 1, 8);
    set(0x0A, "ld a,(bc)", 1, 8);
    set(0x1A, "ld a,(de)", 1, 8);
    set(0x2A, "ldi a,(hl)", 1, 8);
    set(0x3A, "ldd a,(hl)", 1, 8);

    for (int i = 0; i < 8; ++i) {
        if (i == 6) continue;  // (HL) forms are separate rows below
        set(0x04 | (i << 3), names.make("inc ", reg_name(i)), 1, 4);
        set(0x05 | (i << 3), names.make("dec ", reg_name(i)), 1, 4);
        set(0x06 | (i << 3), names.make("ld ", reg_name(i), ",n"), 2, 8);
    }
    set(0x34, "inc (hl)", 1, 12);
    set(0x35, "dec (hl)", 1, 12);
    set(0x36, "ld (hl),n", 2, 12);
}
//@LABS-STUB
// TODO(4): fill remaining base-page metadata per Pan Docs: x=0 misc rows
// (nop/stop/jr family/rotates/daa/cpl/scf/ccf/halt), pair loads + INC/DEC +
// ADD HL,rr, indirect A loads, and inc/dec/ld-immediate rows for every
// 8-bit register including the 12-cycle (HL) variants.
inline void fill_misc_base(Table& t, detail::NamePool& names) {
    fill_unknown(t);  // placeholder until you write it
    (void)names;
}
//@LABS-END

//@LABS-BEGIN 5
//@LABS-SOLUTION
// x=3 page: control flow, stack, immediate-operand ALU, I/O loads.
// Conditional rows carry both costs explicitly: `cycles` when not taken,
// cycles+cycles_alt when taken.
inline void fill_control_base(Table& t, detail::NamePool& names) {
    auto set = [&t](int op, const char* n, int b, int c, int alt = 0) {
        t[op] = {n, static_cast<uint8_t>(b), static_cast<uint8_t>(c),
                 static_cast<uint8_t>(alt)};
    };
    set(0xC0, names.make("ret ", cond_name(0)), 1, 8, 12);
    set(0xC8, names.make("ret ", cond_name(1)), 1, 8, 12);
    set(0xD0, names.make("ret ", cond_name(2)), 1, 8, 12);
    set(0xD8, names.make("ret ", cond_name(3)), 1, 8, 12);
    set(0xC9, "ret", 1, 16);
    set(0xD9, "reti", 1, 16);
    for (int i = 0; i < 4; ++i) {
        const int b = i << 3;
        set(0xC2 | b, names.make("jp ", cond_name(i), ",nn"), 3, 12, 4);
        set(0xC4 | b, names.make("call ", cond_name(i), ",nn"), 3, 12, 12);
    }
    set(0xC3, "jp nn", 3, 16);
    set(0xE9, "jp hl", 1, 4);
    set(0xCD, "call nn", 3, 24);
    static constexpr std::string_view rp2[4] = {"bc", "de", "hl", "af"};
    for (int i = 0; i < 4; ++i) {
        set(0xC5 | (i << 4), names.make("push ", rp2[i]), 1, 16);
        set(0xC1 | (i << 4), names.make("pop ", rp2[i]), 1, 12);
    }
    set(0xC6, "add a,n", 2, 8);
    set(0xCE, "adc a,n", 2, 8);
    set(0xD6, "sub n", 2, 8);
    set(0xDE, "sbc a,n", 2, 8);
    set(0xE6, "and n", 2, 8);
    set(0xEE, "xor n", 2, 8);
    set(0xF6, "or n", 2, 8);
    set(0xFE, "cp n", 2, 8);
    set(0xCB, "cb prefix", 1, 4);
    set(0xE0, "ldh (n),a", 2, 12);
    set(0xF0, "ldh a,(n)", 2, 12);
    set(0xE2, "ldh (c),a", 1, 8);
    set(0xF2, "ldh a,(c)", 1, 8);
    set(0xEA, "ld (nn),a", 3, 16);
    set(0xFA, "ld a,(nn)", 3, 16);
    set(0xE8, "add sp,e", 2, 16);
    set(0xF8, "ld hl,sp+e", 2, 12);
    set(0xF9, "ld sp,hl", 1, 8);
    set(0xF3, "di", 1, 4);
    set(0xFB, "ei", 1, 4);
    for (int v = 0; v < 8; ++v)
        set(0xC7 | (v << 3), names.make("rst ", v * 8, "h"), 1, 16);
    // Undocumented/invalid on SM83: hardware locks up. Metadata marks them;
    // exec() traps because they are not implemented.
    for (const int bad : {0xD3, 0xDB, 0xDD, 0xE3, 0xE4, 0xEB, 0xEC, 0xED,
                          0xF4, 0xFC, 0xFD}) {
        set(bad, "invalid", 1, 4);
    }
}
//@LABS-STUB
// TODO(5): fill the x=3 control-flow page. Conditional rows carry both the
// not-taken cost (`cycles`) and the extra taken delta (`cycles_alt`).
inline void fill_control_base(Table& t, detail::NamePool& names) {
    fill_unknown(t);  // placeholder until you write it
    (void)names;
}
//@LABS-END

//@LABS-BEGIN 6
//@LABS-SOLUTION
// CB page: x==0 rotate/shift r[z]; x==1 BIT, x==2 RES, x==3 SET of bit y.
// (HL) operands add access M-cycles; BIT reads only (12), RES/SET also
// write (16).
inline void fill_cb_table(Table& cb, detail::NamePool& names) {
    static constexpr std::string_view rot[8] = {"rlc", "rrc", "rl", "rr",
                                                "sla", "sra", "swap", "srl"};
    for (int op = 0; op <= 0xFF; ++op) {
        const int x = op >> 6;
        const int y = (op >> 3) & 7;
        const int z = op & 7;
        const bool hl = (z == 6);
        const char* nm;
        int cyc;
        if (x == 0) {
            nm = names.make(rot[y], " ", reg_name(z));
            cyc = hl ? 16 : 8;
        } else {
            const std::string_view kind =
                (x == 1) ? "bit " : (x == 2) ? "res " : "set ";
            nm = names.make(kind, y, ",", reg_name(z));
            cyc = !hl ? 8 : (x == 1) ? 12 : 16;
        }
        cb[op] = {nm, 2, static_cast<uint8_t>(cyc), 0};
    }
}
//@LABS-STUB
// TODO(6): build the CB-page table. Rotates/shifts: 8 cycles on registers,
// 16 via (HL); BIT (HL) reads only -> 12; RES/SET (HL) read+write -> 16.
inline void fill_cb_table(Table& cb, detail::NamePool& names) {
    fill_unknown(cb);  // placeholder until you write it
    (void)names;
}
//@LABS-END

inline Table build_base() {
    Table t{};
    fill_unknown(t);
    static detail::NamePool names;
    fill_load_block(t, names);
    fill_alu_block(t, names);
    fill_misc_base(t, names);
    fill_control_base(t, names);
    return t;
}

inline const Table& base_table() {
    static const Table t = build_base();
    return t;
}

inline const Table& cb_table() {
    static const Table t = [] {
        Table cb{};
        static detail::NamePool cb_names;
        fill_cb_table(cb, cb_names);
        return cb;
    }();
    return t;
}

inline const Instruction& opcode_info(uint8_t op) { return base_table()[op]; }
inline const Instruction& cb_info(uint8_t op) { return cb_table()[op]; }

}  // namespace gb
