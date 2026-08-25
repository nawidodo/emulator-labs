#pragma once
// Watchpoints: stop the world when memory changes or a register predicate
// becomes true (curriculum ch6 "memory watch").
//
//   watch <addr>[:<len>]     break when any byte in the range changes
//   watch V<x><op><value>    break when the predicate holds, e.g. V3==2A
//
// Operators for register predicates: ==, !=, <=, >=, <, >.

#include <cstdint>
#include <cstdio>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "chip8.hpp"

namespace ch06 {

inline std::string hex2(uint32_t v) {
    char buf[8];
    snprintf(buf, sizeof buf, "%02X", v);
    return buf;
}
inline std::string hex4(uint32_t v) {
    char buf[8];
    snprintf(buf, sizeof buf, "%04X", v);
    return buf;
}
inline int hex_digit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}
// Parses a pure-hex string ("400", "0x400"); -1 if not valid hex.
inline long long parse_hex_ll(const std::string& s) {
    std::string t = s;
    if (t.compare(0, 2, "0x") == 0 || t.compare(0, 2, "0X") == 0) t = t.substr(2);
    if (t.empty()) return -1;
    long long v = 0;
    for (char c : t) {
        const int d = hex_digit(c);
        if (d < 0) return -1;
        v = v * 16 + d;
    }
    return v;
}
inline uint8_t parse_hex_byte(const std::string& s) {
    return uint8_t(parse_hex_ll(s) & 0xFF);
}


// One watchpoint request, either a memory range or a register predicate.
struct WatchSpec {
    bool is_mem = true;
    uint16_t addr = 0;   // mem form
    uint16_t len = 1;
    uint8_t reg = 0;     // register form
    char op = '=';
    uint8_t value = 0;
};

inline std::string describe(const WatchSpec& w) {
    if (!w.is_mem) {
        return std::string("V") + "0123456789ABCDEF"[w.reg] +
               std::string(1, w.op) + hex2(w.value);
    }
    return w.len == 1 ? "mem[" + hex4(w.addr) + "]"
                      : "mem[" + hex4(w.addr) + ".." + hex4(w.addr + w.len - 1) + "]";
}

//@LABS-BEGIN 1
//@LABS-SOLUTION
// Parses "0400", "0400:8" or "V3==2A". Returns nullopt on malformed input.
inline std::optional<WatchSpec> parse_watch_spec(std::istringstream& is) {
    std::string tok;
    if (!(is >> tok)) return std::nullopt;

    if (tok.size() >= 2 && (tok[0] == 'V' || tok[0] == 'v')) {
        const size_t op_pos = tok.find_first_of("=!<>", 1);
        if (op_pos == std::string::npos || op_pos > 2) return std::nullopt;
        const int reg = int(hex_digit(tok[1]));
        if (reg < 0) return std::nullopt;
        char op;
        size_t val_pos;
        if (tok.compare(op_pos, 2, "==") == 0) { op = '='; val_pos = op_pos + 2; }
        else if (tok.compare(op_pos, 2, "!=") == 0) { op = '!'; val_pos = op_pos + 2; }
        else if (tok.compare(op_pos, 2, "<=") == 0) { op = 'L'; val_pos = op_pos + 2; }
        else if (tok.compare(op_pos, 2, ">=") == 0) { op = 'G'; val_pos = op_pos + 2; }
        else if (tok[op_pos] == '<') { op = '<'; val_pos = op_pos + 1; }
        else if (tok[op_pos] == '>') { op = '>'; val_pos = op_pos + 1; }
        else return std::nullopt;
        const int value = int(parse_hex_byte(tok.substr(val_pos)));
        WatchSpec w;
        w.is_mem = false;
        w.reg = uint8_t(reg);
        w.op = op;
        w.value = uint8_t(value);
        return w;
    }

    // Memory range: ADDR[:LEN]
    WatchSpec w;
    const size_t colon = tok.find(':');
    const std::string addr_s = colon == std::string::npos ? tok : tok.substr(0, colon);
    const long long addr = parse_hex_ll(addr_s);
    if (addr < 0 || addr >= int(kMemSize)) return std::nullopt;
    w.addr = uint16_t(addr);
    if (colon != std::string::npos) {
        const long long len = parse_hex_ll(tok.substr(colon + 1));
        if (len <= 0 || addr + len > int(kMemSize)) return std::nullopt;
        w.len = uint16_t(len);
    }
    return w;
}
//@LABS-STUB
// TODO(1): parse a watch specification token stream.
//   "0400" or "0400:8" -> memory-range spec (is_mem=true)
//   "V3==2A"           -> register predicate (is_mem=false), ops == != <= >= < >
// Return nullopt for anything malformed.
inline std::optional<WatchSpec> parse_watch_spec(std::istringstream& is) {
    (void)is;
    TODO_MARKER:;
    return std::nullopt;
}
//@LABS-END

