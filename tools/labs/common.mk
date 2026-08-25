# Shared configuration for emulator-labs builds.

PYTHON3   ?= python3
COMMA     := ,
BUILD_DIR ?= build
BUILD_TYPE ?= Release
# SERIAL BY DEFAULT (correctness-first policy): parallel builds masked a
# real portability failure and caused resource-exhaustion flakes. Override
# explicitly when you want speed: make build JOBS=8
JOBS      ?= 1
NPROC     := $(JOBS)
CMAKE_EXTRA ?=
CTEST_EXTRA ?=
GRADE_TARGETS ?=

export CTEST_OUTPUT_ON_FAILURE := 1
