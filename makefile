# --- Toolchain ----------------------------------------------------------------
CC           := clang
AR           := ar
CLANG_FORMAT ?= clang-format

# --- Flags --------------------------------------------------------------------
CPPFLAGS      := -Iinclude -Iinclude/cfgpack -Ithird_party/lz4 -Ithird_party/heatshrink -Ithird_party/littlefs
CFLAGS        := -Wall -Wextra -std=c99 -Os -DCFGPACK_LZ4 -DCFGPACK_HEATSHRINK -DCFGPACK_LITTLEFS
CFLAGS_HOSTED := $(CFLAGS) -DCFGPACK_HOSTED
LDFLAGS       :=
LDLIBS        :=

# Set WERROR=-Werror (CI does) to fail on warnings in project code.  Vendored
# third_party sources are exempt below.
WERROR ?=

# littlefs's debug/warn/error logging pulls <stdio.h> and printf into the
# library.  Compile it out so libcfgpack.a stays stdio-free.  LFS_NO_MALLOC is
# left to the integrator: cfgpack's own wrappers supply every buffer littlefs
# needs, so it is only relevant to the application's direct littlefs calls.
LFS_FLAGS := -DLFS_NO_DEBUG -DLFS_NO_WARN -DLFS_NO_ERROR

# --- Directories --------------------------------------------------------------
# OBJ holds embedded objects (default CFLAGS), HOBJ hosted ones
# (CFLAGS_HOSTED), TMP scratch files written by tests.
BUILD := build
OUT   := $(BUILD)/out
OBJ   := $(BUILD)/obj
HOBJ  := $(BUILD)/obj-hosted
JSON  := $(BUILD)/json
TMP   := $(BUILD)/tmp

# -MJ (emit compile_commands.json fragment) is a clang-only flag.  The fragment
# name mirrors the object path so src/msgpack.o and tests/msgpack.o cannot
# collide.
ifneq (,$(findstring clang,$(shell $(CC) --version 2>/dev/null | head -1)))
  MJ_FLAG = -MJ $(JSON)/$(subst /,_,$(@:$(BUILD)/%=%)).json
else
  MJ_FLAG =
endif

# --- Documentation ------------------------------------------------------------
DOCS_VENV   := docs/.venv
DOCS_PIP    := $(DOCS_VENV)/bin/pip
DOCS_SPHINX := $(DOCS_VENV)/bin/sphinx-build

# --- Outputs ------------------------------------------------------------------
LIB        := $(OUT)/libcfgpack.a
LIB_HOSTED := $(OUT)/libcfgpack-hosted.a

# --- Sources ------------------------------------------------------------------
# Core library: everything an embedded target needs, no stdio, no file I/O.
CORESRC := src/core.c                   \
           src/crc32.c                  \
           src/decompress.c             \
           src/io.c                     \
           src/io_littlefs.c            \
           src/msgpack.c                \
           src/schema_parser.c          \
           src/tokens.c                 \
           src/wbuf.c                   \
           third_party/lz4/lz4.c        \
           third_party/heatshrink/heatshrink_decoder.c \
           third_party/littlefs/lfs.c   \
           third_party/littlefs/lfs_util.c

# Hosted-only sources: POSIX file I/O wrapper and the parse-error string
# table.  Compiled into libcfgpack-hosted.a only.
HOSTEDSRC := src/io_file.c src/parse_error_str.c

# heatshrink encoder: needed by the compress tool and by tests that build
# compressed fixtures.  Never part of either archive.
ENCODER_SRC := third_party/heatshrink/heatshrink_encoder.c

# Tools
COMPRESS_TOOL        := $(OUT)/cfgpack-compress
SCHEMA_PACK_TOOL     := $(OUT)/cfgpack-schema-pack
SCHEMA_VALIDATE_TOOL := $(OUT)/cfgpack-schema-validate
TOOLS := $(COMPRESS_TOOL) $(SCHEMA_PACK_TOOL) $(SCHEMA_VALIDATE_TOOL)

# Test sources
TESTSRC := tests/basic.c         \
           tests/core_edge.c    \
           tests/coverage.c     \
           tests/decompress.c    \
           tests/io_edge.c      \
           tests/io_littlefs.c  \
           tests/json_edge.c    \
           tests/json_remap.c   \
           tests/measure.c      \
           tests/msgpack.c      \
           tests/msgpack_decode.c \
           tests/msgpack_schema.c \
           tests/null_args.c    \
           tests/parser.c        \
           tests/parser_bounds.c \
           tests/runtime.c       \
           tests/test.c

