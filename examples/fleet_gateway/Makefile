# Makefile for CFGPack Fleet Gateway Example
#
# This example demonstrates:
# - Loading LZ4-compressed msgpack binary schemas (pre-compiled from .map files)
# - Using cfgpack_schema_measure_msgpack() for right-sized heap allocation
# - Three-version migration chain: fleet_v1 -> fleet_v2 -> fleet_v3
# - All five migration scenarios: KEEP, WIDEN, MOVE, REMOVE, ADD
#
# Build pipeline:
#   1. Build cfgpack-schema-pack and cfgpack-compress tools (triggers root make)
#   2. Convert .map -> .msgpack binary (fleet_v1, fleet_v2, fleet_v3)
#   3. Compress .msgpack -> .msgpack.lz4 (LZ4, with 4-byte LE size header)
#   4. Compile main.c -> build/fleet_gateway (linked against LZ4 + libcfgpack)
#
# Usage:
#   make        - Build the example (also builds library/tools if needed)
#   make run    - Build and run the example
#   make clean  - Remove build artifacts

# --- Paths --------------------------------------------------------------------
ROOT    := ../..
INCLUDE := $(ROOT)/include
LIBDIR  := $(ROOT)/build/out
LIB     := $(LIBDIR)/libcfgpack.a
LZ4DIR  := $(ROOT)/third_party/lz4

# --- Toolchain ----------------------------------------------------------------
CC          := clang
SCHEMA_PACK := $(LIBDIR)/cfgpack-schema-pack
COMPRESS    := $(LIBDIR)/cfgpack-compress

# --- Flags --------------------------------------------------------------------
CPPFLAGS := -I$(INCLUDE) -I$(LZ4DIR)
CFLAGS   := -Wall -Wextra -std=c99

# --- Outputs ------------------------------------------------------------------
BUILD  := build
TARGET := $(BUILD)/fleet_gateway

SCHEMAS := $(BUILD)/fleet_v1.msgpack.lz4 \
           $(BUILD)/fleet_v2.msgpack.lz4 \
           $(BUILD)/fleet_v3.msgpack.lz4

# --- Sources ------------------------------------------------------------------
SRCS := main.c

# --- Default goal -------------------------------------------------------------
.DEFAULT_GOAL := all

# --- Build rules --------------------------------------------------------------
all: $(TARGET) $(SCHEMAS) ## Build the example

# Build library if it doesn't exist
$(LIB):
	$(MAKE) -C $(ROOT)

# Build schema-pack tool if it doesn't exist
$(SCHEMA_PACK): $(LIB)
	$(MAKE) -C $(ROOT) tools

# Build compress tool if it doesn't exist
$(COMPRESS): $(LIB)
	$(MAKE) -C $(ROOT) tools

$(BUILD):
	mkdir -p $(BUILD)

# Convert .map schemas to msgpack binary
$(BUILD)/fleet_v1.msgpack: fleet_v1.map $(SCHEMA_PACK) | $(BUILD)
	@echo "PACK fleet_v1.map -> fleet_v1.msgpack"
	@$(SCHEMA_PACK) fleet_v1.map $@

$(BUILD)/fleet_v2.msgpack: fleet_v2.map $(SCHEMA_PACK) | $(BUILD)
	@echo "PACK fleet_v2.map -> fleet_v2.msgpack"
	@$(SCHEMA_PACK) fleet_v2.map $@

$(BUILD)/fleet_v3.msgpack: fleet_v3.map $(SCHEMA_PACK) | $(BUILD)
	@echo "PACK fleet_v3.map -> fleet_v3.msgpack"
	@$(SCHEMA_PACK) fleet_v3.map $@

# Compress msgpack schemas with LZ4
$(BUILD)/fleet_v1.msgpack.lz4: $(BUILD)/fleet_v1.msgpack $(COMPRESS) | $(BUILD)
	@echo "LZ4  fleet_v1.msgpack -> fleet_v1.msgpack.lz4"
	@$(COMPRESS) lz4 $< $@

$(BUILD)/fleet_v2.msgpack.lz4: $(BUILD)/fleet_v2.msgpack $(COMPRESS) | $(BUILD)
	@echo "LZ4  fleet_v2.msgpack -> fleet_v2.msgpack.lz4"
	@$(COMPRESS) lz4 $< $@

$(BUILD)/fleet_v3.msgpack.lz4: $(BUILD)/fleet_v3.msgpack $(COMPRESS) | $(BUILD)
	@echo "LZ4  fleet_v3.msgpack -> fleet_v3.msgpack.lz4"
	@$(COMPRESS) lz4 $< $@

$(TARGET): $(SRCS) $(LIB) | $(BUILD)
	@echo "CC fleet_gateway"
	@$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SRCS) $(LZ4DIR)/lz4.c $(LIB)

# --- Utility targets ----------------------------------------------------------
run: $(TARGET) $(SCHEMAS) ## Build and run the example
	@echo ""
	@echo "Running fleet_gateway example..."
	@echo ""
	@cd $(BUILD) && ./fleet_gateway

clean: ## Remove build artifacts
	@echo "rm -rf $(BUILD)/"
	@rm -rf $(BUILD)

# --- Phony --------------------------------------------------------------------
.PHONY: all run clean
