#pragma once
#include <cstdint>
#include <optional>

#include "../01_adpcm/adpcm.hpp"

namespace spu {

// SPU RAM: the real SPU has 512 KiB of dedicated sound RAM.
constexpr uint32_t kSpuRamSize = 512 * 1024;

struct SpuRam {
    std::array<uint8_t, kSpuRamSize> data{};
};

// Voice register file (one slice of 1F801C00 + voice*0x10).
struct VoiceRegs {
    uint16_t vol_left = 0;
    uint16_t vol_right = 0;
    uint16_t pitch = 0x1000;        // 0x1000 == 44100 Hz (see ex03)
    uint16_t start_addr = 0;        // SPU RAM byte address >> 3
    uint16_t adsr1 = 0;
    uint16_t adsr2 = 0;
};

// One SPU voice: key on/off state plus ADPCM block walking through SPU RAM.
//
// Loop semantics implemented here (documented simplification of real HW):
// - a block carrying loop_start records its address as the repeat address;
// - when a block with loop_end finishes, playback resumes at the recorded
//   loop address if one was seen, otherwise the voice switches off;
// - a block carrying mute silences the rest of that block and ends it.
class Voice {
public:
    void reset();
    void key_on(const VoiceRegs& regs);   // begins playback at start_addr<<3
    void key_off();

    // Advances one output sample at the voice's base rate (pitch is applied
    // by the PitchStepper in ex03). Returns the decoded sample, or 0 while
    // the voice is off or muted.
    int16_t tick(const SpuRam& ram);

    bool active() const { return active_; }
    uint16_t pitch() const { return regs_.pitch; }
    uint32_t current_addr() const { return cur_addr_; }
    const std::optional<uint32_t>& loop_addr() const { return loop_addr_; }

private:
    // Decodes the 16-byte ADPCM block at cur_addr_ into block_.
    void fetch_block(const SpuRam& ram);

    VoiceRegs regs_{};
    AdpcmDecoder decoder_;
    DecodedBlock block_{};
    int sample_idx_ = 28;      // exhausted -> fetch next block on tick
    uint32_t cur_addr_ = 0;    // byte address into SPU RAM
    std::optional<uint32_t> loop_addr_;
    bool active_ = false;
    bool muted_ = false;
};

//@LABS-BEGIN 3
//@LABS-SOLUTION
inline void Voice::reset() {
    regs_ = {};
    decoder_.reset();
    sample_idx_ = 28;
    cur_addr_ = 0;
    loop_addr_.reset();
    active_ = false;
    muted_ = false;
}

inline void Voice::key_on(const VoiceRegs& regs) {
    regs_ = regs;
    decoder_.reset();
    loop_addr_.reset();
    cur_addr_ = static_cast<uint32_t>(regs.start_addr) << 3;
    sample_idx_ = 28;  // force a block fetch on the first tick
    muted_ = false;
    active_ = true;
}

inline void Voice::key_off() {
    // The ADSR release phase (ex04) fades the voice out; at this layer we
    // simply stop producing output.
    active_ = false;
}
//@LABS-STUB
// TODO(3): implement the voice lifecycle:
//   reset()   - clear every field back to its default,
//   key_on(r) - store r, reset the decoder + loop address, set
//               cur_addr_ = start_addr << 3, mark active, empty block buffer
//               so the first tick fetches a fresh block,
//   key_off() - clear active.
// Stub keeps the voice silent-but-active so the suite compiles RED.
inline void Voice::reset() {
    // TODO(3): clear voice state.
}

inline void Voice::key_on(const VoiceRegs&) {
    // TODO(3): load registers and begin playback.
    active_ = true;
}

inline void Voice::key_off() {
    // TODO(3): stop output.
}
//@LABS-END

//@LABS-BEGIN 4
//@LABS-SOLUTION
inline void Voice::fetch_block(const SpuRam& ram) {
    block_ = decoder_.decode_block(&ram.data[cur_addr_]);
    if (block_.flags.loop_start) loop_addr_ = cur_addr_;
    if (block_.flags.mute) {
        muted_ = true;
        active_ = false;
    }
}

inline int16_t Voice::tick(const SpuRam& ram) {
    if (!active_) return 0;
    if (sample_idx_ >= 28) {
        if (muted_) return 0;
        if (block_.flags.loop_end && !loop_addr_) {
            active_ = false;
            return 0;
        }
        if (block_.flags.loop_end) {
            cur_addr_ = *loop_addr_;
            sample_idx_ = 28;  // re-fetch from the loop point
        } else {
            sample_idx_ = 28;
        }
        if (sample_idx_ >= 28) {
            fetch_block(ram);
            if (muted_) return 0;
            sample_idx_ = 0;
            cur_addr_ += 16;
        }
    }
    return block_.samples[sample_idx_++];
}
//@LABS-STUB
// TODO(4): implement block walking.
//   fetch_block(): decode the 16 bytes at cur_addr_; a loop_start block
//     records cur_addr_ as the repeat address; a mute block silences the
//     voice entirely (including its own block).
//   tick(): inactive voices return 0; when the 28-sample buffer is spent:
//     - loop_end with a recorded address -> rewind cur_addr_ to it;
//     otherwise advance cur_addr_ by 16, fetch, restart the buffer index.
// Stub returns silence so the suite runs RED until finished.
inline int16_t Voice::tick(const SpuRam&) {
    // TODO(4): walk blocks through SPU RAM.
    return 0;
}
//@LABS-END

}  // namespace spu
