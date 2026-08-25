#!/usr/bin/env python3
"""ctest driver for ch01/03_generator_starter.

Drives generate_skel.py through subprocess over every (fixture, level)
variant and compares each emitted file's sha256 (manifest.json included)
against GOLDENS recorded from the reference solution. Also re-checks
determinism by generating one variant twice into separate directories.

Exit 0 iff every check passes.
"""

import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
SCRIPT = HERE / "generate_skel.py"

# BEGIN GOLDENS (generated — see tests/public/ch01_lab_infrastructure/provenance.md)
GOLDENS = {
    ("fixture_a", "none"): {
        "app.py.tpl": "326d0005b55bfab612f0a6227c5bdf23eb6afea3fccc5aeea1c1b0d86c1d1d8d",
        "manifest.json": "f0149579b4ac146ef70675be8d6f4000ff0af5862549aedab8b63958c702a8bc",
    },
    ("fixture_a", "1"): {
        "app.py.tpl": "88dd8bc362b3105d50aa42f5f3dda108c4e8252497f6cbd43ef9897b57c6a49a",
        "manifest.json": "4d572f6756783db5d7627ffc5ddb820f1bd2ce6b08f00f4f76d81e72f12c4216",
    },
    ("fixture_a", "2"): {
        "app.py.tpl": "96872234c58e1bfd093fb218c2308ab0cc5d9ab1b3c1482bb35fe691bcc9aeb9",
        "manifest.json": "2eda97b930dec6a118ea56bc3bdbad956a8a8f04bf6593fbf02945b32317e6e6",
    },
    ("fixture_a", "3"): {
        "app.py.tpl": "b36d345e1d1b4193d4b887b6f3cd78d66f59f7e4a22fe89bf3ffb662360c4d73",
        "manifest.json": "d3f74799bead6264249741b9bf1b7519591c31b09079dc7fb762e0ccfa4c0bab",
    },
    ("fixture_a", "sol"): {
        "app.py.tpl": "b36d345e1d1b4193d4b887b6f3cd78d66f59f7e4a22fe89bf3ffb662360c4d73",
        "manifest.json": "b0f8e4ccb0d08b2c4c5278e743d3cd4f20e149631661941cb414e4bac95e863f",
    },
    ("fixture_b", "none"): {
        "bus.hpp.tpl": "47667d28ad5f621b7db60242f628cbae74639198cf245a076e78e5201db1113e",
        "cpu.hpp.tpl": "11d50988c8bcd4c54ff66a7d6c6357a95e57dc9d0d39815a9756a07bed04bb21",
        "manifest.json": "120150c391935d2e9140feec6246e7b52d0d29418359bdc2d0392c49a7de08fd",
    },
    ("fixture_b", "1"): {
        "bus.hpp.tpl": "47667d28ad5f621b7db60242f628cbae74639198cf245a076e78e5201db1113e",
        "cpu.hpp.tpl": "69520de266054562b44738e4349b523e0908b8b0de98298139b346d6657214e2",
        "manifest.json": "17392c60e288931006e9aac62ccd5fe522e8403ceb871b1f21172175fb1f32d4",
    },
    ("fixture_b", "2"): {
        "bus.hpp.tpl": "47667d28ad5f621b7db60242f628cbae74639198cf245a076e78e5201db1113e",
        "cpu.hpp.tpl": "14d1b323f8653ab35199fccd0aaff75b062699a8f6be9b87ef00ab75bd1a41d3",
        "manifest.json": "3e176d666ef1236e610d922199e00989d345b7ba6bb6c5c4daed29f1127e12cc",
    },
    ("fixture_b", "3"): {
        "bus.hpp.tpl": "4de5cd478a1d266fa61c3ad3ae05acbb9f28a0266aa94de9070530a43ca7edd4",
        "cpu.hpp.tpl": "14d1b323f8653ab35199fccd0aaff75b062699a8f6be9b87ef00ab75bd1a41d3",
        "manifest.json": "227cef91ff2dbce2f70cd0c4e29fc4378fc3db14133d34df763576fa7370e13b",
    },
    ("fixture_b", "4"): {
        "bus.hpp.tpl": "1558646f06ae3e70228fa83c6e920640f59f3156babaf246c40dbafd35b685c1",
        "cpu.hpp.tpl": "14d1b323f8653ab35199fccd0aaff75b062699a8f6be9b87ef00ab75bd1a41d3",
        "manifest.json": "3253aef869bbd9a793f75b8629cdf4ff2ff68a6a3d21f0fa7c4b0b4a19c2a74c",
    },
    ("fixture_b", "sol"): {
        "bus.hpp.tpl": "1558646f06ae3e70228fa83c6e920640f59f3156babaf246c40dbafd35b685c1",
        "cpu.hpp.tpl": "14d1b323f8653ab35199fccd0aaff75b062699a8f6be9b87ef00ab75bd1a41d3",
        "manifest.json": "974acac7c2680528790c70f8dcf4995526a789a3995c4e8677fdbe41ee517ea9",
    },
    ("fixture_c", "none"): {
        "config.ini.tpl": "22118af71a992ffd777bd4aa82666f7f77e68603824b0a7a71d889deacb891d7",
        "deep/impl.py.tpl": "a4a4513104eb47575e119a2ae3672ee80dbdd21d326324a97cde9c1c610a3b93",
        "manifest.json": "b6b3011aacb9969d84460d7c1831b63f89bb28ce3df79febdd0be4aa11a671b5",
        "notes.txt": "786f625a307e2f3efba20ede0fb61dddbb66b13bd538d67874b35407f3ac9b3d",
    },
    ("fixture_c", "1"): {
        "config.ini.tpl": "e9bdd659677d8120dab517fc668c112a6ab2484da1fe6960fb4c67e759e11dc1",
        "deep/impl.py.tpl": "a4a4513104eb47575e119a2ae3672ee80dbdd21d326324a97cde9c1c610a3b93",
        "manifest.json": "3aff90981e909895be799e0f34b28f9762ab287b6306341ce64a196ee2bf78bd",
        "notes.txt": "786f625a307e2f3efba20ede0fb61dddbb66b13bd538d67874b35407f3ac9b3d",
    },
    ("fixture_c", "2"): {
        "config.ini.tpl": "e9bdd659677d8120dab517fc668c112a6ab2484da1fe6960fb4c67e759e11dc1",
        "deep/impl.py.tpl": "cc8c440dc57a6384fc2638604a046d138807f9c24f159f29051e76c280952cb5",
        "manifest.json": "a7d82982f962eaed65cf62cbfa98bac1de3dc2fdfec7d419953b414a26690671",
        "notes.txt": "786f625a307e2f3efba20ede0fb61dddbb66b13bd538d67874b35407f3ac9b3d",
    },
    ("fixture_c", "sol"): {
        "config.ini.tpl": "e9bdd659677d8120dab517fc668c112a6ab2484da1fe6960fb4c67e759e11dc1",
        "deep/impl.py.tpl": "cc8c440dc57a6384fc2638604a046d138807f9c24f159f29051e76c280952cb5",
        "manifest.json": "6dd19b359a5101970f2cc267ed0e66e2f6e07ce88c720d3526d8c2cc5ade2cdd",
        "notes.txt": "786f625a307e2f3efba20ede0fb61dddbb66b13bd538d67874b35407f3ac9b3d",
    },
}