EXAMPLE_DIRS := $(sort $(dir $(wildcard examples/*/Makefile)))

# All project sources for formatting
FORMAT_FILES := $(shell find src include tests examples tools -name '*.c' -o -name '*.h' | grep -v third_party)

# --- Objects ------------------------------------------------------------------
COREOBJ     := $(CORESRC:%.c=$(OBJ)/%.o)
HCOREOBJ    := $(CORESRC:%.c=$(HOBJ)/%.o)
HOSTEDOBJ   := $(HOSTEDSRC:%.c=$(HOBJ)/%.o)
ENCODEROBJ  := $(ENCODER_SRC:%.c=$(HOBJ)/%.o)
TESTBINS    := $(filter-out $(OUT)/test,$(TESTSRC:tests/%.c=$(OUT)/%))
TESTCOMMON  := $(HOBJ)/tests/test.o
TAB         := $(shell printf '\t')

.DEFAULT_GOAL := all

# --- Library targets ----------------------------------------------------------
all: $(LIB) ## Build libcfgpack.a (embedded: no stdio, no file I/O)

hosted: $(LIB_HOSTED) ## Build libcfgpack-hosted.a (adds printf, file I/O, error strings)

$(LIB): $(COREOBJ)
	@mkdir -p $(OUT)
	@echo "AR $@"
	@$(AR) rcs $@ $^

$(LIB_HOSTED): $(HCOREOBJ) $(HOSTEDOBJ)
	@mkdir -p $(OUT)
	@echo "AR $@"
	@$(AR) rcs $@ $^

# --- Compilation rules --------------------------------------------------------
$(OBJ)/%.o: %.c
	@mkdir -p $(@D) $(JSON)
	@echo "CC $<"
	@$(CC) $(CPPFLAGS) $(CFLAGS) $(WERROR) -MMD -MP $(MJ_FLAG) -c $< -o $@

$(HOBJ)/%.o: %.c
	@mkdir -p $(@D) $(JSON)
	@echo "CC (hosted) $<"
	@$(CC) $(CPPFLAGS) $(CFLAGS_HOSTED) $(WERROR) -MMD -MP $(MJ_FLAG) -c $< -o $@

# Vendored code is not held to -Werror; littlefs logging is compiled out.
$(OBJ)/third_party/%.o $(HOBJ)/third_party/%.o: override WERROR :=
$(OBJ)/third_party/littlefs/%.o $(HOBJ)/third_party/littlefs/%.o: CPPFLAGS += $(LFS_FLAGS)

# --- Test targets -------------------------------------------------------------
tests: $(TESTBINS) ## Build all test binaries (linked against the hosted library)
	@mkdir -p $(TMP)

$(OUT)/%: $(HOBJ)/tests/%.o $(TESTCOMMON) $(LIB_HOSTED) $(ENCODEROBJ)
	@mkdir -p $(OUT)
	@echo "LD $@"
	@$(CC) $(LDFLAGS) -o $@ $< $(TESTCOMMON) $(ENCODEROBJ) $(LIB_HOSTED) $(LDLIBS)

# --- Tool targets -------------------------------------------------------------
tools: $(TOOLS) ## Build cfgpack-compress, cfgpack-schema-pack, cfgpack-schema-validate

$(COMPRESS_TOOL): $(HOBJ)/tools/cfgpack-compress.o $(HOBJ)/third_party/lz4/lz4.o $(ENCODEROBJ)
	@mkdir -p $(OUT)
	@echo "LD $@"
	@$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(SCHEMA_PACK_TOOL): $(HOBJ)/tools/cfgpack-schema-pack.o $(LIB_HOSTED)
	@mkdir -p $(OUT)
	@echo "LD $@"
	@$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(SCHEMA_VALIDATE_TOOL): $(HOBJ)/tools/cfgpack-schema-validate.o $(LIB_HOSTED)
	@mkdir -p $(OUT)
	@echo "LD $@"
	@$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

tools-test: tools ## Smoke-test the CLI tools end to end
	@scripts/test-tools.sh

# --- Examples -----------------------------------------------------------------
examples: $(LIB) tools ## Build every example under examples/
	@for d in $(EXAMPLE_DIRS); do $(MAKE) -C $$d CC=$(CC) || exit 1; done

# --- Documentation target -----------------------------------------------------
docs: ## Generate Sphinx documentation
	@mkdir -p $(BUILD)/docs/doxygen
	@test -d $(DOCS_VENV) || python3 -m venv $(DOCS_VENV)
	@$(DOCS_PIP) install -q -r docs/requirements.txt
	@doxygen Doxyfile
	@$(DOCS_SPHINX) -q -b html docs $(BUILD)/docs/html
	@echo "Documentation: $(BUILD)/docs/html/index.html"

# --- Utility targets ----------------------------------------------------------
compile_commands: $(COREOBJ) $(HCOREOBJ) $(HOSTEDOBJ) ## Generate compile_commands.json (clang only)
	@if ls $(JSON)/*.json >/dev/null 2>&1; then \
		cat $(JSON)/*.json > $(JSON)/temp.json && \
		sed -e '1s/^/[\n/' -e '$$s/,$$/\n]/' $(JSON)/temp.json > compile_commands.json && \
		rm $(JSON)/temp.json && echo "wrote compile_commands.json"; \
	else \
		echo "compile_commands requires clang (-MJ); nothing generated"; \
	fi

SAN_FLAGS := -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer

test-asan: clean ## Rebuild tests with ASan+UBSan and run the full test suite
	@$(MAKE) tests \
		CFLAGS="-Wall -Wextra -std=c99 -O1 -g -DCFGPACK_LZ4 -DCFGPACK_HEATSHRINK -DCFGPACK_LITTLEFS $(SAN_FLAGS)" \
		LDFLAGS="$(SAN_FLAGS)"
	@scripts/run-tests.sh

COV_FLAGS := -fprofile-instr-generate -fcoverage-mapping

coverage: clean ## Rebuild with LLVM coverage, run tests, and generate report
	@$(MAKE) tests \
		CFLAGS="-Wall -Wextra -std=c99 -O0 -g -DCFGPACK_LZ4 -DCFGPACK_HEATSHRINK -DCFGPACK_LITTLEFS $(COV_FLAGS)" \
		LDFLAGS="$(COV_FLAGS)"
	@scripts/run-coverage.sh

stack-usage-O0: clean ## Build with -fstack-usage at -O0 and report per-function stack sizes
	@$(MAKE) all CFLAGS="-Wall -Wextra -std=c99 -DCFGPACK_LZ4 -DCFGPACK_HEATSHRINK -DCFGPACK_LITTLEFS -O0 -fstack-usage"
	@echo "--- Stack usage at -O0 ---"
	@find $(OBJ) -name '*.su' -exec cat {} + | sort -t "$(TAB)" -k2 -n -r

stack-usage-Os: clean ## Build with -fstack-usage at -Os and report per-function stack sizes
	@$(MAKE) all CFLAGS="-Wall -Wextra -std=c99 -DCFGPACK_LZ4 -DCFGPACK_HEATSHRINK -DCFGPACK_LITTLEFS -Os -fstack-usage"
	@echo "--- Stack usage at -Os ---"
	@find $(OBJ) -name '*.su' -exec cat {} + | sort -t "$(TAB)" -k2 -n -r

format: ## Auto-format all source files with clang-format
	$(CLANG_FORMAT) -i $(FORMAT_FILES)

format-check: ## Check formatting without modifying files
	$(CLANG_FORMAT) --dry-run --Werror $(FORMAT_FILES)

clean: ## Remove build artifacts
	-@$(RM) -rvf -- $(BUILD) compile_commands.json .cache
	-@$(RM) -rf -- tests/fuzz/corpus_map/* tests/fuzz/corpus_json/* \
		tests/fuzz/corpus_msgpack/* tests/fuzz/corpus_msgpack_mutator/* \
		tests/fuzz/corpus_pagein/* tests/fuzz/corpus_decompress/* \
		tests/fuzz/corpus_decode/*

clean-docs: ## Remove generated documentation
	-@$(RM) -rf -- $(BUILD)/docs docs/.venv

help: ## List targets with descriptions
	@awk 'BEGIN {FS = ":.*##"} /^[a-zA-Z0-9][^:]*:.*##/ {printf "%-16s %s\n", $$1, $$2}' $(MAKEFILE_LIST)

# --- Fuzz targets (delegated to tests/fuzz/Makefile) ---------------------------
fuzz: ## Build all fuzz harnesses (requires clang with libFuzzer)
	@$(MAKE) -C tests/fuzz fuzz ROOT=$(CURDIR) BUILD=$(CURDIR)/$(BUILD) OUT=$(CURDIR)/$(OUT) CC=$(CC)

# --- Phony / Includes ---------------------------------------------------------
.PHONY: all hosted tests tools tools-test examples clean clean-docs help docs \
        format format-check compile_commands fuzz test-asan coverage \
        stack-usage-O0 stack-usage-Os
-include $(shell find $(BUILD) -name '*.d' 2>/dev/null)
