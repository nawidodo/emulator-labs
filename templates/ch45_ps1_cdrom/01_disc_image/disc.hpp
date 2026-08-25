#pragma once
//
// ch45 / 01_disc_image — BIN/CUE parsing, MSF<->LBA math and sector
// access with header validation (psx-spx "CD-ROM Format").
//
// Raw sector = 2352 bytes:
//   [0..11]  sync: 00 FF FF FF FF FF FF FF FF FF FF 00
//   [12]     minute (BCD)   [13] second   [14] frame   [15] mode
//   MODE2 (0x02): [16..19] subheader; form1 -> 2048 user bytes at 24,
//   form2 -> 2324 bytes at 24 (XA ADPCM audio passthrough: out of scope,
//   returned opaque).
// LBA <-> MSF: LBA counts from -150 (the 2-second lead-in is excluded).

#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace cdrom {

constexpr unsigned kRawSectorSize = 2352;
constexpr unsigned kUserDataSize = 2048;

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline int32_t msf_to_lba(unsigned m, unsigned s, unsigned f) {
    return static_cast<int32_t>((m * 60 + s) * 75 + f) - 150;
}

inline void lba_to_msf(int32_t lba, unsigned& m, unsigned& s,
                       unsigned& f) {
    const int32_t total = lba + 150;  // undo the lead-in bias
    f = static_cast<unsigned>(total % 75);
    s = static_cast<unsigned>((total / 75) % 60);
    m = static_cast<unsigned>(total / 75 / 60);
}
//@LABS-STUB
// TODO(1): LBA = (M*60+S)*75+F minus the 150-frame lead-in bias; the
// inverse adds it back before splitting into M/S/F.
int32_t msf_to_lba(unsigned m, unsigned s, unsigned f) {
    (void)m; (void)s; (void)f;
    return 0;  // wrong on purpose
}
void lba_to_msf(int32_t lba, unsigned& m, unsigned& s, unsigned& f) {
    (void)lba; (void)m; (void)s; (void)f;
}
//@LABS-END

struct Track {
    unsigned number = 0;
    std::string type = "MODE2/2352";
    int32_t lba_start = 0;
};

//@LABS-BEGIN 2
//@LABS-SOLUTION
// Minimal CUE sheet reader: understands FILE/TRACK/INDEX lines for a
// single-binary, single-track MODE2/2352 layout (all this lab needs).
inline bool parse_cue(const std::string& cue_text, Track& out) {
    std::istringstream ss(cue_text);
    std::string line;
    bool saw_index = false;
    while (std::getline(ss, line)) {
        std::istringstream ls(line);
        std::string keyword;
        if (!(ls >> keyword)) continue;
        if (keyword == "TRACK") {
            unsigned num = 0;
            std::string type;
            ls >> num >> type;
            out.number = num;
            out.type = type;
        } else if (keyword == "INDEX") {
            unsigned idx = 0, mm = 0, ss_ = 0, ff = 0;
            char c1, c2;
            ls >> idx >> mm >> c1 >> ss_ >> c2 >> ff;
            if (!ls.fail() && c1 == ':' && c2 == ':' &&
                out.type.rfind("MODE2/2352", 0) == 0) {
                out.lba_start = msf_to_lba(mm, ss_, ff);
                saw_index = true;
            }
        }
    }
    return saw_index;
}
//@LABS-STUB
// TODO(2): parse TRACK (number + type) and INDEX 01 MM:SS:FF lines.
// Return true only when a MODE2/2352 INDEX 01 was seen; fill lba_start
// via msf_to_lba. Non-MODE2/2352 tracks must be rejected.
bool parse_cue(const std::string& cue_text, Track& out) {
    (void)cue_text; (void)out;
    return false;  // wrong on purpose
}
//@LABS-END

inline bool sector_is_form2(const uint8_t* raw) {
    return (raw[16 + 2] & 0x20u) != 0;  // subheader byte 2, bit 5
}

// User-data view: form1 -> 2048 bytes at offset 24. Form2 payloads are
// XA ADPCM audio passthrough — rejected here (documented scope stub).
inline bool sector_user_data(const uint8_t* raw, const uint8_t** begin,
                             unsigned* size) {
    if (sector_is_form2(raw)) return false;
    *begin = raw + 24;
    *size = kUserDataSize;
    return true;
}

class DiscImage {
public:
    bool load(const std::string& cue_path, const std::string& bin_path,
              std::string* err = nullptr) {
        std::ifstream cf(cue_path);
        if (!cf) {
            if (err) *err = "cannot open cue: " + cue_path;
            return false;
        }
        std::stringstream buf;
        buf << cf.rdbuf();
        if (!parse_cue(buf.str(), track_)) {
            if (err) *err = "unsupported cue layout";
            return false;
        }
        std::ifstream bf(bin_path, std::ios::binary);
        if (!bf) {
            if (err) *err = "cannot open bin: " + bin_path;
            return false;
        }
        sectors_ = std::vector<uint8_t>(
            (std::istreambuf_iterator<char>(bf)),
            std::istreambuf_iterator<char>());
        if (sectors_.size() % kRawSectorSize != 0) {
            if (err) *err = "bin size not a multiple of 2352";
            return false;
        }
        return true;
    }

    unsigned sector_count() const {
        return static_cast<unsigned>(sectors_.size() / kRawSectorSize);
    }
    const Track& track() const { return track_; }

//@LABS-BEGIN 3
//@LABS-SOLUTION
    // Copies raw sector bytes; validates the sync pattern and that the
    // embedded BCD header matches `lba`. Returns false on any mismatch
    // or out-of-range lba (real drives would report seek errors here).
    bool read_sector(int32_t lba, uint8_t out[kRawSectorSize]) const {
        if (lba < 0 || static_cast<unsigned>(lba) >= sector_count())
            return false;
        const uint8_t* p = &sectors_[static_cast<size_t>(lba) *
                                     kRawSectorSize];
        static const uint8_t kSync[12] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF,
                                          0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
                                          0xFF, 0x00};
        for (unsigned i = 0; i < 12; ++i)
            if (p[i] != kSync[i]) return false;
        unsigned m, s, f;
        lba_to_msf(lba, m, s, f);
        if (bcd(m) != p[12] || bcd(s) != p[13] || bcd(f) != p[14])
            return false;
        if (p[15] != 0x02) return false;  // this lab handles MODE2 only
        for (unsigned i = 0; i < kRawSectorSize; ++i) out[i] = p[i];
        return true;
    }

private:
    static uint8_t bcd(unsigned v) {
        return static_cast<uint8_t>(((v / 10) << 4) | (v % 10));
    }
    Track track_{};
    std::vector<uint8_t> sectors_;
//@LABS-STUB
    // TODO(3): implement read_sector (sync pattern, embedded BCD header
    // match, MODE2 check). On failure return false without copying.
public:
    bool read_sector(int32_t lba, uint8_t out[kRawSectorSize]) const {
        (void)lba; (void)out;
        return false;  // wrong on purpose
    }
    Track track_{};
    std::vector<uint8_t> sectors_{};
//@LABS-END
};

}  // namespace cdrom
