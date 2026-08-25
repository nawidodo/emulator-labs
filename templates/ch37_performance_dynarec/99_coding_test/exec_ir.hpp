#pragma once
// Exercise 03, part 3 — executing the IR with a translation cache.
//
// IrEngine mirrors the architectural state in a plain Machine and walks
// translated IrBlocks. Translations are cached per block entry and FLUSHED
// whenever a store lands inside the code region — the same invalidation
// discipline as exercise 02's decode cache, now at block granularity.
#include "ir.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rx8 {

class IrEngine {
public:
    Machine m;                   // architectural state (regs/mem/out/pc)
    uint32_t code_end = 0;
    uint64_t ops_executed = 0;   // executed IR ops — the pipeline cost model
    size_t translations = 0;     // blocks currently cached
    uint64_t flushes = 0;        // times the translation cache was dropped

    // Extension hook for the ch37/99 coding test: return true when the
    // hook executed the insn.
    std::function<bool(Machine&, const IrInsn&)> ext_exec;

    void load(std::span<const uint8_t> image) {
        m.load(image);
        code_end = uint32_t(std::min<size_t>(image.size(), kMemSize));
        ops_executed = 0;
        flushes = 0;
        drop_translations();
        analyze();
    }

    // Install pre-translated (possibly optimized) blocks, e.g. an offline
    // translate/optimize pass ahead of execution. Untranslated entries are
    // still filled lazily from fresh analysis.
    void install(std::vector<IrBlock> blocks) {
        drop_translations();
        for (IrBlock& b : blocks) {
            const uint32_t e = b.entry;
            cache_.push_back(std::move(b));
            index_[e] = cache_.size() - 1;
        }
        translations = cache_.size();
        analyze();
    }

    // Run until halt/fault or op budget; returns cumulative ops executed.
    uint64_t run(uint64_t max_ops);

private:
    std::vector<IrBlock> cache_;
    std::unordered_map<uint32_t, size_t> index_;
    std::vector<BasicBlock> layout_;
    bool dirty_ = false;

    void drop_translations() {
        cache_.clear();
        index_.clear();
        translations = 0;
        dirty_ = false;
    }
    void analyze() { layout_ = find_blocks(m, code_end); }

    const IrBlock& block_for(uint32_t entry) {
        auto it = index_.find(entry);
        if (it != index_.end()) return cache_[it->second];
        // Lazy translation of one block from the current memory image.
        for (const BasicBlock& bb : layout_) {
            if (bb.start != entry) continue;
            IrBlock ib;
            ib.entry = bb.start;
            ib.fallthrough = bb.fallthrough;
            ib.taken = bb.taken;
            for (uint32_t pc = bb.start; pc <= bb.term_pc; pc += 4) {
                ib.insns.push_back(lower_insn(decode(m.read_le(pc))));
            }
            cache_.push_back(std::move(ib));
            index_[entry] = cache_.size() - 1;
            translations = cache_.size();
            return cache_.back();
        }
        static const IrBlock empty{};
        return empty;  // caller faults on pc == kNoLink / unknown entry
    }

    // Execute one non-control IR insn against m; marks dirty_ when a store
    // lands inside the code region.
    void exec_insn(const IrInsn& in);
};

inline void IrEngine::exec_insn(const IrInsn& in) {
    switch (in.op) {
        case IrOp::Nop: break;
        case IrOp::Li:
            if (in.rd != 0) m.r[in.rd] = uint32_t(in.imm12);
            break;
        case IrOp::Mov:
            if (in.rd != 0) m.r[in.rd] = m.r[in.rs];
            break;
        case IrOp::Alu: {
            if (in.rd == 0) break;  // r0 writes vanish
            const uint32_t a = m.r[in.rs];
            const uint32_t b = in.use_imm ? uint32_t(in.simm()) : m.r[in.rt];
            uint32_t v = 0;
            switch (in.alu) {
                case AluOp::Add: v = a + b; break;
                case AluOp::Sub: v = a - b; break;
                case AluOp::And: v = a & b; break;
                case AluOp::Or: v = a | b; break;
                case AluOp::Xor: v = a ^ b; break;
                case AluOp::Shl: v = a << (b & 31); break;
                case AluOp::Shr: v = a >> (b & 31); break;
            }
            m.r[in.rd] = v;
            break;
        }
        case IrOp::Load: {
            // Fault behavior must survive even when rd is r0.
            const uint32_t v = m.load_word(m.r[in.rs] + uint32_t(in.simm()));
            if (in.rd != 0) m.r[in.rd] = v;
            break;
        }
        case IrOp::Store: {
            // rd=base, rs=src (guest field positions preserved).
            const uint32_t addr = m.r[in.rd] + uint32_t(in.simm());
            m.store_word(addr, m.r[in.rs]);
            if (!m.fault && addr < code_end) dirty_ = true;
            break;
        }
        case IrOp::Out:
            m.out.push_back(m.r[in.rd]);
            break;
        default:
            break;  // control insns are handled by run()
    }
}

inline uint64_t IrEngine::run(uint64_t max_ops) {
    while (!m.halted && !m.fault && ops_executed < max_ops) {
        const IrBlock& b = block_for(m.pc);
        bool ended = false;
        for (const IrInsn& in : b.insns) {
            ++ops_executed;
            if (ext_exec && ext_exec(m, in)) continue;
            switch (in.op) {
                case IrOp::Br: {
                    const uint32_t v = m.r[in.rs];
                    const bool taken = in.br_kind == 2 ||
                                       (in.br_kind == 0 ? v != 0 : v == 0);
                    m.pc = taken ? in.target : b.fallthrough;
                    ended = true;
                    break;
                }
                case IrOp::DecBr: {
                    // Optimizer-only fused decrement-and-branch.
                    if (in.rd != 0) m.r[in.rd] += uint32_t(in.simm());
                    const bool taken = m.r[in.rd] != 0;
                    m.pc = taken ? in.target : b.fallthrough;
                    ended = true;
                    break;
                }
                case IrOp::Halt:
                    m.halted = true;
                    ended = true;
                    break;
                case IrOp::Undef:
                    m.fault = true;
                    ended = true;
                    break;
                default:
                    exec_insn(in);
                    break;
            }
            if (ended || m.halted || m.fault) break;
        }
        if (!ended && !m.halted && !m.fault) {
            m.pc = b.fallthrough;  // defensive: never happens for well-formed IR
        }
        if (dirty_) {  // SMC: the code region changed under our feet
            drop_translations();
            ++flushes;
            analyze();
        }
        if (m.pc == kNoLink || m.pc % 4 != 0 || m.pc >= kMemSize) m.fault = true;
    }
    return ops_executed;
}

}  // namespace rx8
