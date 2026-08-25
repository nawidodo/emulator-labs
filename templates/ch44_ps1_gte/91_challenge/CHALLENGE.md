# Challenge — ch44: GTE conformance run

Real GTE bugs hide in corners: a vertex exactly at the clip plane, an LM
command fed negative normals, a depth of zero. The conformance harness in
this directory replays register snapshots through your COP2 and compares
the FULL architectural result — MAC0..3, IR0..3, SZ3, SXY2 and FLAG.

## Format

Input records (`gte_conf.hpp` header documents the grammar):

```
OP=RTPS SF=1 LM=0
CREG=0:00001000            <- control write (hex)
REG=9:000003E8             <- data write (hex)
RUN
```

Registers persist between records. Each RUN emits one line:

```
out mac0=... mac1=... mac2=... mac3=... ir0=... ir1=... ir2=... ir3=...
    sz3=... sxy2=... flag=...
```

## Your task

1. Make `run_file` parse the format and drive the ops.
2. Reproduce the public golden `tests/public/ch44_ps1_gte/conf/pub_expected.txt`
   byte-for-byte from `pub_inputs.txt`.
3. The hidden grader replays a different snapshot file against the same
   binary and hashes the output — every FLAG bit must match, including
   sticky accumulation across RTPT steps.

## Acceptance

```bash
ch44_91_conf_tests                       # unit test GREEN
ch44_conf_runner --inputs tests/public/ch44_ps1_gte/conf/pub_inputs.txt
# output identical to pub_expected.txt
```
