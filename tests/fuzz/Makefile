# tests/fuzz/Makefile — libFuzzer harnesses + seed corpus generator
#
# Invoked from the project root:
#   make fuzz           Build all fuzz harnesses
#   make gen-seeds      Build & run seed corpus generator
#
# Requires Clang with libFuzzer support:
#   Linux:  apt install clang (or equivalent) — works out of the box.
#   macOS:  brew install llvm  (Apple Clang does NOT ship libFuzzer)
#
# You can override the compiler:  make fuzz CC=/path/to/clang

# --- Paths (relative to project root, passed in by parent makefile) -----------
ROOT     ?= ../..
BUILD    ?= $(ROOT)/build
OUT      ?= $(BUILD)/out
CC       ?= clang

# --- Include / compile flags inherited from the project -----------------------
CPPFLAGS := -I$(ROOT)/include -I$(ROOT)/include/cfgpack \
            -I$(ROOT)/third_party/lz4 -I$(ROOT)/third_party/heatshrink
CFLAGS   := -Wall -Wextra -std=c99 -DCFGPACK_LZ4 -DCFGPACK_HEATSHRINK -DCFGPACK_HOSTED

# --- Library sources (compiled directly — sanitizers need both compile + link) -
LIBSRC := $(ROOT)/src/core.c           \
          $(ROOT)/src/crc32.c           \
          $(ROOT)/src/decompress.c      \
          $(ROOT)/src/io.c              \
          $(ROOT)/src/msgpack.c         \
          $(ROOT)/src/schema_parser.c   \
          $(ROOT)/src/tokens.c          \
          $(ROOT)/src/wbuf.c            \
          $(ROOT)/src/io_file.c         \
          $(ROOT)/third_party/lz4/lz4.c \
          $(ROOT)/third_party/heatshrink/heatshrink_decoder.c

# --- Platform detection & toolchain validation --------------------------------
UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
  # Check whether the selected CC actually ships libFuzzer.
  _FUZZ_RT := $(shell $(CC) --print-resource-dir 2>/dev/null)/lib/darwin/libclang_rt.fuzzer_osx.a
  ifeq ($(wildcard $(_FUZZ_RT)),)
    # Current CC (likely Apple Clang) lacks libFuzzer — try Homebrew LLVM.
    _BREW_LLVM := $(shell brew --prefix llvm 2>/dev/null)
    _BREW_CLANG := $(_BREW_LLVM)/bin/clang
    ifneq ($(wildcard $(_BREW_CLANG)),)
      $(info [fuzz] Apple Clang lacks libFuzzer — using Homebrew LLVM at $(_BREW_LLVM))
      override CC := $(_BREW_CLANG)
    else
      $(error libFuzzer not found. Install Homebrew LLVM: brew install llvm)
    endif
  endif

  # Point the linker at LLVM's libc++ so the fuzzer runtime resolves.
  _LLVM_PREFIX := $(shell dirname $(shell dirname $(CC)))
  _EXTRA_LDFLAGS := -L$(_LLVM_PREFIX)/lib/c++ -Wl,-rpath,$(_LLVM_PREFIX)/lib/c++
endif

# --- Fuzz compiler / linker flags ---------------------------------------------
FUZZ_CFLAGS  := $(CPPFLAGS) $(CFLAGS) -fsanitize=fuzzer,address,undefined \
                -fno-omit-frame-pointer -g
FUZZ_LDFLAGS := -fsanitize=fuzzer,address,undefined $(_EXTRA_LDFLAGS)

# --- Harness sources & binaries -----------------------------------------------
FUZZSRC := fuzz_parse_map.c    \
           fuzz_parse_json.c   \
           fuzz_parse_msgpack.c \
           fuzz_parse_msgpack_mutator.c \
           fuzz_pagein.c        \
           fuzz_msgpack_decode.c
FUZZBINS := $(FUZZSRC:%.c=$(OUT)/%)

# --- Seed corpus generator ----------------------------------------------------
GEN_SEEDS     := $(OUT)/gen_seeds
GEN_SEEDS_SRC := gen_seeds.c

# --- Targets ------------------------------------------------------------------
.DEFAULT_GOAL := fuzz

fuzz: gen-seeds $(FUZZBINS)

$(OUT)/fuzz_%: fuzz_%.c $(LIBSRC)
	@mkdir -p $(OUT)
	@echo "CC (fuzz) $@"
	@$(CC) $(FUZZ_CFLAGS) $(FUZZ_LDFLAGS) -o $@ $< $(LIBSRC)

gen-seeds: $(GEN_SEEDS)
	@mkdir -p $(ROOT)/tests/fuzz/corpus_json \
	          $(ROOT)/tests/fuzz/corpus_msgpack \
	          $(ROOT)/tests/fuzz/corpus_pagein \
	          $(ROOT)/tests/fuzz/corpus_decode \
	          $(ROOT)/tests/fuzz/corpus_msgpack_mutator \
	          $(ROOT)/tests/fuzz/corpus_map
	@cd $(ROOT) && $(GEN_SEEDS)

$(GEN_SEEDS): $(GEN_SEEDS_SRC) $(LIBSRC)
	@mkdir -p $(OUT)
	@echo "CC $(GEN_SEEDS)"
	@$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(GEN_SEEDS_SRC) $(LIBSRC)

.PHONY: fuzz gen-seeds
