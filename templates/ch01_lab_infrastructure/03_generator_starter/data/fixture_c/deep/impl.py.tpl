# fixture_c — nested file: checkpoint 2 sits below a subdirectory, and text
# follows the final block; the generator must keep both facts intact.

def frame_cycles(cycles_per_frame):
%LABS-BEGIN 2
%LABS-SOLUTION
    return cycles_per_frame * 60  # one second of frames
%LABS-STUB
    # TODO(2): return one second worth of cycles at 60 Hz
    return 0
%LABS-END

print("fixture_c ready:", frame_cycles(1) == 60)
