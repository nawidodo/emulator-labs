#!/usr/bin/env python3
"""Hidden-test grader for emulator-labs.

Reads tests/hidden/<chapter>/manifest.json:

{
  "description": "ch02 hidden coding test",
  "cases": [
    {"name": "add_wraps",
     "binary": "skels/ch02_fictional_core/01_core/core_tests",
     "args": ["fictional-hidden"],
     "expect_exit": 0},
    {"name": "trace_golden",
     "binary": "build/skels/.../runner",
     "args": ["--rom", "tests/hidden/ch02/roms/prog.bin",
              "--cycles", "500", "--trace", "{{tmp}}/t.log"],
     "expect_file_hash": {"file": "{{tmp}}/t.log", "fnv64": "ABCD1234..."}},
    {"name": "hardware_suite",
     "binary": "...", "args": [...],
     "requires_rom": "roms/gb/mooneye/timer.bin",   // student-supplied, skip if absent
     "optional": true}
  ]
}

{{tmp}} expands to a per-case temp dir. Exit code 0 iff every non-skipped,
non-optional-failed case passes. Results also land in .labs/grade-last.json.

Grading modes:
  --pipeline           grade tests/pipeline/ self-checks (author/CI)
  --allow-missing-env  skip required_env cases whose binary is unset
                       (author sweeps) instead of failing them

Failure diagnostics are preserved under <repo>/.labs/failures/<chapter>/<case>/
so debugging never depends on terminal scrollback (v004 P0.1).
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
import tempfile
from pathlib import Path

FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
MASK = 0xFFFFFFFFFFFFFFFF

ALLOW_MISSING_ENV = False   # set by main()

# Current context (for failure artifact paths); set by the sweep loop.
CURRENT = {"chapter": "", "case": ""}


def fnv1a(data: bytes) -> str:
    h = FNV_OFFSET
    for b in data:
        h ^= b
        h = (h * FNV_PRIME) & MASK
    return f"{h:016X}"


def save_failure(repo: Path, reason: str, *,
                 stdout: str = "", stderr: str = "",
                 extra_files: dict[str, str | bytes] | None = None) -> None:
    """Persist failure diagnostics under <repo>/.labs/failures/<chapter>/<case>/.

    Signature-stable single helper (v004 §33). Best-effort: never raises.
    Binary artifacts are stored with write_bytes; text with write_text.
    """
    try:
        d = (repo / ".labs" / "failures"
             / CURRENT.get("chapter", "_") / CURRENT.get("case", "_"))
        d.mkdir(parents=True, exist_ok=True)
        (d / "reason.txt").write_text(reason + "\n")
        if stdout:
            (d / "stdout.txt").write_text(stdout)
        if stderr:
            (d / "stderr.txt").write_text(stderr)
        for name, content in (extra_files or {}).items():
            if content is None:
                continue
            if isinstance(content, bytes):
                (d / name).write_bytes(content)
            else:
                (d / name).write_text(str(content))
    except OSError:
        pass


def fail(repo: Path, msg: str, *, proc: subprocess.CompletedProcess
         | None = None,
         extra_files: dict[str, str | bytes] | None = None) -> tuple[bool, str]:
    """Centralized failure return: persists diagnostics, returns (False, msg)
    (v004 §34)."""
    save_failure(repo, msg,
                 stdout=getattr(proc, "stdout", "") or "",
                 stderr=getattr(proc, "stderr", "") or "",
                 extra_files=extra_files)
    return False, msg


def run_case(repo: Path, case: dict) -> tuple[bool, str]:
    tmp = Path(tempfile.mkdtemp(prefix="labs-grade-"))

    def expand(s: str) -> str:
        """Expand {{tmp}} and {{env:NAME}} placeholders in manifest strings."""
        s = s.replace("{{tmp}}", str(tmp))
        while "{{env:" in s:
            head, _, rest = s.partition("{{env:")
            name, _, tail = rest.partition("}}")
            s = head + os.environ.get(name, "") + tail
        return s

    binary_raw = expand(case["binary"])
    if not binary_raw.strip():
        # Learner gates are HARD: a missing integration binary fails.
        # Author/pipeline sweeps (--allow-missing-env) skip instead.
        if case.get("required_env") and not ALLOW_MISSING_ENV:
            return fail(repo, "required_env binary unset — the learner gate "
                              "must FAIL, not skip, when the integrated "
                              "binary is absent")
        return True, ("SKIPPED ({{env:...}} in manifest 'binary' is unset — "
                      "set it to your integrated binary path)")
    binary = Path(binary_raw)
    if not binary.is_absolute():
        binary = repo / binary
    if not binary.exists():
        # Optional hardware-suite cases may reference binaries that only
        # make sense together with a student-supplied ROM; skip beats
        # 'binary missing' for those.
        if case.get("requires_rom") and case.get("optional"):
            return True, (f"SKIPPED (student-supplied ROM absent: "
                          f"{case['requires_rom']})")
        return fail(repo, f"binary missing: {case['binary']} "
                          f"(run make build)")
    if "requires_rom" in case and not \
            (repo / expand(case["requires_rom"])).exists():
        return True, ("SKIPPED (student-supplied ROM absent: "
                      f"{case['requires_rom']})")

    args = [expand(str(a)) for a in case.get("args", [])]
    timeout = case.get("timeout", 30)

    try:
        proc = subprocess.run([str(binary)] + args, cwd=str(repo),
                              capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired as exc:
        return fail(repo, f"timed out after {timeout}s",
                    extra_files={"stdout.txt": getattr(exc, "stdout", "") or "",
                                 "stderr.txt": getattr(exc, "stderr", "") or ""})
    if proc.returncode < 0:
        return fail(repo, f"crashed (signal {-proc.returncode})", proc=proc)

    if "expect_exit" in case and proc.returncode != case["expect_exit"]:
        tail = (proc.stdout or proc.stderr or "").strip().splitlines()
        detail = tail[-1] if tail else ""
        return fail(repo, f"exit {proc.returncode} != {case['expect_exit']} "
                          f"{detail[:120]}", proc=proc)
    if "expect_stdout_contains" in case:
        needle = case["expect_stdout_contains"]
        if needle not in proc.stdout:
            return fail(repo, f"stdout missing '{needle}'", proc=proc)
    if "expect_file_exists" in case:
        produced = [expand(rel) for rel in case["expect_file_exists"]]
        missing = [rel for rel in produced if not Path(rel).exists()]
        if missing:
            return fail(repo, "expected file(s) not produced: "
                              f"{', '.join(missing)}", proc=proc)
    if "expect_file_hash" in case:
        spec = case["expect_file_hash"]
        path = Path(expand(spec["file"]))
        if not path.exists():
            return fail(repo, f"expected file not produced: {spec['file']}",
                        proc=proc)
        digest = fnv1a(path.read_bytes())
        if digest.upper() != spec["fnv64"].upper():
            return fail(repo, f"hash mismatch: got FNV64 {digest}",
                        extra_files={"produced.bin":
                                     path.read_bytes() if path.exists()
                                     else b""})
    return True, "ok"


def expand_tmp(s: str, tmp: Path) -> str:
    return s.replace("{{tmp}}", str(tmp))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--pipeline", action="store_true",
                    help="grade tests/pipeline manifests (author/CI "
                         "self-checks) instead of learner gates")
    ap.add_argument("--allow-missing-env", action="store_true",
                    help="skip required_env cases whose binary is unset "
                         "(author sweeps) instead of failing them")
    ap.add_argument("targets", nargs="*",
                    help="chapter dirs under tests/hidden (or pipeline)")
    args = ap.parse_args()
    global ALLOW_MISSING_ENV
    ALLOW_MISSING_ENV = args.allow_missing_env

    repo = Path(args.repo).resolve()
    hidden_root = repo / "tests" / "hidden"
    base = (repo / "tests" / "pipeline") if args.pipeline else hidden_root
    if args.targets:
        chapters_dirs = [base / t for t in args.targets]
    else:
        chapters_dirs = sorted(p for p in base.iterdir() if p.is_dir())

    results = []
    failed_chapters = []
    for chdir in chapters_dirs:
        CURRENT["chapter"] = chdir.name
        mf = chdir / "manifest.json"
        if not mf.is_file():
            continue
        spec = json.loads(mf.read_text())
        print(f"\n=== grade {chdir.name}: {spec.get('description', '')} ===")
        ch_failed = False
        for case in spec.get("cases", []):
            CURRENT["case"] = case.get("name", "case")
            ok, msg = run_case(repo, case)
            skipped = msg.startswith("SKIPPED")
            status = "SKIP" if skipped else ("PASS" if ok else "FAIL")
            if skipped:
                ok = True
            elif not ok:
                ch_failed = True
            print(f"  [{status:>4}] {case['name']}: {msg}")
            results.append({"chapter": chdir.name, "case": case["name"],
                            "pass": ok, "note": msg,
                            "skipped": skipped})
        if ch_failed:
            failed_chapters.append(chdir.name)

    out = repo / ".labs" / "grade-last.json"
    out.parent.mkdir(exist_ok=True)
    out.write_text(json.dumps({"results": results}, indent=2))

    passed = sum(1 for r in results if r["pass"] and not r.get("skipped"))
    skipped = sum(1 for r in results if r.get("skipped"))
    failed = sum(1 for r in results if not r["pass"])
    print(f"\n== grade summary: {passed} passed / {skipped} skipped / "
          f"{failed} failed (of {len(results)}) ==")
    if failed_chapters:
        print(f"failing chapters: {', '.join(failed_chapters)}")
    return 1 if failed_chapters else 0


if __name__ == "__main__":
    sys.exit(main())