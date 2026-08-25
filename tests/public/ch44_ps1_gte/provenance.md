# ch44_ps1_gte — fixture provenance

All conformance snapshots are synthetic register writes authored by hand;
no copyrighted data embedded.

## conf/pub_inputs.txt + pub_expected.txt
Nine records covering: clean projection, IR positive/negative saturation,
LM unsigned clamping, divide overflow (SZ3==0), RTPT FIFO sequencing,
AVSZ3 depth averaging, MVMVA operand selection and NCDS lighting.
Golden produced by the reference solution:

    build/skels/ch44_ps1_gte/91_challenge/ch44_conf_runner \
        --inputs tests/public/ch44_ps1_gte/conf/pub_inputs.txt \
        --outputs pub_expected.txt

Executed twice on the reference tree; outputs byte-identical.
