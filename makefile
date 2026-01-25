CC       := clang
AR       := ar
CPPFLAGS := -Iinclude -Iinclude/cfgpack -Ithird_party/lz4 -Ithird_party/heatshrink
CFLAGS   := -Wall -Wextra -std=c99 -DCFGPACK_LZ4 -DCFGPACK_HEATSHRINK
CFLAGS_HOSTED := $(CFLAGS) -DCFGPACK_HOSTED
LDFLAGS  :=
LDLIBS   :=

BUILD    := build
OUT      := $(BUILD)/out
OBJ      := $(BUILD)/obj
JSON     := $(BUILD)/json

LIB      := $(OUT)/libcfgpack.a
TESTBIN  := $(OUT)/tests

SRCDIRS  := src
TESTDIRS := tests
TESTDATA := tests/data

# Core library sources (excludes io_file.c for embedded use)
CORESRC  := $(filter-out src/io_file.c,$(foreach d,$(SRCDIRS),$(wildcard $(d)/*.c)))
CORESRC  += third_party/lz4/lz4.c third_party/heatshrink/heatshrink_decoder.c

# File I/O wrapper (optional, for desktop/POSIX)
IOFILESRC := src/io_file.c

# Compression tool
COMPRESS_TOOL := $(OUT)/cfgpack-compress
COMPRESS_SRC  := tools/cfgpack-compress.c
COMPRESS_DEPS := third_party/lz4/lz4.c third_party/heatshrink/heatshrink_encoder.c

# Encoder libraries needed for tests (to generate compressed test data)
ENCODER_DEPS := third_party/heatshrink/heatshrink_encoder.c

TESTSRC  := $(foreach d,$(TESTDIRS),$(wildcard $(d)/*.c))

COREOBJ  := $(CORESRC:%.c=$(OBJ)/%.o)
IOFILEOBJ := $(IOFILESRC:%.c=$(OBJ)/%.o)
ENCODEROBJ := $(ENCODER_DEPS:%.c=$(OBJ)/%.o)
OBJECTS  := $(COREOBJ) $(IOFILEOBJ) $(ENCODEROBJ)
TESTOBJ  := $(TESTSRC:%.c=$(OBJ)/%.o)
TESTBINS := $(filter-out $(OUT)/test,$(TESTSRC:tests/%.c=$(OUT)/%))
TESTCOMMON := $(OBJ)/tests/test.o
DEPS     := $(OBJECTS:.o=.d) $(TESTOBJ:.o=.d)

vpath %.c $(SRCDIRS) $(TESTDIRS)

.DEFAULT_GOAL := all

all: $(LIB) ## Build static library (core only, no file I/O)

# Core library without io_file.c
$(LIB): $(COREOBJ)
	@mkdir -p $(OUT)
	@echo "AR $(LIB)"
	@$(AR) rcs $(LIB) $(COREOBJ)

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

tests: $(TESTBINS) ## Build all test binaries

# Tests link against core lib + io_file.o + encoder (for decompression tests)
$(OUT)/%: $(OBJ)/tests/%.o $(TESTCOMMON) $(LIB) $(IOFILEOBJ) $(ENCODEROBJ)
	@mkdir -p $(OUT)
	@echo "LD $@"
	@$(CC) $(LDFLAGS) -o $@ $< $(TESTCOMMON) $(IOFILEOBJ) $(ENCODEROBJ) $(LIB) $(LDLIBS)

clean: ## Remove build artifacts
	-@$(RM) -rvf -- $(BUILD) compile_commands.json .cache

clean-docs: ## Remove generated documentation
	-@$(RM) -rf -- $(BUILD)/docs

help: ## List targets with descriptions
	@awk 'BEGIN {FS = ":.*##"} /^[a-zA-Z0-9][^:]*:.*##/ {printf "%-16s %s\n", $$1, $$2}' $(MAKEFILE_LIST)

compile_commands: $(OBJECTS) ## Generate compile_commands.json from dep JSON
	@mkdir -p $(OUT)
	@cat $(JSON)/* > $(JSON)/temp.json
	@sed -e '1s/^/[\n/' -e '$$s/,$$/\n]/' $(JSON)/temp.json > compile_commands.json
	@rm $(JSON)/temp.json

docs: ## Generate Doxygen documentation
	@mkdir -p $(BUILD)/docs
	doxygen Doxyfile

tools: $(COMPRESS_TOOL) ## Build compression tool

$(COMPRESS_TOOL): $(COMPRESS_SRC) $(COMPRESS_DEPS)
	@mkdir -p $(OUT)
	@echo "CC $(COMPRESS_TOOL)"
	@$(CC) $(CFLAGS_HOSTED) -Ithird_party/lz4 -Ithird_party/heatshrink -o $@ $(COMPRESS_SRC) $(COMPRESS_DEPS)

.PHONY: all tests clean clean-docs help doxygen tools

-include $(DEPS)
