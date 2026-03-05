CXX := g++
CXXFLAGS := -std=gnu++17 -O2
BIN := ../vm_cli
SRC := ../vm_cli.cpp

.PHONY: build help fact fib asm-build run-bin dump-bin trace clean

build:
	$(CXX) $(SRC) $(CXXFLAGS) -o $(BIN)

help: build
	$(BIN) --help

fact: build
	$(BIN) fact 5

fib: build
	$(BIN) fib 8

asm-build: build
	$(BIN) asm-build ../sample_factorial.asm ../sample_factorial.bin

run-bin: build
	$(BIN) run-bin ../sample_factorial.bin --trace ../trace_factorial_bin.jsonl

dump-bin: build
	$(BIN) dump-bin ../sample_factorial.bin

trace: build
	$(BIN) trace-summary ../trace_factorial_bin.jsonl

clean:
	rm -f $(BIN) ../sample_factorial.bin ../sample_fibonacci.bin ../trace_factorial_bin.jsonl