def run_gen(args, out_dir):
    cmd = [sys.executable, str(SCRIPT)] + args + ["--out", str(out_dir)]
    return subprocess.run(cmd, capture_output=True, text=True)


def tree_hashes(out_dir: Path) -> dict:
    return {
        p.relative_to(out_dir).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
        for p in sorted(out_dir.rglob("*"))
        if p.is_file()
    }


def variant_args(template: str, variant: str):
    args = ["--template", template]
    if variant == "sol":
        args += ["--mode", "solution"]
    elif variant != "none":
        args += ["--todo", variant]
    return args


def main() -> int:
    failures = 0
    checks = 0
    for (template, variant), expected in sorted(GOLDENS.items()):
        checks += 1
        with tempfile.TemporaryDirectory(prefix="labs_ch01_03_") as td:
            out = Path(td) / "out"
            proc = run_gen(variant_args(template, variant), out)
            if proc.returncode != 0:
                failures += 1
                print(f"FAIL {template}/{variant}: exit {proc.returncode} "
                      f"{(proc.stderr or proc.stdout).strip()[:160]}")
                continue
            got = tree_hashes(out)
            missing = sorted(set(expected) - set(got))
            extra = sorted(set(got) - set(expected))
            bad = [p for p in sorted(set(got) & set(expected))
                   if got[p] != expected[p]]
            if missing or extra or bad:
                failures += 1
                detail = []
                if missing:
                    detail.append(f"missing={missing}")
                if extra:
                    detail.append(f"extra={extra}")
                if bad:
                    detail.append(f"hash-mismatch={bad}")
                print(f"FAIL {template}/{variant}: " + " ".join(detail))
            else:
                print(f"ok   {template}/{variant} ({len(got)} files)")

    # Determinism: same inputs twice must give byte-identical trees.
    checks += 1
    with tempfile.TemporaryDirectory(prefix="labs_ch01_03_det_") as td:
        a, b = Path(td) / "a", Path(td) / "b"
        ra = run_gen(["--template", "fixture_a", "--todo", "2"], a)
        rb = run_gen(["--template", "fixture_a", "--todo", "2"], b)
        ha, hb = tree_hashes(a), tree_hashes(b)
        if ra.returncode == rb.returncode == 0 and ha == hb and \
                json.loads((b / "manifest.json").read_text()) == \
                json.loads((a / "manifest.json").read_text()):
            print("ok   determinism (fixture_a/2 twice)")
        else:
            failures += 1
            print("FAIL determinism (fixture_a/2 twice)")

    print(f"== {checks} checks, {failures} failed")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
