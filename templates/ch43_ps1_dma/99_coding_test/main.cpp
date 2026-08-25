// ct_dma_tests — public unit tests + fixture mode for the ch43 coding test.
#include <cstring>
#include <fstream>
#include <iostream>
#include <vector>
#include <cstddef>

#include "labstest.hpp"
#include "chain_inspect.hpp"

using ps1ct::ChainStatus;
using ps1ct::Ram;

namespace {
std::vector<uint32_t> load_words(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "error: cannot open " << path << "\n";
        exit(2);
    }
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)),
                            std::istreambuf_iterator<char>());
    const size_t n = bytes.size() / 4;
    std::vector<uint32_t> words(n);
    if (n) std::memcpy(words.data(), bytes.data(), n * 4);
    return words;
}
}  // namespace

TEST(ct, single_packet_with_sentinel_is_ok) {
    Ram ram;
    ram.write(0x00, 0x01000008u);          // 1 payload word at 0x4
    ram.write(0x04, 0x12345678u);
    ram.write(0x08, ps1ct::kTerminator);   // final header = sentinel
    EXPECT_EQ(ps1ct::inspect_chain(ram, 0x00), ChainStatus::Ok);
}

TEST(ct, zero_length_packets_are_legal) {
    Ram ram;
    ram.write(0x00, 0x00000010u);
    ram.write(0x10, 0x00000020u);
    ram.write(0x20, ps1ct::kTerminator);
    EXPECT_EQ(ps1ct::inspect_chain(ram, 0x00), ChainStatus::Ok);
}

TEST(ct, unaligned_start_rejected_before_read) {
    Ram ram;
    EXPECT_EQ(ps1ct::inspect_chain(ram, 0x02),
              ChainStatus::PointerOutOfRange);
}

TEST(ct, dangling_link_out_of_range) {
    Ram ram;
    ram.write(0x00, 0x01000000u |
                        static_cast<uint32_t>(ram.word_slots() * 4 + 8));
    EXPECT_EQ(ps1ct::inspect_chain(ram, 0x00),
              ChainStatus::PointerOutOfRange);
}

TEST(ct, self_loop_detected) {
    Ram ram;
    ram.write(0x00, 0x01000000u);  // next link == own header address
    EXPECT_EQ(ps1ct::inspect_chain(ram, 0x00), ChainStatus::SelfLoop);
}

TEST(ct, two_node_cycle_hits_cap) {
    Ram ram;
    ram.write(0x00, 0x01000010u);
    ram.write(0x10, 0x01000000u);
    EXPECT_EQ(ps1ct::inspect_chain(ram, 0x00),
              ChainStatus::MissingTerminator);
}

int main(int argc, char** argv) {
    if (argc > 1) {
        // Fixture mode used by the hidden manifest.
        Ram ram;
        const auto words = load_words(argv[1]);
        for (size_t i = 0; i < words.size() && i < ram.word_slots(); ++i)
            ram.write(static_cast<uint32_t>(4 * i), words[i]);
        const auto st = ps1ct::inspect_chain(ram, 0x00);
        std::cout << "status=" << ps1ct::status_name(st) << "\n";
        return st == ChainStatus::Ok ? 0 : 1;
    }
    ::labstest::run_all("");
    return ::labstest::failures() == 0 ? 0 : 1;
}
