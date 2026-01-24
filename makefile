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

SRC      := $(foreach d,$(SRCDIRS),$(wildcard $(d)/*.c))
TESTSRC  := $(foreach d,$(TESTDIRS),$(wildcard $(d)/*.c))

OBJECTS  := $(SRC:%.c=$(OBJ)/%.o)
TESTOBJ  := $(TESTSRC:%.c=$(OBJ)/%.o)
TESTBINS := $(filter-out $(OUT)/test,$(TESTSRC:tests/%.c=$(OUT)/%))
TESTCOMMON := $(OBJ)/tests/test.o
DEPS     := $(OBJECTS:.o=.d) $(TESTOBJ:.o=.d)

vpath %.c $(SRCDIRS) $(TESTDIRS)

.DEFAULT_GOAL := all

all: $(LIB) ## Build static library

$(LIB): $(OBJECTS)
	@mkdir -p $(OUT)
	@echo "AR $(LIB)"
	@$(AR) rcs $(LIB) $(OBJECTS)

$(OBJ)/%.o: %.c
	@mkdir -p $(@D) $(JSON)
	@echo "CC $<"
	@$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -MJ $(JSON)/$(@F).json -c $< -o $@

tests: $(TESTBINS) ## Build and run all tests
	@for t in $(TESTBINS); do ./$$t || exit 1; done

$(OUT)/%: $(OBJ)/tests/%.o $(TESTCOMMON) $(LIB)
	@mkdir -p $(OUT)
	@echo "LD $@"
	@$(CC) $(LDFLAGS) -o $@ $< $(TESTCOMMON) $(LIB) $(LDLIBS)

clean: ## Remove build artifacts
	-@$(RM) -rvf -- $(BUILD) compile_commands.json .cache

help: ## List targets with descriptions
	@awk 'BEGIN {FS = ":.*##"} /^[a-zA-Z0-9][^:]*:.*##/ {printf "%-12s %s\n", $$1, $$2}' $(MAKEFILE_LIST)

# Generate compile_commands.json from dep JSON
compile_commands.json: $(OBJECTS)
	@mkdir -p $(OUT)
	@cat $(JSON)/* > $(JSON)/temp.json
	@sed -e '1s/^/[\n/' -e '$$s/,$$/\n]/' $(JSON)/temp.json > compile_commands.json
	@rm $(JSON)/temp.json

.PHONY: all tests clean help

-include $(DEPS)
