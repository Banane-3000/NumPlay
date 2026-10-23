PYTHON ?= python3
HOST_CC ?= cc
NWLINK ?= nwlink
TOOLCHAIN ?=
.PHONY: build check module sim-module test test-arm test-integration clean
build check module:
	$(PYTHON) tools/build.py --nwlink "$(NWLINK)" $(if $(TOOLCHAIN),--toolchain "$(TOOLCHAIN)")
test:
	$(PYTHON) tools/test.py --cc "$(HOST_CC)"
test-arm: build test
	$(PYTHON) tests/test_arm.py --nwlink "$(NWLINK)"
test-integration: module
	$(PYTHON) tests/test_integration.py --nwlink "$(NWLINK)"
sim-module:
	mkdir -p output/sim
	$(HOST_CC) -std=c11 -O2 -fPIC $(shell $(NWLINK) eadk-cflags-simulator) -DNP_MODULE=1 -Dmain=np_uno_main -c src/main.c -o output/sim/main.o
	$(HOST_CC) -std=c11 -O2 -fPIC -c src/uno.c -o output/sim/uno.o
	$(HOST_CC) -r -nostdlib output/sim/main.o output/sim/uno.o -o output/sim-module.o
clean:
	$(PYTHON) -c "import shutil; shutil.rmtree('output', ignore_errors=True)"
