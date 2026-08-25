#pragma once
// Exercise 04 — an executed-op-count optimization pass over the IR.
//
// The cost model is deterministic: executed IR operations (never wall
// time). These passes shrink dynamic op counts on the benchmark workload
// by >= 20% while provably preserving the observable dump (OUT log +
// memory). Passes are classic interpreter/JIT peepholes:
//
//   fold_identities : add/sub/or with r0 -> mov, xor-self -> li, ...
//   copy_propagate  : rewrite sources through known register equivalences
//   fuse_dec_branch : "addi rd,rd,i ; bnez rd" -> one DecBr op
//   eliminate_dead  : drop defs no successor can observe (liveness-driven)
#include "ir.hpp"

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace rx8 {

using RegMask = uint32_t;  // bit i = register i

inline RegMask insn_uses(const IrInsn& in) {
    switch (in.op) {
        case IrOp::Mov:
        case IrOp::Load:
        case IrOp::Br: return RegMask(1) << in.rs;
        case IrOp::Alu: {
            RegMask u = RegMask(1) << in.rs;
            if (!in.use_imm) u |= RegMask(1) << in.rt;
            return u;
        }
        case IrOp::Store: return (RegMask(1) << in.rd) | (RegMask(1) << in.rs);
        case IrOp::Out: return RegMask(1) << in.rd;
        case IrOp::DecBr: return RegMask(1) << in.rd;  // reads AND writes rd
        default: return 0;
    }
}

inline RegMask insn_defs(const IrInsn& in) {
    switch (in.op) {
        case IrOp::Li:
        case IrOp::Mov:
        case IrOp::Alu:
        case IrOp::Load:
        case IrOp::DecBr: return RegMask(1) << in.rd;
        default: return 0;
    }
}

void fold_identities(std::vector<IrBlock>& blocks);

inline void fold_identities(std::vector<IrBlock>& blocks) {
    for (IrBlock& b : blocks) {
        for (IrInsn& in : b.insns) {
            if (in.op == IrOp::Alu) {
                if (in.rd == 0) {  // pure compute into r0 vanishes
                    in.op = IrOp::Nop;
                    continue;
                }
                if (!in.use_imm) {
                    if ((in.alu == AluOp::Add || in.alu == AluOp::Sub ||
                         in.alu == AluOp::Or) &&
                        in.rt == 0) {
                        in.op = IrOp::Mov;          // x +/- 0, x | 0
                    } else if (in.alu == AluOp::And && in.rs == in.rt) {
                        in.op = IrOp::Mov;          // x & x
                    } else if (in.alu == AluOp::Xor && in.rs == in.rt) {
                        in.op = IrOp::Li;           // x ^ x == 0
                        in.imm12 = 0;
                    }
                } else if (in.alu == AluOp::Add) {
                    if (in.rs == 0) {
                        in.op = IrOp::Li;           // constant materialization
                    } else if (in.imm12 == 0) {
                        in.op = IrOp::Mov;          // x + 0
                    }
                }
            } else if (in.op == IrOp::Mov && in.rd == 0) {
                in.op = IrOp::Nop;
            }
        }
    }
}

inline void copy_propagate(std::vector<IrBlock>& blocks) {
    for (IrBlock& b : blocks) {
        std::unordered_map<uint8_t, uint8_t> eq;  // reg -> reg known equal
        auto resolve = [&eq](uint8_t r) {
            while (eq.count(r)) r = eq[r];
            return r;
        };
        auto kill = [&eq](uint8_t rd) {
            eq.erase(rd);
            for (auto it = eq.begin(); it != eq.end();) {
                if (it->second == rd) it = eq.erase(it);
                else ++it;
            }
        };
        for (IrInsn& in : b.insns) {
            // Rewrite sources through known equivalences.
            if (in.rs != 0 || in.op == IrOp::Out) {
                if (in.rs != 0) in.rs = resolve(in.rs);
            }
            if (in.op == IrOp::Alu && !in.use_imm && in.rt != 0) {
                in.rt = resolve(in.rt);
            }
            // Update equivalence state for this definition.
            switch (in.op) {
                case IrOp::Mov:
                    if (in.rd != 0) {
                        kill(in.rd);
                        eq[in.rd] = in.rs;  // rs already rewritten above
                    }
                    break;
                case IrOp::Li:
                case IrOp::Alu:
                case IrOp::Load:
                case IrOp::DecBr:  // runtime write kills knowledge
                    if (insn_defs(in) != 0) kill(in.rd);
                    break;
                default: break;
            }
        }
    }
}

// Backward dataflow to fixpoint; returns live-in per block (index-aligned
// with the input vector). Register r0 is never live.
inline std::vector<RegMask> block_live_in(
    const std::vector<IrBlock>& blocks) {
    std::unordered_map<uint32_t, size_t> idx;
    for (size_t i = 0; i < blocks.size(); ++i) idx[blocks[i].entry] = i;

    std::vector<RegMask> gen(blocks.size(), 0), kill(blocks.size(), 0);
    for (size_t i = 0; i < blocks.size(); ++i) {
        RegMask g = 0, k = 0;
        for (const IrInsn& in : blocks[i].insns) {
            g |= insn_uses(in) & ~k;
            k |= insn_defs(in);
        }
        gen[i] = g;
        kill[i] = k;
    }

    std::vector<RegMask> live_in(blocks.size(), 0);
    bool changed = true;
    while (changed) {
        changed = false;
        for (size_t i = blocks.size(); i-- > 0;) {
            RegMask out = 0;
            for (uint32_t succ : {blocks[i].taken, blocks[i].fallthrough}) {
                auto it = idx.find(succ);
                if (succ != kNoLink && it != idx.end())
                    out |= live_in[it->second];
            }
            const RegMask in_set = gen[i] | (out & ~kill[i]);
            if (in_set != live_in[i]) {
                live_in[i] = in_set;
                changed = true;
            }
        }
    }
    return live_in;
}

inline void eliminate_dead(std::vector<IrBlock>& blocks,
                           const std::vector<RegMask>& live_in) {
    std::unordered_map<uint32_t, size_t> idx;
    for (size_t i = 0; i < blocks.size(); ++i) idx[blocks[i].entry] = i;

    for (size_t bi = 0; bi < blocks.size(); ++bi) {
        IrBlock& b = blocks[bi];
        RegMask live = 0;
        for (uint32_t succ : {b.taken, b.fallthrough}) {
            auto it = idx.find(succ);
            if (succ != kNoLink && it != idx.end())
                live |= live_in[it->second];
        }
        std::vector<IrInsn> kept;
        kept.reserve(b.insns.size());
        for (size_t i = b.insns.size(); i-- > 0;) {
            const IrInsn& in = b.insns[i];
            const bool pure = in.op == IrOp::Li || in.op == IrOp::Mov ||
                              in.op == IrOp::Alu;
            const RegMask defs = insn_defs(in);
            // Pure defs no successor can observe are dropped; everything
            // with side effects (Store/Out/branches) always survives.
            if (pure && defs != 0 && (live & defs) == 0) continue;
            live &= ~(defs & ~insn_uses(in));
            live |= insn_uses(in);
            kept.push_back(in);
        }
        std::reverse(kept.begin(), kept.end());
        b.insns = std::move(kept);
    }
}

inline void fuse_dec_branch(std::vector<IrBlock>& blocks) {
    for (IrBlock& b : blocks) {
        if (b.insns.size() < 2) continue;
        const size_t n = b.insns.size();
        const IrInsn& dec = b.insns[n - 2];
        const IrInsn& br = b.insns[n - 1];
        if (dec.op == IrOp::Alu && dec.use_imm && dec.alu == AluOp::Add &&
            dec.rd == dec.rs && dec.rd != 0 && br.op == IrOp::Br &&
            br.br_kind == 0 && br.rs == dec.rd) {
            IrInsn fused;
            fused.op = IrOp::DecBr;
            fused.rd = dec.rd;
            fused.imm12 = dec.imm12;
            fused.target = br.target;
            b.insns[n - 2] = fused;
            b.insns.pop_back();
        }
    }
}

// Driver: pass order matters — folds expose copies, copies expose dead
// code, fusion runs before the final liveness cleanup.
inline void optimize(std::vector<IrBlock>& blocks) {
    fold_identities(blocks);
    copy_propagate(blocks);
    fuse_dec_branch(blocks);
    const std::vector<RegMask> live = block_live_in(blocks);
    eliminate_dead(blocks, live);
}

}  // namespace rx8
