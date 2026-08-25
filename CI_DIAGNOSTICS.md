# CI Failure Diagnostics

**Date**: 2026-08-25  
**Repository**: nawidodo/emulator-labs  
**Status**: Ongoing CI failures across multiple platforms

---

## Executive Summary

The CI pipeline is failing on **two distinct issues**:

1. **Windows (MSVC) Build Failures** - Missing POSIX headers and GCC-specific builtins
2. **macOS Build Resource Exhaustion** - Parallel job spawning exceeding system limits

Both issues are fixable with targeted platform-conditional code and build configuration adjustments.

---

## Issue 1: Windows Build Failures

### Failed Run
- **Run ID**: 32859602723
- **Commit**: `8d517c8081a878e396522dbde619be2d0e3e75e5`
- **Platform**: Windows (MSVC)
- **Status**: ❌ FAILURE

### Error Details

#### Error 1: Missing `sys/wait.h`
```
D:\a\emulator-labs\emulator-labs\solutions\ch50_ps1_accuracy_trace_testing\01_suite_runner\suite.hpp(2,10): 
error C1083: Cannot open include file: 'sys/wait.h': No such file or directory
```

**Location**: `solutions/ch50_ps1_accuracy_trace_testing/01_suite_runner/suite.hpp` (line 2)

**Root Cause**: `<sys/wait.h>` is a POSIX header that does not exist on Windows. MSVC does not provide this header.

**Impact**: Any file including `<sys/wait.h>` will fail to compile on Windows.

#### Error 2: Missing `__builtin_popcount`
```
D:\a\emulator-labs\emulator-labs\solutions\ch07_i8080_architecture\01_flags\main.cpp(20,20): 
error C3861: '__builtin_popcount': identifier not found
```

**Location**: `solutions/ch07_i8080_architecture/01_flags/main.cpp` (line 20)  
**Related Files**: `solutions/ch07_i8080_architecture/99_coding_test/extra_ops.hpp` (line 67)

**Root Cause**: `__builtin_popcount()` is a GCC/Clang builtin that doesn't exist in MSVC. Windows uses `__popcnt()` from `<intrin.h>` instead.

**Impact**: Any use of `__builtin_popcount` will fail on Windows.

### Solution

#### For `sys/wait.h` Headers

Add platform-conditional guards to all files including POSIX-only headers:

```cpp
// In header files that include <sys/wait.h>
#ifndef _WIN32
  #include <sys/wait.h>
#endif
```

Or conditionally wrap the usage:
```cpp
#ifdef _WIN32
  // Windows-specific implementation (if needed)
#else
  // POSIX implementation using sys/wait.h
#endif
```

**Files Affected** (from error logs):
- `solutions/ch50_ps1_accuracy_trace_testing/01_suite_runner/suite.hpp`

#### For `__builtin_popcount`

Create a portable macro in a common header:

```cpp
// In a common utility header (e.g., src/platform.hpp or similar)
#ifdef _MSC_VER
  #include <intrin.h>
  #define POPCOUNT(x) __popcnt(x)
#else
  #define POPCOUNT(x) __builtin_popcount(x)
#endif
```

Then replace all instances of `__builtin_popcount(x)` with `POPCOUNT(x)`.

**Files Affected** (from error logs):
- `solutions/ch07_i8080_architecture/01_flags/main.cpp`
- `solutions/ch07_i8080_architecture/99_coding_test/extra_ops.hpp`

---

## Issue 2: macOS Build Resource Exhaustion

### Failed Run
- **Run ID**: 32849040454
- **Commit**: `8b16dde9532088bf369da62fdc441be1d0e4b7d1`
- **Platform**: macOS
- **Status**: ❌ FAILURE

### Error Details

```
clang++: error: unable to execute command: posix_spawn failed: Resource temporarily unavailable
/bin/sh: fork: Resource temporarily unavailable
make[2]: *** [...] Error 128
make[1]: *** [...] Error 2
```

**Timeline**:
- Build runs for ~15 minutes
- Errors start appearing around 12:42:25Z
- Multiple cascading compilation failures
- Build eventually terminates with `make: *** [all] Error 2`

