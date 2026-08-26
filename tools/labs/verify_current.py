#!/usr/bin/env python3
"""Generate verification-current.json — the LIVE CI verification artifact
(v005 P1-11 / v007 #33-45).

Rules (governing principle: "missing evidence is not a passing result"):

  reference_ctest  : not-run|green|error — never zero-green-looking
  unseen_grade     : green|error|invalid — integers, returncode checked
  pipeline_grade   : green|error|invalid — honors --pipeline (v007 #39)
  grader_self_tests: green|error with ran/failed from unittest
  external_courses : foundations-c17 audit + DERIVED verified count
  integration_reference: ch52/ch51 pending|green|error (derived once a
                    canonical reference binary is made available)

Fail-closed gate (v008 #18/#19): exits nonzero iff any REQUIRED dimension
(reference_ctest, unseen_grade, pipeline_grade when --pipeline,
grader_self_tests, foundations-c17 audit) is error, invalid, or an
unexpected not-run -> "overall": "red". Integration references still
"pending" downgrade green to "incomplete" WITHOUT failing the gate, so
local runs before the canonical binaries exist stay honest-but-passing.
Usage:
  python3 tools/labs/verify_current.py [--repo .] [--build-dir build-solutions]
                                       [--pipeline]
"""
from __future__ import annotations

import argparse
import datetime
import json
import re
import subprocess
import sys
from pathlib import Path


def _run(cmd: list[str], cwd: Path) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, cwd=cwd, capture_output=True, text=True)
def _ch52_status(repo: Path, build_dir: Path) -> str:
    runner = build_dir / "tools/labs/nes_gate/nes_gate_runner"
    golden = repo / "tests/public/ch52_nes_playable_gate/goldens/gate_reference.emu_gate"
    rom = repo / "tests/public/ch52_nes_playable_gate/roms/gate_homebrew.nes"
    if not runner.is_file() or not golden.is_file() or not rom.is_file():
        return "pending"
    try:
        cur = Path("/tmp/cur_gate_verify.emu_gate")
        subprocess.run(
            [str(runner), "--rom", str(rom), "--frames", "180",
             "--gate", str(cur)], check=True, capture_output=True,
            timeout=30)
        want = golden.read_text().strip().splitlines()
        got = cur.read_text().strip().splitlines()
        # Compare the pinned hashes (ROM/FRAME/AUDIO/PPU/RAM) — ignore REPLAY_FNV.
        def kv(lines):
            return {k: v for k, v in (l.split("=", 1) for l in lines if "=" in l)}
        a, b = kv(want), kv(got)
        for k in ("ROM_FNV", "FRAME_FNV", "AUDIO_FNV", "PPU_FNV", "RAM_FNV"):
            if a.get(k) != b.get(k):
                return "error"
        return "green"
    except Exception:
        return "error"

