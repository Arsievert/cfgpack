# Makefile for CFGPack Low-Memory Example
#
# Demonstrates:
# - Using cfgpack_schema_measure() to discover buffer sizes at runtime
# - Allocating right-sized buffers via malloc (no oversized static arrays)
# - CFGPACK_MAX_ENTRIES at the library default (128) — no override needed
# - Lowered CFGPACK_SKIP_MAX_DEPTH (8) to reduce msgpack skip stack
# - Schema migration from hvac_v1 (84 entries) -> hvac_v2 (94 entries)
#
# Usage:
#   make        - Build the example (also builds library if needed)
#   make run    - Build and run the example
#   make clean  - Remove build artifacts

# --- Paths --------------------------------------------------------------------
ROOT    := ../..
INCLUDE := $(ROOT)/include
LIBDIR  := $(ROOT)/build/out
LIB     := $(LIBDIR)/libcfgpack.a

# --- Toolchain ----------------------------------------------------------------
CC := clang

# --- Flags --------------------------------------------------------------------
# CFGPACK_MAX_ENTRIES is left at the library default (128) from config.h.
# This supports up to 128 entries with a 16-byte presence bitmap.
# CFGPACK_SKIP_MAX_DEPTH=8 reduces msgpack skip stack (32 B vs 128 B).
# CFGPACK_HOSTED enables printf for demo output.
CPPFLAGS := -I$(INCLUDE) \
            -DCFGPACK_SKIP_MAX_DEPTH=8 \
            -DCFGPACK_HOSTED
CFLAGS   := -Wall -Wextra -std=c99

# --- Outputs ------------------------------------------------------------------
BUILD  := build
TARGET := $(BUILD)/low_memory

# --- Sources ------------------------------------------------------------------
SRCS := main.c

# --- Default goal -------------------------------------------------------------
.DEFAULT_GOAL := all

# --- Build rules --------------------------------------------------------------
all: $(TARGET) ## Build the example

# Build library with matching flags (default MAX_ENTRIES, reduced skip depth)
$(LIB):
	$(MAKE) -C $(ROOT) CFLAGS="-Wall -Wextra -std=c99 -DCFGPACK_SKIP_MAX_DEPTH=8"

$(BUILD):
	mkdir -p $(BUILD)

$(TARGET): $(SRCS) $(LIB) | $(BUILD)
	@echo "CC low_memory"
	@$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SRCS) $(LIB)

# --- Utility targets ----------------------------------------------------------
run: $(TARGET) ## Build and run the example
	@echo ""
	@./$(TARGET)

clean: ## Remove build artifacts
	@echo "rm -rf $(BUILD)/"
	@rm -rf $(BUILD)

# --- Phony --------------------------------------------------------------------
.PHONY: all run clean
