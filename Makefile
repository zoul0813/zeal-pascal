# Native toolchain jobs; CRT and ZVB each use one isolated emulator afterwards.
JOBS ?= 6
PYTHON ?= python3
ZDE ?= zde

PASCAL_TARGETS := _crt _zvb
.PHONY: all native units dist $(PASCAL_TARGETS)
all: dist

native:
	@if [ ! -f build/CMakeCache.txt ]; then $(ZDE) --no-tty exec cmake -S . -B build; fi
	$(ZDE) --no-tty cmake build --target zpascal_bin sysfile_zpu zos_zpu --parallel $(JOBS)

units: native
	+$(MAKE) --no-print-directory --jobs=$(JOBS) $(PASCAL_TARGETS)

_crt:
	$(PYTHON) tools/compile-unit.py units/crt/CRT.PAS --output build/CRT.ZPU --source-output build/CRT.PAS

_zvb:
	$(PYTHON) tools/compile-unit.py units/zvb/ZVB.PAS --compact --output build/ZVB.ZPU --source-output build/ZVB.PAS

# Share the production build's installation layout after native authoring.
dist: units
	$(PYTHON) tools/stage-distribution.py build
