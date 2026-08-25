#!/usr/bin/env python3
"""Lab skeleton generator for emulator-labs.

Turns fully-annotated template sources into student skeletons (with TODO
stubs) or reference solutions, mirroring the Linux Kernel Labs workflow.

Template marker grammar (language-agnostic; works in C++/Python/Make/CMake):

    //@LABS-BEGIN 3        <- any of // # ; ! * as comment prefix
    //@LABS-SOLUTION
    ...full implementation...
    //@LABS-STUB
    ...stub containing TODO(3)...
    //@LABS-END

Emission rules for mode=skel with target level T (--todo T):
    block seq <= T   -> emit SOLUTION lines (already-completed checkpoints)
    block seq >  T   -> emit STUB lines
    no --todo        -> every block emits its STUB
mode=solution emits SOLUTION lines for every block.

Files without markers are copied verbatim. A manifest JSON records what was
generated so grading and coding tests can hash-compare outputs.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import shutil
import sys
from dataclasses import dataclass, field
from pathlib import Path

GENERATOR_VERSION = "1.0"

MARKER_RE = re.compile(
    r"^\s*[/#;!*\"]*\s*@LABS-(BEGIN[ \t]+(\d+)|SOLUTION|STUB|END)[ \t]*$"
)

DOC_PASSTHROUGH = ("LECTURE.md", "README.md", "EXERCISES.md", "CHALLENGE.md",
                   "DEBUGGING.md", "CODING_TEST.md", "SPEC.md")


@dataclass
class Block:
    seq: int
    solution: list[str] = field(default_factory=list)
    stub: list[str] = field(default_factory=list)


class TemplateError(Exception):
    pass


def parse_template(text: str, path: Path) -> list[tuple]:
    """Return ordered segments: ("text", [lines]) and ("block", Block).

    Positional structure is preserved so text after a block (namespace
    closers, #endif, ...) stays after it in every generated variant.
    """
    segments: list[tuple] = []
    state = "outside"          # outside | header | in_solution | in_stub
    current: Block | None = None
    pending_text: list[str] = []

    def flush_text() -> None:
        if pending_text:
            segments.append(("text", list(pending_text)))
            pending_text.clear()

    for lineno, line in enumerate(text.splitlines(), 1):
        m = MARKER_RE.match(line)
        if not m:
            if state == "outside":
                pending_text.append(line)
            elif state == "in_solution":
                assert current is not None
                current.solution.append(line)
            else:
                assert current is not None
                current.stub.append(line)
            continue

        tag = m.group(1)
        if tag.startswith("BEGIN"):
            if state != "outside":
                raise TemplateError(f"{path}:{lineno}: nested @LABS-BEGIN")
            flush_text()
            current = Block(seq=int(tag.split()[1]))
            segments.append(("block", current))
            state = "header"
        elif tag == "SOLUTION":
            if state not in ("header", "in_stub"):
                raise TemplateError(f"{path}:{lineno}: @LABS-SOLUTION misplaced")
            state = "in_solution"
        elif tag == "STUB":
            if state not in ("header", "in_solution"):
                raise TemplateError(f"{path}:{lineno}: @LABS-STUB misplaced")
            state = "in_stub"
        elif tag == "END":
            if state not in ("in_solution", "in_stub"):
                raise TemplateError(f"{path}:{lineno}: @LABS-END without body")
            if current is None or not (current.solution or current.stub):
                raise TemplateError(f"{path}:{lineno}: empty @LABS block")
            state = "outside"
            current = None

    if state != "outside":
        raise TemplateError(f"{path}: unterminated @LABS block")
    flush_text()

    blocks = [b for kind, b in segments if kind == "block"]
    seqs = [b.seq for b in blocks]
    if len(seqs) != len(set(seqs)):
        dupes = sorted({s for s in seqs if seqs.count(s) > 1})
        raise TemplateError(f"{path}: duplicate block seq {dupes}")
    return segments


def render(segments: list[tuple], todo: int | None,
           path_for_warn: Path) -> str:
    out: list[str] = []
    for kind, payload in segments:
        if kind == "text":
            out.extend(payload)
            continue
        b: Block = payload
        solved = todo is not None and b.seq <= todo
        body = b.solution if solved else b.stub
        if path_for_warn is not None and not solved and b.stub \
                and "TODO" not in "".join(b.stub):
            print(f"warning: {path_for_warn}: block {b.seq} stub has no "
                  f"TODO marker", file=sys.stderr)
        out.extend(body)
    return "\n".join(out) + "\n"


def resolve_target(repo: Path, target: str) -> Path:
    """Accept 'ch03_slug', 'ch03', 'ch03_slug/02_fetch' or 'ch03/02'."""
    cand = repo / "templates" / target
    if cand.is_dir():
        return cand
    parts = target.split("/")
    tdir = repo / "templates"
    matched = None
    for p in sorted(tdir.iterdir()):
        if p.is_dir() and (p.name == parts[0] or p.name.split("_", 1)[0] == parts[0]):
            matched = p
            break
    if matched is None:
        raise SystemExit(f"error: unknown target '{target}' "
                         f"(no template dir matches '{parts[0]}')")
    if len(parts) == 1:
        return matched
    sub = repo / "templates" / matched.name / parts[1]
    if not sub.is_dir():
        for p in sorted((repo / "templates" / matched.name).iterdir()):
            if p.is_dir() and (p.name == parts[1] or
                               p.name.split("_", 1)[0] == parts[1]):
                sub = repo / "templates" / matched.name / p.name
                break
    if not sub.is_dir():
        raise SystemExit(f"error: exercise '{parts[1]}' not found in {matched.name}")
    return sub


def copy_docs(template_root: Path, chapter_dir: Path, dest_chapter: Path,
              manifest: dict, force: bool) -> None:
    """Copy chapter-level lecture/readme/docs alongside an exercise skel."""
    for name in DOC_PASSTHROUGH:
        src = chapter_dir / name
        if src.is_file() and chapter_dir == template_root / chapter_dir.name:
            dst = dest_chapter / name
            _copy(src, dst, manifest, force)
    docs_src = chapter_dir / "docs"
    if docs_src.is_dir():
        for src in docs_src.rglob("*"):
            if src.is_file():
                rel = src.relative_to(chapter_dir)
                _copy(src, dest_chapter / rel, manifest, force)


def _copy(src: Path, dst: Path, manifest: dict, force: bool,
          transform: bool = False, todo: int | None = None,
          warn_todo: bool = True) -> str | None:
    if dst.exists() and not force:
        return "skipped"
    dst.parent.mkdir(parents=True, exist_ok=True)
    if transform:
        segments = parse_template(src.read_text(), src)
        text = render(segments, todo, src if warn_todo else None)
        dst.write_text(text)
        action = "skeleton" if todo is None else f"skeleton(todo={todo})"
    else:
        # copyfile, not copy2: fresh mtimes so incremental rebuilds after
        # regeneration never silently skip recompiles.
        shutil.copyfile(src, dst)
        action = "copy"
    manifest["files"].append({
        "template": str(src),
        "output": str(dst),
        "action": action,
        "sha256": hashlib.sha256(dst.read_bytes()).hexdigest(),
    })
    return action


def generate(repo: Path, target: str, todo: int | None, mode: str,
             force: bool, out_root: Path | None) -> Path:
    resolved = resolve_target(repo, target)
    templates_root = repo / "templates"

    # Chapter root + relative position of the generated subtree.
    rel = resolved.relative_to(templates_root)
    chapter_name = rel.parts[0]
    chapter_dir = templates_root / chapter_name
    sub_rel = rel.relative_to(chapter_name)

    if mode == "solution":
        base = out_root if out_root else repo / "solutions"
    else:
        base = out_root if out_root else repo / "skels"
    dest = base / rel

    manifest: dict = {
        "generator": GENERATOR_VERSION,
        "mode": mode,
        "target": target,
        "chapter": chapter_name,
        "files": [],
    }
    for src in sorted(resolved.rglob("*")):
        if not src.is_file() or "__pycache__" in src.parts:
            continue
        rel_path = src.relative_to(resolved)
        dst = dest / rel_path
        text = src.read_text(errors="replace")
        has_markers = "@LABS-BEGIN" in text
        if has_markers and src.suffix in {
                ".cpp", ".cc", ".cxx", ".h", ".hpp", ".py", ".mk", ".cmake",
                ".txt", ".json", ".md"}:
            # Same filter in BOTH modes so marker-bearing fixtures (data
            # files) are never flattened by --mode solution.
            # Bugged-stub exercises (90_debug, 99_coding_test) intentionally
            # ship bugs rather than TODO markers — exempt from the warning.
            exempt = ("90_debug" in rel_path.as_posix()
                      or "99_coding_test" in rel_path.as_posix())
            _copy(src, dst, manifest, force, transform=True,
                  todo=(10**9 if mode == "solution" else todo),
                  warn_todo=not exempt)
        else:
            _copy(src, dst, manifest, force)

    # Chapter context docs for exercise-level targets.
    if sub_rel.parts and chapter_dir == templates_root / chapter_name:
        copy_docs(templates_root, chapter_dir, dest.parent, manifest, force)
    (dest / ".labs-manifest.json").write_text(json.dumps(manifest, indent=2))
    n = len(manifest["files"])
    kind = "solution" if mode == "solution" else "skeleton"
    print(f"[generate] {kind} '{target}' -> {dest} ({n} files)")
    return dest


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--repo", default=None, help="repository root "
                    "(default: two levels up from this script)")
    ap.add_argument("--targets", nargs="+", metavar="TARGET",
                    help="chapter or chapter/exercise targets")
    ap.add_argument("--list", action="store_true", help="list available targets")
    ap.add_argument("--todo", type=int, default=None,
                    help="resume level: blocks with seq <= TODO emit solutions")
    ap.add_argument("--mode", choices=("skel", "solution"), default="skel")
    ap.add_argument("--force", action="store_true",
                    help="overwrite existing output files")
    ap.add_argument("--out", default=None, help="alternate output root")
    args = ap.parse_args(argv)

    repo = Path(args.repo).resolve() if args.repo else \
        Path(__file__).resolve().parents[2]

    if args.list:
        for ch in sorted((repo / "templates").glob("ch*")):
            exercises = sorted(p.name for p in ch.iterdir()
                               if p.is_dir() and p.name[0].isdigit())
            print(ch.name)
            for e in exercises:
                print(f"  {ch.name}/{e}")
        return 0

    if not args.targets:
        ap.error("no --targets given (or use --list)")

    failures = 0
    for target in args.targets:
        try:
            generate(repo, target, args.todo, args.mode, args.force,
                     Path(args.out).resolve() if args.out else None)
        except (TemplateError, SystemExit) as exc:
            print(f"error: {exc}", file=sys.stderr)
            failures += 1
    return 1 if failures else 0

if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:
        sys.exit(0)
