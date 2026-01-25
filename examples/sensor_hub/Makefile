# Makefile for CFGPack Sensor Hub Example
#
# This example demonstrates:
# - Loading a compressed JSON schema from an external file
# - Decompressing with heatshrink at runtime
# - Using the generic cfgpack API with schema index IDs
#
# Build pipeline:
#   1. Compress sensor_hub.json -> build/schema.hs.bin (heatshrink)
#   2. Compile main.c -> build/sensor_hub
#
# Usage:
#   make        - Build the example (also builds library/tools if needed)
#   make run    - Build and run the example
#   make clean  - Remove build artifacts

# Paths relative to this directory
ROOT     := ../..
INCLUDE  := $(ROOT)/include
LIBDIR   := $(ROOT)/build/out
LIB      := $(LIBDIR)/libcfgpack.a
HS_INC   := $(ROOT)/third_party/heatshrink

# Compression tool
COMPRESS := $(LIBDIR)/cfgpack-compress

# Compiler settings
CC       := clang
CPPFLAGS := -I$(INCLUDE) -I$(HS_INC)
CFLAGS   := -Wall -Wextra -std=c99

# Output
BUILD    := build
TARGET   := $(BUILD)/sensor_hub
SCHEMA   := $(BUILD)/schema.hs.bin

# Sources
SRCS     := main.c

.DEFAULT_GOAL := all

all: $(TARGET) $(SCHEMA)

# Build library if it doesn't exist
$(LIB):
	$(MAKE) -C $(ROOT)

# Build compression tool if it doesn't exist
$(COMPRESS): $(LIB)
	$(MAKE) -C $(ROOT) tools

$(BUILD):
	mkdir -p $(BUILD)

# Compress JSON schema with heatshrink
$(SCHEMA): sensor_hub.json $(COMPRESS) | $(BUILD)
	@echo "Compressing schema with heatshrink..."
	@$(COMPRESS) heatshrink sensor_hub.json $@

# Build example binary
$(TARGET): $(SRCS) $(LIB) | $(BUILD)
	@echo "CC sensor_hub"
	@$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SRCS) $(LIB)

run: $(TARGET) $(SCHEMA)
	@echo ""
	@echo "Running sensor_hub example..."
	@echo ""
	@cd $(BUILD) && ./sensor_hub

clean:
	@echo "rm -rf $(BUILD)/"
	@rm -rf $(BUILD)

.PHONY: all run clean
