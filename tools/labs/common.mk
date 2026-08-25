# Shared configuration for emulator-labs builds.

PYTHON3   ?= python3
COMMA     := ,
BUILD_DIR ?= build
BUILD_TYPE ?= Release
# Conservative default (comprehensive-review-2055 #28/29): hundreds of
# generated targets can exhaust process slots at full parallelism.
JOBS      ?= 4
NPROC     := $(JOBS)
CMAKE_EXTRA ?=
CTEST_EXTRA ?=
GRADE_TARGETS ?=

export CTEST_OUTPUT_ON_FAILURE := 1
