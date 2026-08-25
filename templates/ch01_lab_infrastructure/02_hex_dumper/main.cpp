#define LABSTEST_MAIN
#include "labstest.hpp"
#include <cstddef>

#include <sstream>
#include <string>

#include "dumper.hpp"

namespace {

std::string render(std::span<const uint8_t> data) {
    std::ostringstream os;
    ch01::dump(data, os);
    return os.str();
}

// Expected data-row builder mirroring SPEC: offset + 2sp + 16 hex columns
// (3 chars each) + 1sp + " |ascii|" (interior padded to 16).
std::string expect_row(uint32_t off, const uint8_t* b, size_t n) {
    std::string s = "00000000  ";
    for (int i = 7; i >= 0; --i) {
        static const char* d = "0123456789abcdef";
        s[7 - i] = d[(off >> (i * 4)) & 0xF];
    }
    for (unsigned c = 0; c < 16; ++c) {
        if (c < n) {
            static const char* h = "0123456789abcdef";
            s += h[b[c] >> 4];
            s += h[b[c] & 0xF];
            s += ' ';
        } else {
            s += "   ";
        }
    }
    s += "  |";  // dump() emits one space before the " |" gutter opener
    for (unsigned c = 0; c < 16; ++c) {
        if (c < n) {
            s += (b[c] >= 0x20 && b[c] <= 0x7e) ? char(b[c]) : '.';
        } else {
            s += ' ';
        }
    }
    s += '|';
    return s;
}

constexpr uint8_t SHEBANG6[] = {0x23, 0x21, 0x2f, 0x75, 0x73, 0x72};  // "#!/usr"

}  // namespace

TEST(dumper_offset, zero_pads_to_eight) {
    std::ostringstream os;
    ch01::write_offset(os, 0x1u);
    EXPECT_EQ(os.str(), std::string("00000001"));
}

TEST(dumper_offset, lowercase_full_width) {
    std::ostringstream os;
    ch01::write_offset(os, 0xDEADBEEFu);
    EXPECT_EQ(os.str(), std::string("deadbeef"));
}

TEST(dumper_hex, partial_row_keeps_trailing_space_per_byte) {
    std::ostringstream os;
    ch01::write_hex_bytes(os, SHEBANG6);
    // six "%02x " columns then ten 3-space filler columns
    EXPECT_EQ(os.str(),
              std::string("23 21 2f 75 73 72 ") + std::string(30, ' '));
    EXPECT_EQ(os.str().size(), size_t{48});
}

TEST(dumper_hex, full_row_is_48_columns) {
    const uint8_t row[16] = {0};
    std::ostringstream os;
    ch01::write_hex_bytes(os, row);
    EXPECT_EQ(os.str().size(), size_t{48});
}

TEST(dumper_ascii, printable_verbatim_others_dotted) {
    // 0x20 (space) IS printable; NUL, DEL and high bytes are dots.
    const uint8_t row[] = {'H', 'i', '!', 0x00, 0x7F, 0x80, 0xFF, 0x20};
    std::ostringstream os;
    ch01::write_ascii_gutter(os, row);
    std::string want = " |Hi!.... ";
    want += std::string(8, ' ');
    want += "|";
    EXPECT_EQ(os.str(), want);
}

TEST(dumper_ascii, pads_short_rows_to_sixteen) {
    const uint8_t row[] = {'A'};
    std::ostringstream os;
    ch01::write_ascii_gutter(os, row);
    std::string want = " |A";
    want += std::string(15, ' ');
    want += "|";
    EXPECT_EQ(os.str(), want);
}

TEST(dumper_dump, empty_input_is_final_offset_only) {
    EXPECT_EQ(render({}), std::string("00000000\n"));
}

TEST(dumper_dump, short_row_golden_shape) {
    const uint8_t data[] = {'H', 'i', '!', 0xC3, 0xA9};
    std::string want = expect_row(0, data, 5) + "\n00000005\n";
    EXPECT_EQ(render(data), want);
    // Independent sanity pins on the composed expectation:
    EXPECT_EQ(want.substr(0, 21), std::string("00000000  48 69 21 c3"));
    EXPECT_EQ(want.size(), size_t{88});
}

TEST(dumper_dump, crosses_row_boundary_with_final_offset) {
    uint8_t data[19];
    for (unsigned i = 0; i < 19; ++i) {
        data[i] = uint8_t(0x20 + i);  // printable run ' '..'?'
    }
    std::string want = expect_row(0, data, 16) + "\n" +
                       expect_row(16, data + 16, 3) + "\n00000013\n";
    EXPECT_EQ(want.substr(60, 18),
              std::string("| !\"#$%&'()*+,-./|"));
}
