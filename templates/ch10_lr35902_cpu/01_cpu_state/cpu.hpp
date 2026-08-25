#pragma once
#include <cstdint>

namespace gb {

// Flag masks inside F. Only the high nibble is meaningful; the low nibble
// always reads as zero on hardware.
enum Flag : uint8_t {
    FLAG_C = 1u << 4,
    FLAG_H = 1u << 5,
    FLAG_N = 1u << 6,
    FLAG_Z = 1u << 7,
};

// SM83 programmer's model. Reset values match the DMG boot ROM handoff so
// fixture programs start in a realistic state.
struct Registers {
    uint8_t a{0x01};
    uint8_t f{0xB0};
    uint8_t b{0x00};
    uint8_t c{0x13};
    uint8_t d{0x00};
    uint8_t e{0xD8};
    uint8_t h{0x01};
    uint8_t l{0x4D};
    uint16_t sp{0xFFFE};
    uint16_t pc{0x0100};

    void reset() { *this = Registers{}; }

//@LABS-BEGIN 2
//@LABS-SOLUTION
    // F accessors enforce the read-as-0 low nibble.
    void set_f(uint8_t value) { f = value & 0xF0; }

    bool flag_z() const { return (f & FLAG_Z) != 0; }
    bool flag_n() const { return (f & FLAG_N) != 0; }
    bool flag_h() const { return (f & FLAG_H) != 0; }
    bool flag_c() const { return (f & FLAG_C) != 0; }

    void set_z(bool on) { f = on ? uint8_t(f | FLAG_Z) : uint8_t(f & ~FLAG_Z); }
    void set_n(bool on) { f = on ? uint8_t(f | FLAG_N) : uint8_t(f & ~FLAG_N); }
    void set_h(bool on) { f = on ? uint8_t(f | FLAG_H) : uint8_t(f & ~FLAG_H); }
    void set_c(bool on) { f = on ? uint8_t(f | FLAG_C) : uint8_t(f & ~FLAG_C); }
//@LABS-STUB
    // TODO(2): implement F accessors. Writes must mask the low nibble.
    void set_f(uint8_t value) {
        (void)value;
        f = 0;  // wrong on purpose
    }
    bool flag_z() const {
        (void)this;
        return false;  // TODO(2)
    }
    bool flag_n() const {
        (void)this;
        return false;  // TODO(2)
    }
    bool flag_h() const {
        (void)this;
        return false;  // TODO(2)
    }
    bool flag_c() const {
        (void)this;
        return false;  // TODO(2)
    }
    void set_z(bool on) { (void)on; }   // TODO(2)
    void set_n(bool on) { (void)on; }   // TODO(2)
    void set_h(bool on) { (void)on; }   // TODO(2)
    void set_c(bool on) { (void)on; }   // TODO(2)
//@LABS-END

//@LABS-BEGIN 3
//@LABS-SOLUTION
    // 16-bit pair views. BC means b<<8 | c; AF packs A high, F low
    // (set_af keeps the low-nibble rule).
    uint16_t bc() const { return uint16_t(b) << 8 | c; }
    uint16_t de() const { return uint16_t(d) << 8 | e; }
    uint16_t hl() const { return uint16_t(h) << 8 | l; }
    uint16_t af() const { return uint16_t(a) << 8 | (f & 0xF0); }

    void set_bc(uint16_t v) { b = uint8_t(v >> 8); c = uint8_t(v); }
    void set_de(uint16_t v) { d = uint8_t(v >> 8); e = uint8_t(v); }
    void set_hl(uint16_t v) { h = uint8_t(v >> 8); l = uint8_t(v); }
    void set_af(uint16_t v) { a = uint8_t(v >> 8); set_f(uint8_t(v)); }
//@LABS-STUB
    // TODO(3): implement the pair views and setters.
    uint16_t bc() const {
        (void)this;
        return 0;  // wrong on purpose
    }
    uint16_t de() const {
        (void)this;
        return 0;  // wrong on purpose
    }
    uint16_t hl() const {
        (void)this;
        return 0;  // wrong on purpose
    }
    uint16_t af() const {
        (void)this;
        return 0;  // wrong on purpose
    }
    void set_bc(uint16_t v) { (void)v; }  // TODO(3)
    void set_de(uint16_t v) { (void)v; }  // TODO(3)
    void set_hl(uint16_t v) { (void)v; }  // TODO(3)
    void set_af(uint16_t v) { (void)v; }  // TODO(3)
//@LABS-END
};

}  // namespace gb
