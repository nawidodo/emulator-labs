#pragma once
// ch01/02_hex_dumper — canonical `hexdump -C` style dumper.
//
// Output contract (one line per 16 input bytes):
//
//   00000000  23 21 2f 75 73 72 2f 62 69 6e 2f 70 79 74 68 6f  |#!/usr/bin/pytho|
//   00000010  6e 33                                            |n3|
//   00000011
//
// Layout per data row, exactly:
//   - 8 lowercase hex digits of the row offset (multiple of 16)
//   - two spaces
//   - 16 hex columns of two digits + one trailing space each; missing bytes
//     render as three spaces so the gutter stays aligned
//   - one space, then the ASCII gutter: '|', one char per byte position
//     (printable ASCII 0x20..0x7e verbatim, anything else '.'), padded with
//     spaces to width 16, then '|'
//   - '\n'
// After the last data row: the total byte count as 8 hex digits + '\n'.
// Empty input produces just that final offset line ("00000000\n").
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <span>

namespace ch01 {

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline void write_offset(std::ostream& os, uint32_t offset) {
    static const char kDigits[] = "0123456789abcdef";
    char buf[8];
    for (int i = 7; i >= 0; --i) {
        buf[i] = kDigits[offset & 0xFu];
        offset >>= 4;
    }
    os.write(buf, 8);
}
//@LABS-STUB
inline void write_offset(std::ostream& os, uint32_t offset) {
    (void)os;
    (void)offset;
    // TODO(1): write offset as exactly 8 lowercase hex digits
}
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
inline void write_hex_bytes(std::ostream& os,
                            std::span<const uint8_t> row) {
    static const char kDigits[] = "0123456789abcdef";
    for (unsigned col = 0; col < 16; ++col) {
        if (col < row.size()) {
            const uint8_t b = row[col];
            const char pair[3] = {kDigits[b >> 4], kDigits[b & 0xF], ' '};
            os.write(pair, 3);
        } else {
            os.write("   ", 3);  // keep column alignment for short rows
        }
    }
}
//@LABS-STUB
inline void write_hex_bytes(std::ostream& os,
                            std::span<const uint8_t> row) {
    (void)os;
    (void)row;
    // TODO(2): write 16 columns — "%02x " per present byte, three spaces
    // per missing byte
}
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
inline void write_ascii_gutter(std::ostream& os,
                               std::span<const uint8_t> row) {
    os.write(" |", 2);
    for (unsigned col = 0; col < 16; ++col) {
        if (col < row.size()) {
            const uint8_t b = row[col];
            const bool printable = b >= 0x20 && b <= 0x7e;
            const char c = printable ? static_cast<char>(b) : '.';
            os.write(&c, 1);
        } else {
            os.write(" ", 1);
        }
    }
    os.write("|", 1);
}
//@LABS-STUB
inline void write_ascii_gutter(std::ostream& os,
                               std::span<const uint8_t> row) {
    (void)os;
    (void)row;
    // TODO(3): " |" + printable chars ('.' otherwise), space-padded to 16,
    // then "|"
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
inline void dump(std::span<const uint8_t> data, std::ostream& os) {
    size_t off = 0;
    while (off < data.size()) {
        const size_t n = std::min(size_t{16}, data.size() - off);
        write_offset(os, static_cast<uint32_t>(off));
        os.write("  ", 2);
        write_hex_bytes(os, data.subspan(off, n));
        os.write(" ", 1);
        write_ascii_gutter(os, data.subspan(off, n));
        os.write("\n", 1);
        off += n;
    }
    write_offset(os, static_cast<uint32_t>(data.size()));
    os.write("\n", 1);
}
//@LABS-STUB
inline void dump(std::span<const uint8_t> data, std::ostream& os) {
    (void)data;
    (void)os;
    // TODO(4): loop over 16-byte rows calling the three helpers above,
    // then emit the final total-length offset line
}
//@LABS-END

}  // namespace ch01
