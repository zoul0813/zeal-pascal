# Native toolchain jobs; CRT and ZVB each use one isolated emulator afterwards.
JOBS ?= 6
PYTHON ?= python3
ZDE ?= zde

PASCAL_TARGETS := _crt _zvb
DIST_FILES := zpascal.bin zpascal.zpl SYSFILE.ZPU CRT.ZPU ZVB.ZPU ZOS.ZPU \
              CRT.PAS ZVB.PAS ZOS.PAS

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

# Only publish the loose hardware files after every compilation succeeds.
dist: units
	mkdir -p bin
	cp $(addprefix build/,$(DIST_FILES)) bin/
	cat units/zvb/ZVB-LICENSE.txt units/zvb/ZVB-NOTICE.txt > bin/ZVB.LIC
