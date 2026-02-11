# Makefile for CFGPack Allocate-Once Example
#
# This example demonstrates:
# - Heap-allocated buffers (malloc once, never free)
# - Two-phase init: discovery parse -> right-sized malloc -> final parse
# - cfgpack_schema_get_sizing() for computing exact buffer sizes
#
# The example reuses the datalogger schema file to show that the same
# schema can be consumed with either static or dynamic allocation.
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
CPPFLAGS := -I$(INCLUDE)
CFLAGS   := -Wall -Wextra -std=c99

# --- Outputs ------------------------------------------------------------------
BUILD  := build
TARGET := $(BUILD)/allocate-once

# --- Sources ------------------------------------------------------------------
SRCS := main.c

# --- Default goal -------------------------------------------------------------
.DEFAULT_GOAL := all

# --- Build rules --------------------------------------------------------------
all: $(TARGET) ## Build the example

# Build library if it doesn't exist
$(LIB):
	$(MAKE) -C $(ROOT)

$(BUILD):
	mkdir -p $(BUILD)

$(TARGET): $(SRCS) $(LIB) | $(BUILD)
	@echo "CC allocate-once"
	@$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SRCS) $(LIB)

# --- Utility targets ----------------------------------------------------------
run: $(TARGET) ## Build and run the example
	@echo ""
	@./$(TARGET) ../datalogger/datalogger.map

clean: ## Remove build artifacts
	@echo "rm -rf $(BUILD)/"
	@rm -rf $(BUILD)

# --- Phony --------------------------------------------------------------------
.PHONY: all run clean
