# Makefile for CFGPack Data Logger Example
#
# This is a self-contained Makefile that builds the example against
# the cfgpack library in the parent directory.
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
TARGET := $(BUILD)/datalogger

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
	@echo "CC datalogger"
	@$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SRCS) $(LIB)

# --- Utility targets ----------------------------------------------------------
run: $(TARGET) ## Build and run the example
	@echo ""
	@./$(TARGET) datalogger.map

clean: ## Remove build artifacts
	@echo "rm -rf $(BUILD)/"
	@rm -rf $(BUILD)

# --- Phony --------------------------------------------------------------------
.PHONY: all run clean
