#pragma once
// GB-style multi-device state aggregation. Each device registers a named
// section with fixed size; the registry serializes them into ONE versioned
// blob with a section table, so devices can be added without breaking
// older readers (version + names are checked, never assumed).
//
// Blob layout (little-endian):
//   magic "LBST" | u8 version(1) | u8 reserved | u16 section_count
//   per section: u8 name_len | name bytes | u32 size | payload
#include <cstdint>
#include <cstring>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace registry {

inline constexpr uint8_t kBlobVersion = 1;
inline constexpr char kMagic[4] = {'L', 'B', 'S', 'T'};

struct Section {
    std::string name;
    uint32_t size;
    // write fills exactly `size` bytes at dst; read consumes them.
    std::function<void(uint8_t* dst)> write;
    std::function<void(const uint8_t* src)> read;
};

class StateRegistry {
public:
    // Register a device section. Fixed sizes keep offsets computable and
    // make accidental layout drift loud.
    void add(const char* name, uint32_t size,
             std::function<void(uint8_t*)> write,
             std::function<void(const uint8_t*)> read) {
        sections_.push_back(
            Section{name, size, std::move(write), std::move(read)});
    }

    size_t section_count() const { return sections_.size(); }

//@LABS-BEGIN 1
//@LABS-SOLUTION
    bool save(std::vector<uint8_t>& out) const {
        out.clear();
        auto put = [&out](const void* p, size_t n) {
            const auto* b = static_cast<const uint8_t*>(p);
            out.insert(out.end(), b, b + n);
        };
        put(kMagic, 4);
        out.push_back(kBlobVersion);
        out.push_back(0);  // reserved
        const uint16_t count = uint16_t(sections_.size());
        out.push_back(uint8_t(count));
        out.push_back(uint8_t(count >> 8));
        for (const Section& s : sections_) {
            const auto len = uint8_t(s.name.size());
            out.push_back(len);
            put(s.name.data(), len);
            const uint32_t sz = s.size;
            for (int k = 0; k < 4; ++k)
                out.push_back(uint8_t(sz >> (8 * k)));
            std::vector<uint8_t> buf(s.size);
            s.write(buf.data());
            put(buf.data(), buf.size());
        }
        return true;
    }
//@LABS-STUB
    bool save(std::vector<uint8_t>& out) const {
        // TODO(1): build the blob — magic, version byte, reserved byte,
        // section count (u16 LE), then per section: name length byte,
        // name, size (u32 LE), payload (call s.write into a scratch
        // buffer). Return false only on allocation failure.
        (void)out;
        return false;  // wrong on purpose: saves nothing
    }
//@LABS-END

//@LABS-BEGIN 2
//@LABS-SOLUTION
    bool load(std::span<const uint8_t> in) const {
        size_t o = 0;
        auto need = [&](size_t n) { return in.size() - o >= n; };
        auto take = [&](void* p, size_t n) {
            if (!need(n)) return false;
            std::memcpy(p, in.data() + o, n);
            o += n;
            return true;
        };
        char magic[4];
        if (!take(magic, 4) || std::memcmp(magic, kMagic, 4) != 0)
            return false;
        uint8_t version = 0, reserved = 0;
        if (!take(&version, 1) || !take(&reserved, 1)) return false;
        if (version != kBlobVersion) return false;  // foreign layout: NO
        uint8_t lo = 0, hi = 0;
        if (!take(&lo, 1) || !take(&hi, 1)) return false;
        const uint16_t count = uint16_t(lo | (hi << 8));
        if (count != sections_.size()) return false;  // schema mismatch
        for (const Section& s : sections_) {
            uint8_t len = 0;
            if (!take(&len, 1)) return false;
            std::string name(len, '?');
            if (!take(name.data(), len) || name != s.name ||
                s.size > in.size() - o)
                return false;
            if (!need(4)) return false;
            o += 4;  // stored size is redundant with registered size
            std::vector<uint8_t> buf(s.size);
            if (!take(buf.data(), s.size)) return false;
            s.read(buf.data());
        }
        return true;
    }
//@LABS-STUB
    bool load(std::span<const uint8_t>) const {
        // TODO(2): parse and dispatch. Validate magic AND version byte
        // AND that blob section names/order match the registered
        // sections; on any mismatch return false WITHOUT calling any
        // reader.
        return false;  // wrong on purpose: loads nothing
    }
//@LABS-END

private:
    std::vector<Section> sections_;
};

}  // namespace registry
