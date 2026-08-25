// bus.hpp — ordered range->device routing for the SM83 address space.
//
// WHY device objects instead of a giant switch or one flat 64 KiB array:
//   * a flat array cannot express side effects (a write to FF50 must be
//     observable by the boot-remap logic, not just store a byte);
//   * a switch duplicates knowledge of the map in two places (read and
//     write) and grows O(map) every time hardware is added;
//   * an ordered table gives overlays for free: the FIRST entry whose
//     range contains the address wins, so a boot ROM overlaid in front
//     of the cartridge shadows it without touching cart code at all.
//
// Every address the CPU puts on the bus resolves through exactly one
// scan of this table. See LECTURE.md for the full map and policies.
#pragma once

#include <cstdint>
#include <vector>

namespace gbmap {

// Documented open/unmapped policy for this chapter: unpopulated decode
// slots read $00 and silently drop writes. (Real boards float; $00 keeps
// runs deterministic. Other consoles differ — the challenge in 91 uses
// $FF open bus on purpose.)
constexpr uint8_t kOpenBusByte = 0x00;

// Anything routable implements this. Side effects live here: devices see
// every read/write aimed inside their range and nothing else.
class Device {
public:
    virtual ~Device() = default;
    virtual uint8_t read(uint16_t addr) = 0;
    virtual void write(uint16_t addr, uint8_t val) = 0;
};

// Plain RAM sized to exactly its decoded range. Indexing is wrap-free:
// the Bus only ever delivers addresses inside [lo, hi], so cells are
// addressed directly as (addr - lo) with no modulo — a wrap would hide
// routing bugs instead of surfacing them.
class Ram : public Device {
public:
    Ram(uint16_t lo, uint16_t hi)
        : lo_(lo), hi_(hi), cells_(static_cast<size_t>(hi - lo) + 1u, 0x00) {}

    uint8_t read(uint16_t addr) override { return cells_[addr - lo_]; }
    void write(uint16_t addr, uint8_t val) override { cells_[addr - lo_] = val; }

    uint16_t lo() const { return lo_; }
    uint16_t hi() const { return hi_; }
    size_t size() const { return cells_.size(); }
    const std::vector<uint8_t>& cells() const { return cells_; }

private:
    uint16_t lo_;
    uint16_t hi_;
    std::vector<uint8_t> cells_;
};

struct RangeEntry {
    uint16_t lo;
    uint16_t hi;  // inclusive on BOTH ends — half-open ranges are the classic bus off-by-one
    Device* device;
};

class Bus {
public:
    // Inserts keeping the table ascending by base address so scans are
    // deterministic regardless of attach order. Ties on the same base go
    // to the EARLIER attach: re-registering a range later never silently
    // hijacks an existing device — use attachFront for overlays.
    void attach(uint16_t lo, uint16_t hi, Device* dev) {
        auto pos = table_.begin();
        while (pos != table_.end() && pos->lo <= lo) ++pos;
        table_.insert(pos, RangeEntry{lo, hi, dev});
    }

    // Overlay support (exercised in 04_boot_rom): front entries win the
    // first-match scan; detach removes every entry pointing at dev.
    void attachFront(uint16_t lo, uint16_t hi, Device* dev) {
        table_.insert(table_.begin(), RangeEntry{lo, hi, dev});
    }
    size_t detach(const Device* dev) {
        size_t removed = 0;
        for (auto it = table_.begin(); it != table_.end();) {
            if (it->device == dev) {
                it = table_.erase(it);
                ++removed;
            } else {
                ++it;
            }
        }
        return removed;
    }

    const std::vector<RangeEntry>& table() const { return table_; }

    //@LABS-BEGIN 1
    //@LABS-SOLUTION
    // Linear scan of the ordered table; the FIRST entry whose [lo, hi]
    // contains addr wins. Priority-by-order is what makes overlays work.
    const RangeEntry* findRange(uint16_t addr) const {
        for (const auto& e : table_)
            if (addr >= e.lo && addr <= e.hi) return &e;
        return nullptr;
    }
    //@LABS-STUB
    const RangeEntry* findRange(uint16_t addr) const {
        (void)addr;  // TODO(1): scan the ordered table, first containing entry wins
        return table_.empty() ? nullptr : &table_.front();
    }
    //@LABS-END

    //@LABS-BEGIN 2
    //@LABS-SOLUTION
    uint8_t read(uint16_t addr) const {
        if (const RangeEntry* e = findRange(addr)) return e->device->read(addr);
        return unmappedRead(addr);
    }
    //@LABS-STUB
    uint8_t read(uint16_t addr) const {
        (void)findRange(addr);  // TODO(2): route hits to the device, misses to unmappedRead
        return unmappedRead(addr);
    }
    //@LABS-END

    //@LABS-BEGIN 3
    //@LABS-SOLUTION
    void write(uint16_t addr, uint8_t val) {
        if (const RangeEntry* e = findRange(addr)) e->device->write(addr, val);
        else unmappedWrite(addr, val);
    }
    //@LABS-STUB
    void write(uint16_t addr, uint8_t val) {
        (void)addr;  // TODO(3): route hits to the device, misses to unmappedWrite
        (void)val;
    }
    //@LABS-END

    //@LABS-BEGIN 4
    //@LABS-SOLUTION
    // Documented policy for gaps: reads sample $00, writes are dropped.
    static uint8_t unmappedRead(uint16_t) { return kOpenBusByte; }
    static void unmappedWrite(uint16_t, uint8_t) {}
    //@LABS-STUB
    static uint8_t unmappedRead(uint16_t) {
        return 0xFF;  // TODO(4): documented policy says $00 — floating-high is another console's rule
    }
    static void unmappedWrite(uint16_t, uint8_t) {}  // TODO(4): keep writes dropped
    //@LABS-END

private:
    std::vector<RangeEntry> table_;
};

}  // namespace gbmap
