# Makefile for CFGPack Flash Config Example
#
# Demonstrates:
# - LittleFS pageout/pagein with RAM-backed block device
# - LZ4-compressed msgpack binary schemas
# - Schema migration v1 -> v2 with KEEP, WIDEN, MOVE, REMOVE, ADD
# - Composable I/O: manual LFS read + pagein_remap for migration
#
# Build pipeline:
#   1. Build cfgpack-schema-pack and cfgpack-compress tools
#   2. Convert .map -> .msgpack binary (sensor_v1, sensor_v2)
#   3. Compress .msgpack -> .msgpack.lz4 (LZ4, with 4-byte LE size header)
#   4. Compile main.c -> build/flash_config
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
LFSDIR  := $(ROOT)/third_party/littlefs

# --- Toolchain ----------------------------------------------------------------
CC          := clang
SCHEMA_PACK := $(LIBDIR)/cfgpack-schema-pack
COMPRESS    := $(LIBDIR)/cfgpack-compress

# --- Flags --------------------------------------------------------------------
CPPFLAGS := -I$(INCLUDE) -I$(LZ4DIR) -I$(LFSDIR)
CFLAGS   := -Wall -Wextra -std=c99

# --- Outputs ------------------------------------------------------------------
BUILD  := build
TARGET := $(BUILD)/flash_config

SCHEMAS := $(BUILD)/sensor_v1.msgpack.lz4 \
           $(BUILD)/sensor_v2.msgpack.lz4

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
$(BUILD)/sensor_v1.msgpack: sensor_v1.map $(SCHEMA_PACK) | $(BUILD)
	@echo "PACK sensor_v1.map -> sensor_v1.msgpack"
	@$(SCHEMA_PACK) sensor_v1.map $@

$(BUILD)/sensor_v2.msgpack: sensor_v2.map $(SCHEMA_PACK) | $(BUILD)
	@echo "PACK sensor_v2.map -> sensor_v2.msgpack"
	@$(SCHEMA_PACK) sensor_v2.map $@

# Compress msgpack schemas with LZ4
$(BUILD)/sensor_v1.msgpack.lz4: $(BUILD)/sensor_v1.msgpack $(COMPRESS) | $(BUILD)
	@echo "LZ4  sensor_v1.msgpack -> sensor_v1.msgpack.lz4"
	@$(COMPRESS) lz4 $< $@

$(BUILD)/sensor_v2.msgpack.lz4: $(BUILD)/sensor_v2.msgpack $(COMPRESS) | $(BUILD)
	@echo "LZ4  sensor_v2.msgpack -> sensor_v2.msgpack.lz4"
	@$(COMPRESS) lz4 $< $@

$(TARGET): $(SRCS) $(LIB) | $(BUILD)
	@echo "CC flash_config"
	@$(CC) $(CPPFLAGS) $(CFLAGS) \
		-DCFGPACK_LITTLEFS -DCFGPACK_LZ4 -DCFGPACK_HOSTED \
		-o $@ $(SRCS) \
		$(LZ4DIR)/lz4.c \
		$(LFSDIR)/lfs.c $(LFSDIR)/lfs_util.c \
		$(LIB)

# --- Utility targets ----------------------------------------------------------
run: $(TARGET) $(SCHEMAS) ## Build and run the example
	@echo ""
	@echo "Running flash_config example..."
	@echo ""
	@cd $(BUILD) && ./flash_config

clean: ## Remove build artifacts
	@echo "rm -rf $(BUILD)/"
	@rm -rf $(BUILD)

# --- Phony --------------------------------------------------------------------
.PHONY: all run clean
