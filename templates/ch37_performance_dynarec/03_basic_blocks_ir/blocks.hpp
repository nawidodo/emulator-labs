#pragma once
// Exercise 03, part 1 — basic-block analysis.
//
// A basic block is a straight-line run of guest instructions that can only
// be entered at its first instruction and only left at its last. Leaders
// (block starts) are: the entry point, every branch/jump target, and every
// address that follows a terminator. Everything an optimizing backend later
// does is phrased over these units — this is where dynarec parts ways with
// per-instruction interpretation.
#include "rx8.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace rx8 {

constexpr uint32_t kNoLink = 0xFFFFFFFFu;

struct BasicBlock {
    uint32_t start = 0;             // byte address of the first instruction
    uint32_t term_pc = 0;           // address of the terminating instruction
    uint32_t fallthrough = kNoLink; // next block when not taken / not jumped
    uint32_t taken = kNoLink;       // branch or jump target
};

// Scan the code region [0, code_end) of a loaded machine and return all
// basic blocks in ascending start order. Fixed 4-byte encodings make the
// linear scan exact: decode each word, mark leaders, close blocks at
// terminators (beqz/bnez/jmp/halt).
std::vector<BasicBlock> find_blocks(const Machine& m, uint32_t code_end);

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline std::vector<BasicBlock> find_blocks(const Machine& m,
                                           uint32_t code_end) {
    const uint32_t words = std::min(code_end, kMemSize) / 4;
    std::vector<bool> leader(words, false);
    if (words != 0) leader[0] = true;
    auto terminates = [](uint8_t op) {
        return op == OP_BEQZ || op == OP_BNEZ || op == OP_JMP ||
               op == OP_HALT;
    };
    for (uint32_t i = 0; i < words; ++i) {
        const Decoded d = decode(m.read_le(i * 4));
        if (terminates(d.op)) {
            if (i + 1 < words) leader[i + 1] = true;
            if (d.op != OP_HALT) {
                const uint32_t t = d.target() / 4;
                if (t < words) leader[t] = true;
            }
        }
    }

    std::vector<BasicBlock> blocks;
    uint32_t i = 0;
    while (i < words) {
        BasicBlock b;
        b.start = i * 4;
        bool terminated = false;
        while (i < words) {
            const uint32_t at = i * 4;
            // Another leader means the current block must end HERE — a
            // branch target can land mid-run without any terminator.
            if (leader[i] && at != b.start) break;
            const Decoded d = decode(m.read_le(at));
            ++i;
            b.term_pc = at;
            if (!terminates(d.op)) continue;
            terminated = true;
            if (d.op != OP_JMP && d.op != OP_HALT && i < words) {
                b.fallthrough = i * 4;
            }
            if (d.op != OP_HALT) b.taken = d.target();
            break;
        }
        if (!terminated && i < words) b.fallthrough = i * 4;
        blocks.push_back(b);
    }
    return blocks;
}
//@LABS-STUB
// TODO(1): two passes over [0, code_end). Pass A marks leaders: word 0,
// every branch/jump target (d.target()/4), and the word after every
// terminator (beqz/bnez/jmp/halt). Pass B walks words in order opening a
// BasicBlock at each leader; a block CLOSES at its terminator (fill
// term_pc/fallthrough/taken, kNoLink after jmp/halt or past the image)
// or immediately BEFORE the next leader (a branch target can land mid-run
// with no terminator: fallthrough only). Return blocks in ascending order.
inline std::vector<BasicBlock> find_blocks(const Machine& m,
                                           uint32_t code_end) {
    (void)m;
    (void)code_end;
    return {};  // wrong on purpose: finds nothing
}
//@LABS-END

}  // namespace rx8
