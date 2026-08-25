#!/usr/bin/env bash
# Isolated chapter verification for emulator-labs.
#
#   tools/labs/verify_chapter.sh ch03_slug [more_targets...]
#   VERIFY_PREFIX=/tmp/mine tools/labs/verify_chapter.sh ...
#
# Generates skeleton + solution trees into an isolated prefix (safe for
# parallel authors), builds both, runs ctest on both.
# Verdicts:
#   skel tree      : build MUST succeed; failing tests are EXPECTED (RED)
#   solutions tree : build AND all tests MUST pass (GREEN)
# Exit 0 iff solutions green and skel buildable.

set -u
REPO="$(cd "$(dirname "$0")/../.." && pwd)"
PREFIX="${VERIFY_PREFIX:-$(mktemp -d /tmp/labs-verify.XXXXXX)}"
mkdir -p "$PREFIX"
echo "[verify] prefix: $PREFIX"

if [ $# -eq 0 ]; then
    echo "usage: $0 TARGET [TARGET...]" >&2
    exit 2
fi

rc_skel_build=0
rc_sol=0

for mode in skel solution; do
    out="$PREFIX/$mode"
    rm -rf "$out"
    mkdir -p "$out"

    extra=""
    [ "$mode" = "solution" ] && extra="--mode solution"
    for t in "$@"; do
        python3 "$REPO/tools/labs/generate.py" --repo "$REPO" \
            $extra --force --targets "$t" --out "$out/tree" || {
            echo "[verify] GENERATE FAILED ($mode, $t)"
            [ "$mode" = "solution" ] && rc_sol=1
            [ "$mode" = "skel" ] && rc_skel_build=1
        }
    done

    # Standalone project root mirroring the repo build rules.
    cat > "$out/CMakeLists.txt" <<EOF
cmake_minimum_required(VERSION 3.21)
project(labs_verify_${mode} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
option(LABS_WARNINGS On)
option(LABS_SANITIZE Off)
if(LABS_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()
if(LABS_WARNINGS)
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()
add_subdirectory(${REPO}/third_party/labstest labstest)
enable_testing()
file(GLOB labs_lists "\${CMAKE_CURRENT_SOURCE_DIR}/tree/ch*/CMakeLists.txt"
     "\${CMAKE_CURRENT_SOURCE_DIR}/tree/ch*/*/CMakeLists.txt")
foreach(l \${labs_lists})
  get_filename_component(d "\${l}" DIRECTORY)
  add_subdirectory("\${d}")
endforeach()
EOF

    if ! cmake -S "$out" -B "$out/build" >"$out/configure.log" 2>&1; then
        echo "[verify] CONFIGURE FAILED ($mode): see $out/configure.log"
        [ "$mode" = "solution" ] && rc_sol=1
        [ "$mode" = "skel" ] && rc_skel_build=1
        continue
    fi
    if ! cmake --build "$out/build" -j "$(sysctl -n hw.ncpu 2>/dev/null || nproc)" \
            >"$out/build.log" 2>&1; then
        echo "[verify] BUILD FAILED ($mode): see $out/build.log"
        grep -m5 -E "error:" "$out/build.log" || true
        [ "$mode" = "solution" ] && rc_sol=1
        [ "$mode" = "skel" ] && rc_skel_build=1
        continue
    fi
    ctest --test-dir "$out/build" --output-on-failure >"$out/ctest.log" 2>&1
    summary=$(grep -E "^100% tests|[0-9]+% tests passed" "$out/ctest.log" | tail -1)
    nfail=$(grep -cE "^The following tests FAILED" "$out/ctest.log" || true)
    if [ "$mode" = "skel" ]; then
        echo "[verify] SKEL: build OK; ctest: ${summary:-no tests found} " \
             "(red failures expected here)"
        grep -E "tests passed" "$out/ctest.log" | tail -1 || true
    else
        if ctest --test-dir "$out/build" >/dev/null 2>&1; then
            echo "[verify] SOLUTIONS: GREEN — $summary"
        else
            echo "[verify] SOLUTIONS: RED (must be green!) — see $out/ctest.log"
            rc_sol=1
        fi
    fi
done

echo "[verify] verdict: skel_build=$([ $rc_skel_build -eq 0 ] && echo ok || echo BROKEN) solutions=$([ $rc_sol -eq 0 ] && echo GREEN || echo RED)"
[ $rc_skel_build -eq 0 ] && [ $rc_sol -eq 0 ]
