# seven_level — util fixture: checkpoints 2, 4, 6 ('#'-prefixed blocks)

MASK4 = 0xF


%LABS-BEGIN 2
%LABS-SOLUTION
def nibble_pair(byte):
    return (byte >> 4, byte & MASK4)
%LABS-STUB
def nibble_pair(byte):
    # TODO(2): split byte into (high nibble, low nibble)
    return (0, 0)
%LABS-END


# Verbatim gap between checkpoint 2 and checkpoint 4.


%LABS-BEGIN 4
%LABS-SOLUTION
def join_nibbles(hi, lo):
    return ((hi & MASK4) << 4) | (lo & MASK4)
%LABS-STUB
def join_nibbles(hi, lo):
    # TODO(4): reassemble two nibbles into one byte
    return 0
%LABS-END


%LABS-BEGIN 6
%LABS-SOLUTION
def rotate_left_4(byte):
    return ((byte << 4) | (byte >> 4)) & 0xFF
%LABS-STUB
def rotate_left_4(byte):
    # TODO(6): swap the two nibbles of byte
    return 0
%LABS-END


print("seven_level util loaded")
