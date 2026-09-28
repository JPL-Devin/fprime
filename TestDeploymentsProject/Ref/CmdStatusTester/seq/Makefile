# Makefile for compiling the CmdStatusTester test sequences
# Compiles every .seq file here to a .bin binary sequence file with fprime-seqgen
#
# Requires an fprime-gds whose fprime-seqgen understands sequence directives
# (LABEL, JCF, JCS, EXIT, ERROR_MODE) on the PATH, e.g. from the activated
# fprime-venv. Override SEQGEN or DICT on the command line if needed:
#   make SEQGEN=/path/to/fprime-seqgen DICT=/path/to/RefTopologyDictionary.json

# Project paths
PROJECT_ROOT := ../../..
BUILD_ARTIFACTS := $(PROJECT_ROOT)/build-artifacts
DICT ?= $(BUILD_ARTIFACTS)/Linux/Ref/dict/RefTopologyDictionary.json
SEQGEN ?= fprime-seqgen

# Find all .seq files
SEQ_FILES := $(wildcard *.seq)
BIN_FILES := $(SEQ_FILES:.seq=.bin)

# Default target
.PHONY: all
all: check-dict $(BIN_FILES)

# Check if dictionary exists
.PHONY: check-dict
check-dict:
	@if [ ! -f "$(DICT)" ]; then \
		echo "Error: Dictionary not found at $(DICT)"; \
		echo "Please build the project first:"; \
		echo "  cd $(PROJECT_ROOT)/Ref && fprime-util build"; \
		exit 1; \
	fi

# Rule to compile .seq files to .bin files
%.bin: %.seq
	@echo "Compiling $< -> $@"
	$(SEQGEN) --dictionary $(DICT) $< $@

# Clean target
.PHONY: clean
clean:
	rm -f *.bin
	@echo "Cleaned all binary sequence files"

# List target
.PHONY: list
list:
	@echo "Source sequences:"
	@for seq in $(SEQ_FILES); do echo "  $$seq"; done
	@echo ""
	@echo "Binary sequences (to be generated):"
	@for bin in $(BIN_FILES); do echo "  $$bin"; done

# Help target
.PHONY: help
help:
	@echo "F Prime Sequence Compiler Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all         - Compile all .seq files to .bin (default)"
	@echo "  clean       - Remove all .bin files"
	@echo "  list        - List all sequence files"
	@echo "  help        - Show this help message"
	@echo ""
	@echo "Individual sequences can be built by name:"
	@echo "  make test_jcf_basic.bin"
	@echo ""
	@echo "Dictionary location:"
	@echo "  $(DICT)"

# Phony target to prevent conflicts with files
.PHONY: all clean list help
