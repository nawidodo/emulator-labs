# emulator-labs top-level driver.
# Usage:
#   make list                     show generatable targets
#   LABS=ch03_chip8_architecture [TODO=4] make skels
#   LABS="ch01/a ch01/b" make skels
#   make build / test / debug / sanitize / grade / trace-test
#   make solutions                regenerate full reference solution tree

-include tools/labs/common.mk

.PHONY: list skels build test debug sanitize grade trace-test accuracy solutions progress status clean deep-clean solution-build solution-test lint-portable audit grade-pipeline test-gradertool

lint-portable:
	@$(PYTHON3) tools/labs/check_portable_core.py skels solutions

audit:
	@$(PYTHON3) tools/labs/audit_manifest.py

grade-pipeline:
	@$(PYTHON3) tools/labs/grade.py --repo . --pipeline

test-gradertool:
	@$(PYTHON3) -m unittest discover -s tools/labs/tests -v
list:
	@$(PYTHON3) tools/labs/generate.py --list
skels:
ifdef LABS
	@for t in $(subst $(COMMA), ,$(LABS)); do \
		$(PYTHON3) tools/labs/generate.py --force --targets $$t $(if $(TODO),--todo $(TODO),); done
else
	@echo "usage: LABS=<target>[,<target>...] [TODO=N] make skels"
	@exit 1
endif

build:
	@cmake -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(BUILD_TYPE) $(CMAKE_EXTRA)
	@cmake --build $(BUILD_DIR) --parallel $(JOBS)

test: build
	@ctest --test-dir $(BUILD_DIR) --output-on-failure $(CTEST_EXTRA)

debug: BUILD_TYPE := Debug
debug: build
	@echo "Debug binaries in $(BUILD_DIR). Try: lldb $(BUILD_DIR)/skels/..."

sanitize: CMAKE_EXTRA += -DLABS_SANITIZE=On
sanitize: test

grade:
	@$(PYTHON3) tools/labs/grade.py --repo . $(GRADE_TARGETS)

trace-test:
	@$(PYTHON3) tools/labs/compare_trace.py --manifest traces/trace-manifest.json

accuracy: CTEST_EXTRA += -L accuracy
accuracy: test

solutions:
	@$(PYTHON3) tools/labs/generate.py --mode solution --force --targets $(if $(LABS),$(LABS),all)

progress:
	@$(PYTHON3) tools/labs/progress.py status

status: progress

clean:
	@rm -rf $(BUILD_DIR) $(BUILD_DIR)-sanitize

deep-clean: clean
	@rm -rf skels solutions

solution-build:
	@cmake -S . -B $(BUILD_DIR)-solutions -DLABS_BUILD_SOLUTIONS=On -DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	@cmake --build $(BUILD_DIR)-solutions --parallel $(JOBS)

solution-test: solution-build
	@ctest --test-dir $(BUILD_DIR)-solutions --output-on-failure $(CTEST_EXTRA)
