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
"""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

FNV_OFFSET = 0xCBF29CE484222325
FNV_PRIME = 0x100000001B3
MASK = 0xFFFFFFFFFFFFFFFF


def fnv1a(data: bytes) -> str:
    h = FNV_OFFSET
    for b in data:
        h ^= b
        h = (h * FNV_PRIME) & MASK
    return f"{h:016X}"


def run_case(repo: Path, case: dict) -> tuple[bool, str]:
    import os
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
        if case.get("required_env"):
            return False, ("required_env binary unset — the learner gate "
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
        return False, f"binary missing: {case['binary']} (run make build)"
    if "requires_rom" in case and not \
            (repo / expand(case["requires_rom"])).exists():
        return True, ("SKIPPED (student-supplied ROM absent: "
                      f"{case['requires_rom']})")

    args = [expand(str(a)) for a in case.get("args", [])]
    timeout = case.get("timeout", 30)
    try:
        proc = subprocess.run([str(binary)] + args, cwd=str(repo),
                              capture_output=True, text=True, timeout=timeout)
    except subprocess.TimeoutExpired:
        return False, f"timed out after {timeout}s"

    if "expect_exit" in case and proc.returncode != case["expect_exit"]:
        tail = (proc.stdout or proc.stderr or "").strip().splitlines()
        detail = tail[-1] if tail else ""
        return False, (f"exit {proc.returncode} != {case['expect_exit']} "
                       f"{detail[:120]}")
    if "expect_stdout_contains" in case:
        needle = case["expect_stdout_contains"]
        if needle not in proc.stdout:
            return False, f"stdout missing '{needle}'"
    if "expect_file_hash" in case:
        spec = case["expect_file_hash"]
        path = Path(expand(spec["file"]))
        if not path.exists():
            return False, f"expected file not produced: {spec['file']}"
        digest = fnv1a(path.read_bytes())
        if digest.upper() != spec["fnv64"].upper():
            return False, f"hash mismatch: got FNV64 {digest}"
    if proc.returncode < 0:
        return False, "crashed (signal)"
    return True, "ok"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("targets", nargs="*", help="chapter dirs under tests/hidden")
    args = ap.parse_args()
    repo = Path(args.repo).resolve()

    hidden_root = repo / "tests" / "hidden"
    chapters = sorted(p for p in hidden_root.iterdir() if p.is_dir()) \
        if not args.targets else [hidden_root / t for t in args.targets]

    results = []
    failed_chapters = []
    for chdir in chapters:
        mf = chdir / "manifest.json"
        if not mf.is_file():
            continue
        spec = json.loads(mf.read_text())
        print(f"\n=== grade {chdir.name}: {spec.get('description', '')} ===")
        ch_failed = False
        for case in spec.get("cases", []):
            ok, msg = run_case(repo, case)
            status = "PASS" if ok else "FAIL"
            if msg.startswith("SKIPPED"):
                status = "SKIP"
            elif ok:
                pass
            else:
                ch_failed = True
            print(f"  [{status:>4}] {case['name']}: {msg}")
            results.append({"chapter": chdir.name, "case": case["name"],
                            "pass": ok, "note": msg,
                            "skipped": msg.startswith("SKIPPED")})
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
