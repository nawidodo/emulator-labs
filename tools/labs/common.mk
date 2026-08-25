# Shared configuration for emulator-labs builds.

PYTHON3   ?= python3
COMMA     := ,
BUILD_DIR ?= build
BUILD_TYPE ?= Release
NPROC     := $(shell sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
CMAKE_EXTRA ?=
CTEST_EXTRA ?=
GRADE_TARGETS ?=

export CTEST_OUTPUT_ON_FAILURE := 1
