JOBS ?= 6
UNIT_JOBS ?= $(JOBS)
UNIT_RUNNER ?= zde
PYTHON ?= python3
ZDE_PYTHON ?= python3
ZDE ?= zde
export ZDE_STRICT_EXIT := 1

.PHONY: all native units dist
all: dist

ifeq ($(ZDE),true)

ifneq ($(UNIT_RUNNER),zde)
$(error UNIT_RUNNER=host must be selected from the host)
endif

UNIT_PYTHON = $(ZDE_PYTHON)
ZDE_CMAKE = $(ZDE_PYTHON) "$(ZDE_HOME)/zde/zde.py" cmake build

native:
	@if [ -d build ] && [ ! -f build/CMakeCache.txt ]; then cmake -S . -B build; fi
	$(ZDE_CMAKE) --target zpascal_bin sysfile_zpu zos_zpu --parallel $(JOBS)

units: native
	$(ZDE_CMAKE) --target pascal_units --parallel $(UNIT_JOBS)

_unit-rom:
	$(ZDE_CMAKE) --target unit_rom
.PHONY: _unit-rom

else

ZDE_MAKE = $(ZDE) --no-tty make UNIT_RUNNER=zde JOBS=$(JOBS) UNIT_JOBS=$(UNIT_JOBS) ZDE_PYTHON="$(ZDE_PYTHON)"

ifeq ($(UNIT_RUNNER),host)
UNIT_PYTHON = $(PYTHON)

native _unit-rom:
	$(ZDE_MAKE) $@

PASCAL_TARGETS := _crt _zvb
.PHONY: _unit-rom _units $(PASCAL_TARGETS)

units: native
	+$(MAKE) --no-print-directory --jobs=$(UNIT_JOBS) _units

_units: $(PASCAL_TARGETS)
$(PASCAL_TARGETS): | _unit-rom

_crt:
	$(UNIT_PYTHON) tools/compile-unit.py units/crt/CRT.PAS --rom build/unit-rom.img --output build/CRT.ZPU --source-output build/CRT.PAS --record build/CRT.unit.json

_zvb:
	$(UNIT_PYTHON) tools/compile-unit.py units/zvb/ZVB.PAS --rom build/unit-rom.img --compact --output build/ZVB.ZPU --source-output build/ZVB.PAS --record build/ZVB.unit.json

else ifeq ($(UNIT_RUNNER),zde)
.PHONY: _zde-build
native units dist: _zde-build
_zde-build:
	$(ZDE_MAKE) $(or $(filter all native units dist,$(MAKECMDGOALS)),dist)
else
$(error UNIT_RUNNER must be zde or host)
endif

endif

ifdef UNIT_PYTHON
dist: units
	$(UNIT_PYTHON) tools/stage-distribution.py build
endif
