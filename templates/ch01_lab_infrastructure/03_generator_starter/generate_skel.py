#!/usr/bin/env python3
"""generate_skel.py — YOUR simplified skeleton generator (ch01 starter).

This is the student-built counterpart of tools/labs/generate.py. It applies
the same @LABS marker grammar (see docs/AUTHORING.md) to fixture templates
under this script's data/ directory and writes generated trees plus a
deterministic manifest.json.

Usage:
    generate_skel.py --list
    generate_skel.py --template NAME [--todo N | --mode solution] --out DIR
    generate_skel.py --template-dir PATH [--todo N | --mode solution] --out DIR

Contract highlights (full contract in SPEC.md):
    - files containing an @LABS or %LABS marker block are transformed,
      everything else is copied verbatim;
    - skel mode with --todo T emits SOLUTION for block seq <= T, STUB
      otherwise; without --todo every block emits its STUB; --mode solution
      emits every SOLUTION;
    - manifest.json lists every output file with a sha256, sorted by path,
      and must be byte-identical across repeated runs on the same inputs.
"""

import argparse
import hashlib
import json
import re
import sys
from dataclasses import dataclass, field
from pathlib import Path

GENERATOR = "student-gen 1.0"

# Marker grammar shared with tools/labs/generate.py — comment prefix may be
# // # ; ! * or ".  Fixture templates shipped under data/ store their
# markers with a '%' sentinel ('%LABS-BEGIN') instead of '@LABS-BEGIN' so
# the repository-level generator copies them through verbatim; the two
# spellings are semantically identical and this generator accepts both.
MARKER_RE = re.compile(
    r"^\s*[/#;!*\"]*\s*[@%]LABS-(BEGIN[ \t]+(\d+)|SOLUTION|STUB|END)[ \t]*$"
)


class TemplateError(Exception):
    pass


@dataclass
class Block:
    seq: int
    solution: list = field(default_factory=list)
    stub: list = field(default_factory=list)


def discover_template(templates_root: Path, name: str) -> Path:
    """Resolve NAME to templates_root/NAME."""
//@LABS-BEGIN 1
//@LABS-SOLUTION
    cand = templates_root / name
    if not cand.is_dir():
        known = ", ".join(sorted(p.name for p in templates_root.iterdir()
                                 if p.is_dir())) or "(none)"
        raise SystemExit(f"error: unknown template '{name}' "
                         f"(available: {known})")
    return cand
//@LABS-STUB
    # TODO(1): return templates_root/name, raising SystemExit("error: ...")
    # when it is not an existing directory.
    raise SystemExit(f"TODO(1): discover template '{name}'")
//@LABS-END


def copy_verbatim(src: Path, dst: Path) -> None:
    """Byte-exact copy of one non-template file, creating parents."""
//@LABS-BEGIN 2
//@LABS-SOLUTION
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_bytes(src.read_bytes().replace(b"\r\n", b"\n"))
//@LABS-STUB
    # TODO(2): create dst's parent directories, then copy src byte-exact.
    raise SystemExit("TODO(2): implement copy_verbatim")
//@LABS-END


def parse_template(text: str, path: Path):
    """Parse into ordered segments: ("text", [lines]) / ("block", Block).

    Positional structure is preserved so a line after a block stays after it
    in every generated variant. Malformed markers raise TemplateError.
    """
//@LABS-BEGIN 3
//@LABS-SOLUTION
    segments = []
    state = "outside"  # outside | header | in_solution | in_stub
    block = None
    pending = []

    def flush():
        if pending:
            segments.append(("text", list(pending)))
            pending.clear()

    for lineno, line in enumerate(text.splitlines(), 1):
        m = MARKER_RE.match(line)
        if not m:
            if state == "outside":
                pending.append(line)
            elif state == "in_solution":
                block.solution.append(line)
            else:
                block.stub.append(line)
            continue
        tag = m.group(1)
        if tag.startswith("BEGIN"):
            if state != "outside":
                raise TemplateError(f"{path}:{lineno}: nested @LABS-BEGIN")
            flush()
            block = Block(seq=int(tag.split()[1]))
            segments.append(("block", block))
            state = "header"
        elif tag == "SOLUTION":
            if state not in ("header", "in_stub"):
                raise TemplateError(f"{path}:{lineno}: misplaced SOLUTION")
            state = "in_solution"
        elif tag == "STUB":
            if state not in ("header", "in_solution"):
                raise TemplateError(f"{path}:{lineno}: misplaced STUB")
            state = "in_stub"
        else:  # END
            if state not in ("in_solution", "in_stub"):
                raise TemplateError(f"{path}:{lineno}: END without body")
            state = "outside"
            block = None

    if state != "outside":
        raise TemplateError(f"{path}: unterminated @LABS block")
    flush()
    return segments
