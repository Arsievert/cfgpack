# Makefile for CFGPack Data Logger Example
#
# This is a self-contained Makefile that builds the example against
# the cfgpack library in the parent directory.
#
# Usage:
#   make        - Build the example (also builds library if needed)
#   make run    - Build and run the example
#   make clean  - Remove build artifacts

# Paths relative to this directory
ROOT     := ../..
INCLUDE  := $(ROOT)/include
LIBDIR   := $(ROOT)/build/out
LIB      := $(LIBDIR)/libcfgpack.a

# Compiler settings
CC       := clang
CPPFLAGS := -I$(INCLUDE)
CFLAGS   := -Wall -Wextra -std=c99

# Output
BUILD    := build
TARGET   := $(BUILD)/datalogger

# Sources
SRCS     := main.c

.DEFAULT_GOAL := all

all: $(TARGET)

# Build library if it doesn't exist
$(LIB):
	$(MAKE) -C $(ROOT)

$(BUILD):
	mkdir -p $(BUILD)

$(TARGET): $(SRCS) $(LIB) | $(BUILD)
	@echo "CC datalogger"
	@$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(SRCS) $(LIB)

run: $(TARGET)
	@echo ""
	@./$(TARGET) datalogger.map

clean:
	@echo "rm -rf $(BUILD)/"
	@rm -rf $(BUILD)

.PHONY: all run clean
