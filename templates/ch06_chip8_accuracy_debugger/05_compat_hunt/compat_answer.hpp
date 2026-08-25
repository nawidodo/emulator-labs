#pragma once
// Your answer to the compat hunt lives here (see HUNT.md).
//
// Record the PC of the FIRST trace line where the wrong-profile execution
// shows a register/memory value inconsistent with the reference trace.
// The unit test re-derives the true first divergence by simulation, so an
// honest answer passes and a guess does not.

#include <cstdint>

namespace ch06 {

//@LABS-BEGIN 1
//@LABS-SOLUTION
inline constexpr uint16_t kFirstDivergencePc = 0x0206;
//@LABS-STUB
// TODO(1): set this to the PC where your debugging session located the
// first divergence from the reference trace.
inline constexpr uint16_t kFirstDivergencePc = 0x0000;
//@LABS-END

}  // namespace ch06
