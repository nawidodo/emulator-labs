#pragma once
// Coding test — unseen rx8 extension opcodes (see CODING_TEST.md).
//
// Three tiers must agree bit-exactly: the switch interpreter, the IR
// translation, and the optimized IR execution. The extended ALU kinds ride
// ABOVE the base rx8::AluOp value set; the base executor never emits or handles
// them, which is why the ext_exec hook exists in IrEngine.
#include "exec_ir.hpp"
#include "ir.hpp"
#include "opt.hpp"
#include "rx8.hpp"

#include <cstdint>
#include <span>
#include <string>

namespace rx8ext {

enum : uint8_t { OP_MUL = 0x11, OP_NOT = 0x12, OP_MIN = 0x13 };

enum class AluExt : uint8_t { Mul = 0x08, Not = 0x09, Min = 0x0A };

struct ExtResult {
    uint64_t ops = 0;
    std::string dump;
    bool halted = false;
    bool fault = false;
};

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Interpreter tier: execute one decoded EXTENSION instruction. Returns
// false when the opcode is not one of ours (caller falls back to
// rx8::execute).
inline bool execute_ext(rx8::Machine& m, const rx8::Decoded& d) {
    auto set_reg = [](rx8::Machine& mm, uint8_t rd, uint32_t v) {
        if (rd != 0) mm.r[rd] = v;
    };
    switch (d.op) {
        case OP_MUL:
            set_reg(m, d.rd, m.r[d.rs] * m.r[d.rt]);
            return true;
        case OP_NOT:
            set_reg(m, d.rd, ~m.r[d.rs]);
            return true;
        case OP_MIN: {
            const int32_t a = static_cast<int32_t>(m.r[d.rs]);
            const int32_t b = static_cast<int32_t>(m.r[d.rt]);
            set_reg(m, d.rd, static_cast<uint32_t>(a < b ? a : b));
            return true;
        }
        default:
            return false;
    }
}
//@LABS-STUB
// TODO(1): implement the interpreter tier for MUL (low 32 bits, wrapping),
// NOT (complement), and MIN (SIGNED 32-bit compare), honoring the r0 rule.
// Return true when you retired the insn, false for foreign opcodes so the
// caller falls back to rx8::execute.
inline bool execute_ext(rx8::Machine& m, const rx8::Decoded& d) {
    (void)m;
    (void)d;
    return false;  // wrong on purpose: no extension executes
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Translation tier: lower an extension decode onto an rx8::IrInsn using the
// extended ALU kinds. Returns false for foreign opcodes.
inline bool lower_ext(const rx8::Decoded& d, rx8::IrInsn& out) {
    out.op = rx8::IrOp::Alu;
    out.rd = d.rd;
    out.rs = d.rs;
    out.rt = d.rt;
    out.imm12 = d.imm12;
    switch (d.op) {
        case OP_MUL:
            out.alu = static_cast<rx8::AluOp>(AluExt::Mul);
            return true;
        case OP_NOT:
            out.alu = static_cast<rx8::AluOp>(AluExt::Not);
            return true;
        case OP_MIN:
            out.alu = static_cast<rx8::AluOp>(AluExt::Min);
            return true;
        default:
            out = rx8::IrInsn{};
            return false;
    }
}
//@LABS-STUB
// TODO(2): translate the extension opcodes to rx8::IrOp::Alu insns carrying the
// matching AluExt kind (fields copied straight from the guest encoding,
// imm12 raw). Reset `out` and return false for foreign opcodes.
inline bool lower_ext(const rx8::Decoded& d, rx8::IrInsn& out) {
    (void)d;
    (void)out;
    return false;  // wrong on purpose: lowers nothing
}
//@LABS-END

namespace detail {
inline bool is_ext_alu(const rx8::IrInsn& in) {
    return in.op == rx8::IrOp::Alu &&
           static_cast<uint8_t>(in.alu) >= static_cast<uint8_t>(AluExt::Mul);
}
}  // namespace detail

//@LABS-BEGIN 3
//@LABS-SOLUTION
// Full pipeline: translate (with extension lowering), optionally rx8::optimize,
// install, execute with the extension hook wired. This is the entry point
// the hidden gate exercises.
inline ExtResult run_ext(std::span<const uint8_t> image, bool optimize_ir,
                         uint64_t max_ops) {
    rx8::Machine m;
    m.load(image);

    // Offline translation pass with extension-aware lowering.
    std::vector<rx8::BasicBlock> layout =
        rx8::find_blocks(m, uint32_t(std::min<size_t>(image.size(), rx8::kMemSize)));
    std::vector<rx8::IrBlock> blocks;
    blocks.reserve(layout.size());
    for (const rx8::BasicBlock& bb : layout) {
        rx8::IrBlock ib;
        ib.entry = bb.start;
        ib.fallthrough = bb.fallthrough;
        ib.taken = bb.taken;
        for (uint32_t pc = bb.start; pc <= bb.term_pc; pc += 4) {
            const rx8::Decoded d = rx8::decode(m.read_le(pc));
            rx8::IrInsn in;
            if (!lower_ext(d, in)) in = rx8::lower_insn(d);
            ib.insns.push_back(std::move(in));
        }
        blocks.push_back(std::move(ib));
    }
    if (optimize_ir) rx8::optimize(blocks);

    rx8::IrEngine eng;
    eng.load(image);
    eng.ext_exec = [](rx8::Machine& mm, const rx8::IrInsn& in) -> bool {
        if (!detail::is_ext_alu(in)) return false;
        const uint32_t a = mm.r[in.rs];
        const uint32_t b = mm.r[in.rt];
        uint32_t v = 0;
        switch (static_cast<AluExt>(in.alu)) {
            case AluExt::Mul: v = a * b; break;
            case AluExt::Not: v = ~a; break;
            case AluExt::Min:
                v = static_cast<uint32_t>(
                    static_cast<int32_t>(a) < static_cast<int32_t>(b) ? a : b);
                break;
        }
        if (in.rd != 0) mm.r[in.rd] = v;
        return true;
    };
    eng.install(std::move(blocks));
    eng.run(max_ops);

    ExtResult r;
    r.ops = eng.ops_executed;
    r.dump = rx8::observable_dump(eng.m);
    r.halted = eng.m.halted;
    r.fault = eng.m.fault;
    return r;
}
//@LABS-STUB
// TODO(3): assemble the whole extension pipeline. Translate every block
// yourself: try lower_ext() first, fall back to rx8::lower_insn(). Apply
// rx8::optimize() when optimize_ir is set. Then load an IrEngine with the
// image, wire eng.ext_exec so extended ALU kinds compute their result
// (respecting the r0 rule) and everything else falls through (return
// false), install(blocks), run(max_ops), and fill an ExtResult{ops, dump
// (rx8::observable_dump), halted, fault}.
inline ExtResult run_ext(std::span<const uint8_t> image, bool optimize_ir,
                         uint64_t max_ops) {
    (void)image;
    (void)optimize_ir;
    (void)max_ops;
    return {};  // wrong on purpose: the pipeline does nothing
}
//@LABS-END

}  // namespace rx8ext
