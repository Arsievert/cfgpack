CC       := clang
AR       := ar
CPPFLAGS := -Iinclude -Iinclude/cfgpack
CFLAGS   := -Wall -Wextra -std=c99
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
# File I/O wrapper (optional, for desktop/POSIX)
IOFILESRC := src/io_file.c

TESTSRC  := $(foreach d,$(TESTDIRS),$(wildcard $(d)/*.c))

COREOBJ  := $(CORESRC:%.c=$(OBJ)/%.o)
IOFILEOBJ := $(IOFILESRC:%.c=$(OBJ)/%.o)
OBJECTS  := $(COREOBJ) $(IOFILEOBJ)
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

tests: $(TESTBINS) ## Build and run all tests
	@for t in $(TESTBINS); do ./$$t || exit 1; done

# Tests link against core lib + io_file.o (for file convenience functions)
$(OUT)/%: $(OBJ)/tests/%.o $(TESTCOMMON) $(LIB) $(IOFILEOBJ)
	@mkdir -p $(OUT)
	@echo "LD $@"
	@$(CC) $(LDFLAGS) -o $@ $< $(TESTCOMMON) $(IOFILEOBJ) $(LIB) $(LDLIBS)

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

.PHONY: all tests clean clean-docs help doxygen

-include $(DEPS)
