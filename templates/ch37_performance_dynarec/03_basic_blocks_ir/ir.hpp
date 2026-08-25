#pragma once
// Exercise 03, part 2 — the tiny host-independent IR.
//
// Each guest instruction lowers to exactly one IrInsn. This is deliberately
// NOT host machine code: a portable IR keeps the translation correct on
// every platform and gives the ch37/04 optimizer something shape-stable to
// rewrite. One guest instruction -> one IR op also makes "executed ops" a
// clean, deterministic cost model.
#include "blocks.hpp"

#include <cstdint>
#include <vector>

namespace rx8 {

enum class IrOp : uint8_t {
    Nop, Li, Mov, Alu, Load, Store, Out,
    Br,     // conditional/unconditional branch (br_kind), block terminator
    Halt,   // block terminator
    Undef,  // unknown guest opcode: faults at execution time
    DecBr,  // fused decrement-and-branch-if-nonzero; emitted ONLY by the
            // ch37/04 optimizer (fuse_dec_branch). Reserved here so every
            // tier agrees on the enum from day one.
};

enum class AluOp : uint8_t { Add, Sub, And, Or, Xor, Shl, Shr };

struct IrInsn {
    IrOp op = IrOp::Nop;
    AluOp alu = AluOp::Add;
    uint8_t rd = 0, rs = 0, rt = 0;
    uint16_t imm12 = 0;
    bool use_imm = false;       // Alu: apply simm() instead of r[rt]
    uint32_t target = kNoLink;  // Br/DecBr absolute byte target
    uint8_t br_kind = 0;        // Br: 0 nz(rs), 1 z(rs), 2 always

    int32_t simm() const {
        return (imm12 & 0x800) ? int32_t(imm12) - 4096 : int32_t(imm12);
    }
};

struct IrBlock {
    uint32_t entry = 0;
    uint32_t fallthrough = kNoLink;
    uint32_t taken = kNoLink;
    std::vector<IrInsn> insns;  // last insn is always the terminator
};

// Lower one decoded guest instruction to its IR form. Field positions
// mirror the guest encoding: for Store, rd is the BASE register and rs the
// stored source, exactly as in Decoded.
IrInsn lower_insn(const Decoded& d);

// Translate whole blocks (convenience wrapper over lower_insn).
inline std::vector<IrBlock> translate(const Machine& m,
                                      const std::vector<BasicBlock>& blocks) {
    std::vector<IrBlock> out;
    out.reserve(blocks.size());
    for (const BasicBlock& bb : blocks) {
        IrBlock ib;
        ib.entry = bb.start;
        ib.fallthrough = bb.fallthrough;
        ib.taken = bb.taken;
        for (uint32_t pc = bb.start; pc <= bb.term_pc; pc += 4) {
            ib.insns.push_back(lower_insn(decode(m.read_le(pc))));
        }
        out.push_back(std::move(ib));
    }
    return out;
}

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline IrInsn lower_insn(const Decoded& d) {
    IrInsn in;
    in.rd = d.rd;
    in.rs = d.rs;
    in.rt = d.rt;
    in.imm12 = d.imm12;
    switch (d.op) {
        case OP_NOP: in.op = IrOp::Nop; break;
        case OP_MOV: in.op = IrOp::Mov; break;
        case OP_ADD: in.op = IrOp::Alu; in.alu = AluOp::Add; break;
        case OP_SUB: in.op = IrOp::Alu; in.alu = AluOp::Sub; break;
        case OP_AND: in.op = IrOp::Alu; in.alu = AluOp::And; break;
        case OP_OR: in.op = IrOp::Alu; in.alu = AluOp::Or; break;
        case OP_XOR: in.op = IrOp::Alu; in.alu = AluOp::Xor; break;
        case OP_SHL: in.op = IrOp::Alu; in.alu = AluOp::Shl; break;
        case OP_SHR: in.op = IrOp::Alu; in.alu = AluOp::Shr; break;
        case OP_ADDI:
            in.op = IrOp::Alu;
            in.alu = AluOp::Add;
            in.use_imm = true;
            break;
        case OP_LW: in.op = IrOp::Load; break;   // rd=dest, rs=base, imm12
        case OP_SW: in.op = IrOp::Store; break;  // rd=base, rs=src, imm12
        case OP_BEQZ:
            in.op = IrOp::Br;
            in.br_kind = 1;
            in.target = d.target();
            break;
        case OP_BNEZ:
            in.op = IrOp::Br;
            in.br_kind = 0;
            in.target = d.target();
            break;
        case OP_JMP:
            in.op = IrOp::Br;
            in.br_kind = 2;
            in.target = d.target();
            break;
        case OP_OUT: in.op = IrOp::Out; break;
        case OP_HALT: in.op = IrOp::Halt; break;
        default: in.op = IrOp::Undef; break;
    }
    return in;
}
//@LABS-STUB
// TODO(1): map every guest opcode onto its IrInsn form per the table in
// this header. Keep field positions identical to Decoded (Store carries
// rd=base / rs=src), copy imm12 raw, set use_imm only for ADDI, and give
// branches their absolute target plus br_kind (0 nz, 1 z, 2 always).
// Unknown opcodes become IrOp::Undef so execution faults like the
// interpreter does.
inline IrInsn lower_insn(const Decoded& d) {
    IrInsn in;
    (void)d;
    in.op = IrOp::Undef;  // wrong on purpose: lowers nothing correctly
    return in;
}
//@LABS-END

}  // namespace rx8
