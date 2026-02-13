# --- Toolchain ----------------------------------------------------------------
CC           := clang
AR           := ar
CLANG_FORMAT ?= clang-format

# --- Flags --------------------------------------------------------------------
CPPFLAGS      := -Iinclude -Iinclude/cfgpack -Ithird_party/lz4 -Ithird_party/heatshrink
CFLAGS        := -Wall -Wextra -std=c99 -DCFGPACK_LZ4 -DCFGPACK_HEATSHRINK
CFLAGS_HOSTED := $(CFLAGS) -DCFGPACK_HOSTED
LDFLAGS       :=
LDLIBS        :=

# --- Directories --------------------------------------------------------------
BUILD := build
OUT   := $(BUILD)/out
OBJ   := $(BUILD)/obj
JSON  := $(BUILD)/json

# --- Documentation ------------------------------------------------------------
DOCS_VENV   := docs/.venv
DOCS_PIP    := $(DOCS_VENV)/bin/pip
DOCS_SPHINX := $(DOCS_VENV)/bin/sphinx-build

# --- Outputs ------------------------------------------------------------------
LIB := $(OUT)/libcfgpack.a

# --- Sources ------------------------------------------------------------------
# Core library (excludes io_file.c for embedded use)
CORESRC := src/core.c                   \
           src/decompress.c             \
           src/io.c                     \
           src/msgpack.c                \
           src/schema_parser.c          \
           src/tokens.c                 \
           third_party/lz4/lz4.c        \
           third_party/heatshrink/heatshrink_decoder.c

# File I/O wrapper (optional, for desktop/POSIX)
IOFILESRC := src/io_file.c

# Compression tool
COMPRESS_TOOL := $(OUT)/cfgpack-compress
COMPRESS_SRC  := tools/cfgpack-compress.c
COMPRESS_DEPS := third_party/lz4/lz4.c third_party/heatshrink/heatshrink_encoder.c

# Encoder libraries needed for tests (to generate compressed test data)
ENCODER_DEPS := third_party/heatshrink/heatshrink_encoder.c

# Test sources
TESTSRC := tests/basic.c         \
           tests/core_edge.c    \
           tests/decompress.c    \
           tests/io_edge.c      \
           tests/json_edge.c    \
           tests/msgpack.c      \
           tests/parser.c        \
           tests/parser_bounds.c \
           tests/runtime.c       \
           tests/test.c

# All project sources for formatting
FORMAT_FILES := $(shell find src include tests examples tools -name '*.c' -o -name '*.h' | grep -v third_party)

# --- Objects / Dependencies ---------------------------------------------------
COREOBJ    := $(CORESRC:%.c=$(OBJ)/%.o)
IOFILEOBJ  := $(IOFILESRC:%.c=$(OBJ)/%.o)
ENCODEROBJ := $(ENCODER_DEPS:%.c=$(OBJ)/%.o)
OBJECTS    := $(COREOBJ) $(IOFILEOBJ) $(ENCODEROBJ)
TESTBINS   := $(filter-out $(OUT)/test,$(TESTSRC:tests/%.c=$(OUT)/%))
TESTCOMMON := $(OBJ)/tests/test.o
DEPS       := $(OBJECTS:.o=.d) $(TESTSRC:%.c=$(OBJ)/%.d)

# --- Vpath / Default goal -----------------------------------------------------
vpath %.c src tests
.DEFAULT_GOAL := all

# --- Build rules --------------------------------------------------------------
all: $(LIB) ## Build static library (core only, no file I/O)

# Core library without io_file.c
$(LIB): $(COREOBJ)
	@mkdir -p $(OUT)
	@echo "AR $(LIB)"
	@$(AR) rcs $(LIB) $(COREOBJ)

# Generic object compilation
$(OBJ)/%.o: %.c
	@mkdir -p $(@D) $(JSON)
	@echo "CC $<"
	@$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MJ $(JSON)/$(@F).json -c $< -o $@

# Test objects need CFLAGS_HOSTED for printf support
$(OBJ)/tests/%.o: tests/%.c
	@mkdir -p $(@D) $(JSON)
	@echo "CC (hosted) $<"
	@$(CC) $(CPPFLAGS) $(CFLAGS_HOSTED) -MMD -MP -MJ $(JSON)/$(@F).json -c $< -o $@

# io_file.c needs CFLAGS_HOSTED for stdio
$(OBJ)/src/io_file.o: src/io_file.c
	@mkdir -p $(@D) $(JSON)
	@echo "CC (hosted) $<"
	@$(CC) $(CPPFLAGS) $(CFLAGS_HOSTED) -MMD -MP -MJ $(JSON)/$(@F).json -c $< -o $@

# --- Test targets -------------------------------------------------------------
tests: $(TESTBINS) ## Build all test binaries

# Tests link against core lib + io_file.o + encoder (for decompression tests)
$(OUT)/%: $(OBJ)/tests/%.o $(TESTCOMMON) $(LIB) $(IOFILEOBJ) $(ENCODEROBJ)
	@mkdir -p $(OUT)
	@echo "LD $@"
	@$(CC) $(LDFLAGS) -o $@ $< $(TESTCOMMON) $(IOFILEOBJ) $(ENCODEROBJ) $(LIB) $(LDLIBS)

# --- Tool targets -------------------------------------------------------------
tools: $(COMPRESS_TOOL) ## Build compression tool

$(COMPRESS_TOOL): $(COMPRESS_SRC) $(COMPRESS_DEPS)
	@mkdir -p $(OUT)
	@echo "CC $(COMPRESS_TOOL)"
	@$(CC) $(CFLAGS_HOSTED) -Ithird_party/lz4 -Ithird_party/heatshrink -o $@ $(COMPRESS_SRC) $(COMPRESS_DEPS)

# --- Documentation target -----------------------------------------------------
docs: ## Generate Sphinx documentation
	@mkdir -p $(BUILD)/docs/doxygen
	@test -d $(DOCS_VENV) || python3 -m venv $(DOCS_VENV)
	@$(DOCS_PIP) install -q -r docs/requirements.txt
	@doxygen Doxyfile
	@$(DOCS_SPHINX) -q -b html docs/source $(BUILD)/docs/html
	@echo "Documentation: $(BUILD)/docs/html/index.html"

# --- Utility targets ----------------------------------------------------------
compile_commands: $(OBJECTS) ## Generate compile_commands.json from dep JSON
	@mkdir -p $(OUT)
	@cat $(JSON)/* > $(JSON)/temp.json
	@sed -e '1s/^/[\n/' -e '$$s/,$$/\n]/' $(JSON)/temp.json > compile_commands.json
	@rm $(JSON)/temp.json

format: ## Auto-format all source files with clang-format
	$(CLANG_FORMAT) -i $(FORMAT_FILES)

format-check: ## Check formatting without modifying files
	$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_FILES)

clean: ## Remove build artifacts
	-@$(RM) -rvf -- $(BUILD) compile_commands.json .cache

clean-docs: ## Remove generated documentation
	-@$(RM) -rf -- $(BUILD)/docs docs/.venv

help: ## List targets with descriptions
	@awk 'BEGIN {FS = ":.*##"} /^[a-zA-Z0-9][^:]*:.*##/ {printf "%-16s %s\n", $$1, $$2}' $(MAKEFILE_LIST)

# --- Phony / Includes ---------------------------------------------------------
.PHONY: all tests clean clean-docs help docs tools format format-check compile_commands
-include $(DEPS)