**Root Cause**: The parallel build job limit (set via `-j` flag) is set too high for the GitHub Actions macOS runner, causing system resource exhaustion. When too many compiler processes spawn simultaneously, the operating system cannot allocate enough resources for new processes.

**Evidence**:
- Initial builds complete successfully on some targets
- Errors escalate as more parallel jobs attempt to spawn
- Fork failures indicate process table exhaustion or memory pressure

### Solution

#### Reduce Parallel Job Limit

The commit message mentions:
> "conservative overrideable JOBS default (4) in common.mk; Makefile and CI use --parallel explicitly"

**Implementation**:

1. **In `common.mk` (or equivalent build configuration)**:
```makefile
# Default to 4 parallel jobs (conservative for CI environments)
JOBS ?= 4
MAKEFLAGS += -j$(JOBS)
```

2. **In `.github/workflows/ci.yml`**:
```yaml
jobs:
  build-macos:
    runs-on: macos-latest
    steps:
      - name: Build Solutions
        run: make build-solutions JOBS=4
        
      - name: Build and Test
        run: make build JOBS=4
```

3. **In root `Makefile`** (if not using `common.mk`):
```makefile
.PHONY: build build-solutions
build:
	make -C build --parallel=$(JOBS)
build-solutions:
	make -C build-solutions --parallel=$(JOBS)
```

#### Alternative: Use `--load-average` Flag

If you want more aggressive parallelism while still protecting system resources:

```bash
make --load-average=4 --parallel
```

This tells `make` to spawn new jobs only when the system load average drops below 4.

---

## Platform Matrix Status

| Platform | Status | Issue |
|----------|--------|-------|
| Ubuntu (GCC) | 🟡 Unknown | See logs for details |
| Ubuntu (Clang) | 🟡 Unknown | See logs for details |
| macOS (AppleClang) | ❌ FAILURE | Resource exhaustion during parallel build |
| Windows (MSVC) | ❌ FAILURE | Missing POSIX headers + GCC builtins |

---

## Recommended Action Plan

### Immediate (Critical Path)

1. **Windows Fixes** (high priority)
   - [ ] Add `#ifndef _WIN32` guards around `<sys/wait.h>` includes
   - [ ] Create portable `POPCOUNT()` macro
   - [ ] Replace all `__builtin_popcount` calls with `POPCOUNT`
   - [ ] Test Windows build locally or in CI

2. **macOS Fixes** (high priority)
   - [ ] Update `.github/workflows/ci.yml` to explicitly set `JOBS=4`
   - [ ] Verify `common.mk` has `JOBS ?= 4` default
   - [ ] Re-run macOS job

### Follow-up

3. **Auditing** (medium priority)
   - [ ] Search entire codebase for other POSIX-only headers
   - [ ] Search for other GCC/Clang-specific builtins
   - [ ] Document platform-specific code patterns in CONTRIBUTING.md

4. **Testing** (ongoing)
   - [ ] Validate all three platform jobs pass
   - [ ] Consider adding Windows to the required build matrix (currently may be best-effort)

---

## Log References

### Raw Error Logs

**Windows (MSVC) - Run 32859602723**:
- Multiple `C1083` errors for missing `sys/wait.h`
- Multiple `C3861` errors for missing `__builtin_popcount`
- Build exits with code 1

**macOS - Run 32849040454**:
- `posix_spawn failed: Resource temporarily unavailable` (clang++)
- `fork: Resource temporarily unavailable` (shell)
- Build stalls after ~15 minutes of parallel compilation
- Build exits with code 2

---

## Additional Notes

- The commit messages indicate awareness of build stability issues: *"Build stability (macOS posix_spawn class)"*
- Windows CI is noted as *"best-effort"* with `continue-on-error` in some configurations
- The codebase uses both C++20 and C17 (strict) across different tracks
- Recent refactors include include-hygiene sweeps, suggesting awareness of portability concerns

---

**Generated**: 2026-08-25  
**Next Review**: After fixes are applied to main branch