class WatchSet {
public:
    void add(const WatchSpec& w, Chip8& cpu) {
        Entry e;
        e.spec = w;
        if (w.is_mem)
            e.snapshot.assign(cpu.mem + w.addr, cpu.mem + w.addr + w.len);
        entries_.push_back(std::move(e));
    }

    size_t size() const { return entries_.size(); }

    //@LABS-BEGIN 2
    //@LABS-SOLUTION
    // Evaluates every watchpoint against the CURRENT cpu state and returns
    // a hit description like:
    //   mem[0400] changed 00->AA at pc=0204
    //   V3=2A satisfies V3==2A at pc=0204
    // Empty string means nothing fired. Memory snapshots are refreshed so
    // the same change is not reported twice.
    std::string check(Chip8& cpu) {
        for (auto& e : entries_) {
            const WatchSpec& w = e.spec;
            if (w.is_mem) {
                for (uint16_t off = 0; off < w.len; ++off) {
                    const uint8_t now = cpu.mem[w.addr + off];
                    if (now == e.snapshot[off]) continue;
                    std::string msg = "watch hit: mem[" + hex4(uint32_t(w.addr + off)) +
                                      "] changed " + hex2(e.snapshot[off]) + "->" +
                                      hex2(now) + " at pc=" + hex4(cpu.pc);
                    e.snapshot[off] = now;
                    return msg;
                }
            } else if (pred_holds(cpu.v[w.reg], w.op, w.value)) {
                return "watch hit: V" + std::string(1, "0123456789ABCDEF"[w.reg]) +
                       "=" + hex2(cpu.v[w.reg]) + " satisfies V" +
                       std::string(1, "0123456789ABCDEF"[w.reg]) + pred_text(w.op) +
                       hex2(w.value) + " at pc=" + hex4(cpu.pc);
            }
        }
        return "";
    }
    //@LABS-STUB
    // TODO(2): evaluate all watchpoints (see SOLUTION contract above);
    // refresh memory snapshots after reporting so one change fires once.
    std::string check(Chip8& cpu) {
        (void)cpu;
        return "";
    }
    //@LABS-END

private:
    struct Entry {
        WatchSpec spec;
        std::vector<uint8_t> snapshot;
    };

    static bool pred_holds(uint8_t lhs, char op, uint8_t rhs) {
        switch (op) {
            case '=': return lhs == rhs;
            case '!': return lhs != rhs;
            case '<': return lhs < rhs;
            case '>': return lhs > rhs;
            case 'L': return lhs <= rhs;
            case 'G': return lhs >= rhs;
        }
        return false;
    }

    static std::string pred_text(char op) {
        switch (op) {
            case '=': return "==";
            case '!': return "!=";
            case '<': return "<";
            case '>': return ">";
            case 'L': return "<=";
            case 'G': return ">=";
        }
        return "?";
    }

    std::vector<Entry> entries_;
};

}  // namespace ch06