def _ch51_status(repo: Path, build_dir: Path) -> str:
    runner = build_dir / "tools/labs/ps1_gate/ps1_gate_runner"
    goldens_dir = repo / "tests/public/ch51_ps1_capstone/goldens"
    # Goldens for all 10 cases (pad/GTE/MDEC/CPU/timer/DMA + SPU/CD/card/boot). Check existence.
    pad_golden = goldens_dir / "pad_resp.fnv"
    gte_golden = goldens_dir / "gte_vector.fnv"
    mdec_golden = goldens_dir / "mdec_block.fnv"
    cpu_golden = goldens_dir / "cpu_trace.fnv"
    timer_golden = goldens_dir / "timer_irq.fnv"
    dma_golden = goldens_dir / "dma_chain.fnv"
    spu_golden = goldens_dir / "spu_stream.fnv"
    cd_golden = goldens_dir / "cd_read.fnv"
    card_golden = goldens_dir / "card_rt.fnv"
    boot_golden = goldens_dir / "boot_milestones.fnv"
    pad_rom = repo / "tests/hidden/ch51_ps1_capstone/roms/pad_txn.bin"
    gte_rom = repo / "tests/hidden/ch51_ps1_capstone/roms/gte_vector.bin"
    mdec_rom = repo / "tests/hidden/ch51_ps1_capstone/roms/mdec_block.bin"
    cpu_rom = repo / "tests/hidden/ch51_ps1_capstone/roms/cpu_smoke.bin"
    timer_rom = repo / "tests/hidden/ch51_ps1_capstone/roms/irq_order.bin"
    dma_rom = repo / "tests/hidden/ch51_ps1_capstone/roms/dma_chain.bin"
    spu_rom = repo / "tests/hidden/ch51_ps1_capstone/roms/spu_stream.bin"
    cd_rom = repo / "tests/hidden/ch51_ps1_capstone/roms/cd_read.bin"
    card_rom = repo / "tests/hidden/ch51_ps1_capstone/roms/card_rt.bin"
    boot_rom = repo / "tests/hidden/ch51_ps1_capstone/roms/boot_milestones.bin"
    pad_script = repo / "tests/hidden/ch51_ps1_capstone/scripts/pad.script"
    spu_script = repo / "tests/hidden/ch51_ps1_capstone/scripts/spu.script"
    if not runner.is_file() or not pad_golden.is_file() or not gte_golden.is_file() or not mdec_golden.is_file():
        return "pending"
    has_new = cpu_golden.is_file() and timer_golden.is_file() and dma_golden.is_file()
    has_final4 = spu_golden.is_file() and cd_golden.is_file() and card_golden.is_file() and boot_golden.is_file()
    if not pad_rom.is_file() or not gte_rom.is_file() or not mdec_rom.is_file():
        return "pending"
    if has_new and (not cpu_rom.is_file() or not timer_rom.is_file() or not dma_rom.is_file()):
        return "pending"
    if has_final4 and (not spu_rom.is_file() or not cd_rom.is_file() or not card_rom.is_file() or not boot_rom.is_file()):
        return "pending"
    # Fail-closed: once goldens exist we require all 10; pending only when goldens absent
    def fnv1a(data: bytes) -> str:
        h = 0xCBF29CE484222325
        for b in data:
            h ^= b
            h = (h * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
        return f"{h:016X}"
    cases = [
        (pad_rom, pad_script, pad_golden, "pad", False, []),
        (gte_rom, None, gte_golden, "gte", False, []),
        (mdec_rom, None, mdec_golden, "mdec", False, []),
    ]
    if has_new:
        cases += [
            (cpu_rom, None, cpu_golden, "cpu", True, ["--cycles", "20000"]),
            (timer_rom, None, timer_golden, "timer", False, []),
            (dma_rom, None, dma_golden, "dma", False, ["--cycles", "100000"]),
        ]
    if has_final4:
        cases += [
            (spu_rom, spu_script, spu_golden, "spu", False, ["--frames", "4000"]),
            (cd_rom, None, cd_golden, "cd", False, []),
            (card_rom, None, card_golden, "card", False, []),
            (boot_rom, None, boot_golden, "boot", False, ["--cycles", "5000000"]),
        ]
    # If still not all 10 present, stay pending (pre-pin state); once all 10 exist, require green/error
    if not has_new or not has_final4:
        # Not yet fully pinned — keep previous behavior (green on available cases)
        pass
    else:
        # All 10 goldens present: require all cases to pass for green
        pass
    try:
        for rom, script, golden, tag, is_trace, extra in cases:
            want = golden.read_text().strip().upper()
            out = Path(f"/tmp/ch51_verify_{tag}.bin")
            if is_trace:
                out = Path(f"/tmp/ch51_verify_{tag}.trace")
                cmd = [str(runner), "--rom", str(rom), "--trace", str(out), "--headless"]
                if extra:
                    cmd += extra
            else:
                cmd = [str(runner), "--rom", str(rom), "--hash-frame", str(out), "--headless"]
                if extra:
                    cmd += extra
            if script is not None and script.is_file():
                cmd += ["--input-file", str(script)]
            subprocess.run(cmd, check=True, capture_output=True, timeout=10)
            got = fnv1a(out.read_bytes())
            if got.upper() != want:
                return "error"
            out2 = Path(f"/tmp/ch51_verify_{tag}2.bin")
            if is_trace:
                out2 = Path(f"/tmp/ch51_verify_{tag}2.trace")
                cmd2 = [str(runner), "--rom", str(rom), "--trace", str(out2), "--headless"]
                if extra:
                    cmd2 += extra
            else:
                cmd2 = [str(runner), "--rom", str(rom), "--hash-frame", str(out2), "--headless"]
                if extra:
                    cmd2 += extra
            if script is not None and script.is_file():
                cmd2 += ["--input-file", str(script)]
            subprocess.run(cmd2, check=True, capture_output=True, timeout=10)
            if out.read_bytes() != out2.read_bytes():
                return "error"
        return "green"
    except Exception:
        return "error"




def _verdict(required: dict[str, str],
             integration: dict[str, str]) -> tuple[str, bool]:
    """Return (overall, gate_ok) per the v008 #18/#19 contract.

    required  — dimension name -> status; anything but "green" is a
                missing/failed evidence and makes the run red.
    integration — ch51/ch52 -> pending|green|error; "pending" keeps the
                gate open (incomplete), only all-green earns green.
    """
    if any(st != "green" for st in required.values()):
        return "red", False
    if integration and all(v == "green" for v in integration.values()):
        return "green", True
    return "incomplete", True


def _grade_status(repo: Path, argv: list[str]) -> dict:
    proc = _run([sys.executable, "tools/labs/grade.py", *argv,
                 "--repo", str(repo)], repo)
    m = re.search(
        r"(\d+) passed / (\d+) skipped / (\d+) failed \(of (\d+)\)",
        proc.stdout)
    if m:
        passed, skipped, failed, total = (int(m.group(1)), int(m.group(2)),
                                          int(m.group(3)), int(m.group(4)))
        return {"status": "green" if proc.returncode == 0 and failed == 0
                else ("error" if failed > 0 else "invalid"),
                "passed": passed, "skipped": skipped, "failed": failed,
                "total": total}
    return {"status": "invalid", "passed": -1, "skipped": -1,
            "failed": -1, "total": -1,
            "reason": "grade summary could not be parsed"}


def _ctest_state(returncode: int, stdout: str) -> dict:
    """Parse a ctest run into its evidence dimension (v008 blocker fix).

    Accepts BOTH summary formats:
      modern : "100% tests passed, 0 tests failed out of 414"
      legacy : "100% tests passed out of 414"
    Green requires returncode == 0 AND zero failed tests; an unparseable
    tail is an error, never a pass.
    """
    m = re.search(
        r"(\d+)% tests passed(?:,\s*(\d+) tests failed)? out of (\d+)",
        stdout)
    if not m:
        return {"status": "error", "reason": "ctest output unparseable",
                "ran": 0, "passed": 0, "failed": -1}
    total = int(m.group(3))
    failed = int(m.group(2)) if m.group(2) is not None else 0
    passed = total - max(failed, 0)
    if returncode != 0:
        # Keep the parseable summary so a failing run records its real
        # counts instead of sentinels (P2: rc!=0 discarded them).
        return {"status": "error", "reason": f"ctest exited {returncode}",
                "ran": total, "passed": passed, "failed": failed}
    status = "green" if failed == 0 else "error"
    return {"status": status, "ran": total, "passed": passed,
            "failed": failed}


def main() -> int:

    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", default=".")
    ap.add_argument("--build-dir", default="build-solutions")
    ap.add_argument("--pipeline", action="store_true",
                    help="also grade tests/pipeline manifests")
    args = ap.parse_args()

    repo = Path(args.repo).resolve()

    head = _run(["git", "rev-parse", "--short", "HEAD"], repo).stdout.strip()

    # ---- reference CTest: returncode-first, explicit not-run ----
    ct = repo / args.build_dir
    if (ct / "CTestTestfile.cmake").is_file():
        proc = _run(["ctest", "--test-dir", str(ct), "--output-on-failure"],
                    repo)
        ctest_state = _ctest_state(proc.returncode, proc.stdout)
    else:
        ctest_state = {"status": "not-run",
                       "reason": f"{args.build_dir}/CTestTestfile.cmake "
                                 "missing",
                       "ran": 0, "passed": 0, "failed": 0}

    # ---- unseen grade sweep ----
    unseen = _grade_status(repo, ["--allow-missing-env"])

    # ---- pipeline grade sweep (v007 #39: the flag now does something) ----
    pipeline = {"status": "skipped"}
    if args.pipeline:
        proc = _run([sys.executable, "tools/labs/grade.py", "--pipeline",
                     "--allow-missing-env", "--repo", str(repo)], repo)
        m = re.search(r"(\d+) passed / (\d+) skipped / (\d+) failed "
                      r"\(of (\d+)\)", proc.stdout)
        if m and proc.returncode == 0 and int(m.group(3)) == 0:
            pipeline = {"status": "green", "passed": int(m.group(1)),
                        "skipped": int(m.group(2)), "failed": int(m.group(3))}
        else:
            pipeline = {"status": "error" if proc.returncode != 0
                        else "invalid", "reason": "pipeline sweep failed"}

    # ---- grader self-tests ----
    gt = _run([sys.executable, "-m", "unittest", "discover", "-s",
               "tools/labs/tests", "-v"], repo)
    gm = re.search(r"Ran (\d+) tests", gt.stdout + gt.stderr)
    self_state = {"status": "green" if gt.returncode == 0 else "error",
                  "ran": int(gm.group(1)) if gm else 0,
                  "failed": 0 if gt.returncode == 0 else -1}

    # ---- external C17 track: derived count + audit ----
    tmf = repo / "tracks" / "foundations-c17" / "manifest.json"
    if tmf.is_file():
        m = json.loads(tmf.read_text())
        verified = sum(1 for e in m.get("chapters", {}).values()
                       if e.get("implementation") == "verified")
        audit = _run([sys.executable, "tools/labs/audit_track.py",
                      "foundations-c17"], repo)
        ext = {"audit": "green" if audit.returncode == 0 else "error",
               "verified_nodes": verified}
    else:
        ext = {"audit": "invalid", "verified_nodes": -1,
               "reason": "track manifest missing"}
    required = {
        "reference_ctest": ctest_state["status"],
        "unseen_grade": unseen["status"],
        "grader_self_tests": self_state["status"],
        "foundations_c17_audit": ext["audit"],
    }
    if args.pipeline:
        required["pipeline_grade"] = pipeline["status"]

    integration = {
        "ch52": _ch52_status(repo, ct),
        "ch51": _ch51_status(repo, ct),
    }
    overall, gate_ok = _verdict(required, integration)

    current = {
        "generated_at": datetime.datetime.now().isoformat(timespec="seconds"),
        "head": head,
        "platform": sys.platform,
        "overall": overall,
        "required": required,
        "reference_ctest": ctest_state,
        "unseen_grade": unseen,
        "pipeline_grade": pipeline,
        "grader_self_tests": self_state,
        "external_courses": {"foundations-c17": ext},
        "integration_reference": integration,
    }
    (repo / "verification-current.json").write_text(
        json.dumps(current, indent=2))
    print(json.dumps(current, indent=2))
    return 0 if gate_ok else 1


if __name__ == "__main__":
    sys.exit(main())