//@LABS-STUB
    # TODO(3): walk text.splitlines() matching MARKER_RE; collect verbatim
    # lines between blocks and SOLUTION/STUB bodies inside blocks, keeping
    # positional order. Malformed sequences raise TemplateError.
    raise SystemExit("TODO(3): implement parse_template")
//@LABS-END


def render(segments, todo):
    """Emit final text: block seq <= todo gets SOLUTION, others STUB.

    todo=None emits every STUB.
    """
//@LABS-BEGIN 4
//@LABS-SOLUTION
    out = []
    for kind, payload in segments:
        if kind == "text":
            out.extend(payload)
            continue
        solved = todo is not None and payload.seq <= todo
        out.extend(payload.solution if solved else payload.stub)
    return "\n".join(out) + "\n"
//@LABS-STUB
    # TODO(4): emit text segments verbatim; each block emits SOLUTION lines
    # iff todo is not None and seq <= todo, otherwise its STUB lines.
    raise SystemExit("TODO(4): implement render")
//@LABS-END


def build_manifest(mode, todo, template_name, entries):
    """Serialize the deterministic manifest written next to the outputs.

    entries: iterable of (relative output path, action, sha256-hex).
    """
//@LABS-BEGIN 5
//@LABS-SOLUTION
    manifest = {
        "generator": GENERATOR,
        "mode": mode,
        "todo": todo,
        "template": template_name,
        "files": [
            {"path": rel, "action": action, "sha256": digest}
            for rel, action, digest in sorted(entries)
        ],
    }
    return json.dumps(manifest, indent=2) + "\n"
//@LABS-STUB
    # TODO(5): build {"generator","mode","todo","template","files":[...]}
    # with files sorted by path, serialized as json.dumps(indent=2)+"\n".
    raise SystemExit("TODO(5): implement build_manifest")
//@LABS-END


def run(args) -> int:
    here = Path(__file__).resolve().parent
    templates_root = here / "data"

    if args.list:
        for p in sorted(templates_root.iterdir()):
            if p.is_dir():
                print(p.name)
        return 0

    if not args.out:
        raise SystemExit("error: --out is required unless --list is given")

    template_name = None
    if args.template:
        src_root = discover_template(templates_root, args.template)
        template_name = args.template
    elif args.template_dir:
        src_root = Path(args.template_dir).resolve()
        if not src_root.is_dir():
            raise SystemExit(
                f"error: template dir '{args.template_dir}' not found")
    # Solution mode solves every block; skel mode uses --todo (or none,
    # meaning every block stays a stub).
    todo = args.todo if args.mode == "skel" else 10**9

    out_root = Path(args.out)
    entries = []
    for src in sorted(p for p in src_root.rglob("*") if p.is_file()):
        rel = src.relative_to(src_root).as_posix()
        dst = out_root / rel
        text = src.read_text(encoding="utf-8", errors="replace")
        if "@LABS-BEGIN" in text or "%LABS-BEGIN" in text:
            body = render(parse_template(text, src), todo)
            dst.parent.mkdir(parents=True, exist_ok=True)
            dst.write_bytes(body.replace("\r\n", "\n").encode("utf-8"))
            action = "skeleton" if args.mode == "skel" else "solution"
        else:
            copy_verbatim(src, dst)
            action = "copy"
        entries.append(
            (rel, action, hashlib.sha256(dst.read_bytes()).hexdigest()))

    manifest_path = out_root / "manifest.json"
    manifest_text = build_manifest(args.mode, args.todo, template_name, entries)
    manifest_path.write_bytes(manifest_text.replace("\r\n", "\n").encode("utf-8"))
    print(f"[student-gen] {args.mode} '{template_name}' -> {out_root} "
          f"({len(entries)} files)")
    return 0


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description="student-built skeleton generator (ch01 starter)")
    ap.add_argument("--list", action="store_true",
                    help="list fixture templates under data/")
    ap.add_argument("--template", metavar="NAME",
                    help="fixture template name under data/")
    ap.add_argument("--template-dir", metavar="PATH",
                    help="arbitrary template directory")
    ap.add_argument("--todo", type=int, default=None,
                    help="resume level: blocks with seq <= TODO are solved")
    ap.add_argument("--mode", choices=("skel", "solution"), default="skel",
                    help="emission mode (default: skel)")
    ap.add_argument("--out", default=None, metavar="DIR",
                    help="output directory (created on demand)")
    args = ap.parse_args(argv)

    try:
        return run(args)
    except TemplateError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
