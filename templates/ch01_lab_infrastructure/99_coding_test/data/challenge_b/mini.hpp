#pragma once
// ch01/b — mini fixture: offset column formatting (challenge target 'ch01/b')

#include <cstdint>
#include <string>

namespace mini_b {

//%LABS-BEGIN 1
//%LABS-SOLUTION
inline std::string offset8(uint32_t offset) {
    static const char kDigits[] = "0123456789abcdef";
    std::string s(8, '0');
    for (int i = 7; i >= 0; --i) {
        s[i] = kDigits[offset & 0xFu];
        offset >>= 4;
    }
    return s;
}
//%LABS-STUB
inline std::string offset8(uint32_t offset) {
    (void)offset;
    return "";  // TODO(1): format as exactly 8 lowercase hex digits
}
//%LABS-END

}  // namespace mini_b
