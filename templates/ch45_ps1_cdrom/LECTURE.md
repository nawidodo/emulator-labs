# Chapter 45 — PS1 CD-ROM

The CD-ROM subsystem is the PlayStation's long-latency periphery: a
command/response controller talking to a drive whose answers arrive
hundreds of thousands of cycles later. Emulating it synchronously breaks
games; the reference design here is asynchronous and event-driven.

## Disc images: BIN/CUE

A `.bin` holds raw 2352-byte sectors; the `.cue` sheet maps tracks and
indices onto it. This lab parses single-track MODE2/2352 cues:

```
FILE "game.bin" BINARY
  TRACK 01 MODE2/2352
    INDEX 01 00:02:00
```

Raw sector layout:

```
[0..11]   sync 00 FF*10 00
[12..14]  address MSF as BCD      [15] mode (02 = MODE2)
[16..19]  subheader (MODE2): byte 18 bit 5 selects FORM2
[24..]    user data: form1 -> 2048 bytes; form2 -> 2324 bytes of XA
          ADPCM audio — passthrough only, out of scope in this lab
```

Sector header validity matters: the embedded BCD minute/second/frame must
match the sector's LBA, otherwise games "read" garbage silently.

## MSF <-> LBA

LBA numbering starts at the first data sector (MSF 00:02:00):

```
lba = (minute*60 + second)*75 + frame - 150
```

Forgetting the -150 lead-in bias is one of the seeded bugs in this
chapter's debug exercise — every seek lands exactly 150 sectors off.

## Controller protocol

Commands are issued by pushing BCD parameter bytes and writing an opcode;
the drive responds through a FIFO and raises one of five interrupt levels:

| Level | Meaning                                   |
|-------|-------------------------------------------|
| INT3  | first response (command accepted)         |
| INT2  | second response (operation complete)      |
| INT1  | data ready / streaming event              |
| INT5  | error                                     |

Implemented commands:

| Opcode | Command  | Timing model (guest ticks)                    |
|--------|----------|-----------------------------------------------|
| 01h    | GetStat  | immediate INT3                                 |
| 02h    | Setloc   | immediate INT3, stores target LBA              |
| 09h    | Pause    | INT3 now; INT2 after 250; aborts streaming     |
| 0Ah    | Init     | INT3 now (motor on); INT2 after 1200 spin-up   |
| 15h    | SeekL    | INT3 now; INT2 after `100 + |delta|`           |
| 06h    | ReadN    | INT3 now; first INT1 after seek + 1 sector     |
| ("ReadS") | speed | Setmode-style double-speed halves sector time  |

STAT bits used: `01 error`, `02 motor on`, `10 shell open`,
`20 reading`, `40 seeking`. The linear seek model is deliberately simple
but deterministic; document whatever you change.

## Event-driven structure

The controller owns a monotonic guest clock. Every deferred action is an
event `(deadline, sequence, callback)`; `tick n` advances time and fires
due events in deadline order. Streaming reads carry an EPOCH counter:
Pause bumps it so already-scheduled sector deliveries become no-ops
instead of ghost interrupts.

## Debugging workflow

Transcript-first (curriculum §54): run a scripted session, diff your
`<t= int= resp=>` log against the golden line-by-line, and find the FIRST
divergent timestamp or response byte. The hidden grader replays unseen
scripts and hashes transcripts.
