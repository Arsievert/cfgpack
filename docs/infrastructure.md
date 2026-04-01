# CFGPack Infrastructure

This document describes in detail all the tools, systems, and methods used to build, test, format, fuzz, document, and maintain the CFGPack C library.

---

## Table of Contents

- [Build System](#build-system)
- [Compiler and Toolchain](#compiler-and-toolchain)
- [Conditional Compilation](#conditional-compilation)
- [Static Library Production](#static-library-production)
- [Test Infrastructure](#test-infrastructure)
- [Fuzz Testing](#fuzz-testing)
- [Code Formatting](#code-formatting)
- [Git Hooks](#git-hooks)
- [Documentation Generation](#documentation-generation)
- [CLI Tools](#cli-tools)
- [Third-Party Dependencies](#third-party-dependencies)
- [Stack Analysis](#stack-analysis)
- [Compile Commands Database](#compile-commands-database)
- [Project Setup](#project-setup)
- [Directory Layout](#directory-layout)

---

## Build System

The project uses a single GNU `Makefile` at the repository root. There are no CMake, Meson, or Autotools files. The build is intentionally minimal and self-contained, with no external package manager dependencies beyond a C compiler and standard POSIX tools.

### Build Directories

All build artifacts are placed under `build/`:

| Path | Contents |
|------|----------|
| `build/obj/` | Compiled `.o` object files (mirroring source tree) |
| `build/out/` | Final outputs: static library, test binaries, tool binaries |
| `build/json/` | Per-TU JSON fragments for `compile_commands.json` generation |
| `build/docs/` | Generated documentation (Doxygen XML, Sphinx HTML) |

### Makefile Targets

| Target | Description |
|--------|-------------|
| `all` (default) | Build `libcfgpack.a` -- core library only, no stdio |
| `tests` | Build all test binaries into `build/out/` |
| `tools` | Build `cfgpack-compress` and `cfgpack-schema-pack` |
| `fuzz` | Build all libFuzzer harnesses (delegated to `tests/fuzz/Makefile`) |
| `docs` | Generate Sphinx + Doxygen documentation |
| `format` | Auto-format all source with clang-format |
| `format-check` | Dry-run format check (fails on diff -- used by CI/hooks) |
| `compile_commands` | Generate `compile_commands.json` from per-TU JSON fragments |
| `stack-usage-O0` | Build at `-O0` with `-fstack-usage` and report per-function stack sizes |
| `stack-usage-Os` | Build at `-Os` with `-fstack-usage` and report per-function stack sizes |
| `clean` | Remove all build artifacts, compile_commands.json, fuzz corpora |
| `clean-docs` | Remove generated docs and the Python venv |
| `help` | List all targets with descriptions |

### Dependency Tracking

The Makefile uses Clang's `-MMD -MP` flags to emit `.d` dependency files alongside each `.o`. These are included at the bottom of the Makefile with `-include $(DEPS)`, giving automatic incremental rebuilds when any header changes.

---

## Compiler and Toolchain

| Setting | Value |
|---------|-------|
| Compiler | `clang` (overridable via `CC=`) |
| Archiver | `ar` |
| C standard | C99 (`-std=c99`) |
| Warning flags | `-Wall -Wextra` |
| Optimization | `-Os` (size-optimized, appropriate for embedded targets) |
| Formatter | `clang-format` (overridable via `CLANG_FORMAT=`) |

The Makefile uses `@` prefixes to silence command echo, printing shorter custom status lines instead (e.g., `CC src/core.c`, `AR build/out/libcfgpack.a`, `LD build/out/basic`).

---

## Conditional Compilation

The library has two build modes controlled by preprocessor defines:

### `CFGPACK_EMBEDDED` (default)

- No `<stdio.h>` dependency in the core library.
- `cfgpack_print()` / `cfgpack_print_all()` become silent no-ops.
- Float formatting uses a minimal internal implementation (no `snprintf`).
- `CFGPACK_PRINTF(...)` expands to `((void)0)`.

### `CFGPACK_HOSTED`

Enabled by compiling with `-DCFGPACK_HOSTED`. Provides full `printf`/`snprintf` support. The Makefile defines a separate `CFLAGS_HOSTED` variable and uses it for:
- All test object files (`tests/*.c`)
- `src/io_file.c` (file I/O wrapper requiring `<stdio.h>`)
- Both CLI tools

This is enforced with pattern-specific rules in the Makefile:

```makefile
# Test objects need CFLAGS_HOSTED for printf support
$(OBJ)/tests/%.o: tests/%.c
    $(CC) $(CPPFLAGS) $(CFLAGS_HOSTED) ...

# io_file.c needs CFLAGS_HOSTED for stdio
$(OBJ)/src/io_file.o: src/io_file.c
    $(CC) $(CPPFLAGS) $(CFLAGS_HOSTED) ...
```

### Optional Compression Support

Two additional feature flags gate decompression library support:
- `-DCFGPACK_LZ4` -- enables LZ4 decompression (on by default in CFLAGS)
- `-DCFGPACK_HEATSHRINK` -- enables Heatshrink decompression (on by default in CFLAGS)

These are also passed to Doxygen as `PREDEFINED` macros so that conditional API surfaces appear in documentation.

### Compile-Time Limits

Defined in `include/cfgpack/config.h` and overridable before including cfgpack headers:

| Macro | Default | Purpose |
|-------|---------|---------|
| `CFGPACK_MAX_ENTRIES` | 128 | Max schema entries; determines inline presence bitmap size |
| `CFGPACK_SKIP_MAX_DEPTH` | 32 | Max nesting depth for msgpack skip (32 levels = 256 bytes stack) |

---

## Static Library Production

The default `make` target produces `build/out/libcfgpack.a` -- a static archive of the core library. The core sources are:

```
src/core.c
src/decompress.c
src/io.c
src/msgpack.c
src/schema_parser.c
src/tokens.c
src/wbuf.c
third_party/lz4/lz4.c
third_party/heatshrink/heatshrink_decoder.c
```

Notably, `src/io_file.c` is **excluded** from the core library because it depends on `<stdio.h>`. It is compiled separately with hosted flags and linked into tests and tools as needed. This preserves the embedded-friendly, zero-stdio core.

The archiver creates the library with `ar rcs`.

---

## Test Infrastructure

### Framework

The project uses a custom minimal test framework defined in `tests/test.h` (116 lines). There are no external test framework dependencies.

**Core types and macros:**

| Macro/Type | Purpose |
|------------|---------|
| `test_result_t` | Enum: `TEST_OK` (0) or `TEST_FAIL` (1) |
| `TEST_CASE(name)` | Declares a test function: `test_result_t name(void)` |
| `CHECK(expr)` | Assert; on failure prints file:line and the expression, then returns `TEST_FAIL` |
| `CHECK_LOG(expr, fmt, ...)` | Assert with success logging |
| `LOG(fmt, ...)` | Print dimmed verbose output |
| `LOG_SECTION(title)` | Print a cyan section header |
| `LOG_HEX(label, buf, len)` | Hex-dump a buffer (up to 32 bytes) |
| `LOG_VALUE(label, val)` | Print a `cfgpack_value_t` with type information |

**Shared implementation** in `tests/test.c`:
- `test_case_result(name, result)` -- prints colored PASS/FAIL and returns the result.

### Test Binary Structure

Each test file has its own `main()` that runs its test cases and returns a combined pass/fail status:

```c
int main(void) {
    test_result_t overall = TEST_OK;
    overall |= (test_case_result("basic", test_basic_case()) != TEST_OK);
    overall |= (test_case_result("pageout_small_buffer",
                                 test_pageout_small_buffer()) != TEST_OK);
    /* ... */
    if (overall == TEST_OK) {
        printf("ALL PASS\n");
    } else {
        printf("SOME FAIL\n");
    }
    return (overall);
}
```

Each test binary links against: the core static library, `io_file.o`, the encoder dependency objects, and the shared `test.o`.

### Test Binaries

14 test files producing 13 test binaries (test.c is shared infrastructure, not a standalone binary):

| Binary | Source | Area |
|--------|--------|------|
| `basic` | `tests/basic.c` | Core set/get/pageout/pagein, defaults, typed convenience functions |
| `core_edge` | `tests/core_edge.c` | Edge cases in core API |
| `decompress` | `tests/decompress.c` | LZ4 and heatshrink decompression |
| `io_edge` | `tests/io_edge.c` | I/O edge cases |
| `json_edge` | `tests/json_edge.c` | JSON parser edge cases |
| `json_remap` | `tests/json_remap.c` | JSON remapping functionality |
| `measure` | `tests/measure.c` | Schema measure (pre-parse sizing) |
| `msgpack` | `tests/msgpack.c` | MessagePack encode/decode |
| `msgpack_schema` | `tests/msgpack_schema.c` | MessagePack schema handling |
| `null_args` | `tests/null_args.c` | NULL pointer and bounds validation |
| `parser` | `tests/parser.c` | Schema parser |
| `parser_bounds` | `tests/parser_bounds.c` | Parser boundary conditions |
| `runtime` | `tests/runtime.c` | Runtime behavior |

### Test Runner Script

`scripts/run-tests.sh` orchestrates test execution:

1. Iterates over a hardcoded list of test binary names.
2. Runs each binary, captures stdout/stderr and exit code.
3. Parses output for `PASS`/`FAIL` lines (excluding the `ALL PASS` summary).
4. Prints a per-binary summary with color-coded pass/fail counts.
5. Writes full output to `build/test.log`.
6. Prints a total summary and exits non-zero if any test failed.

Binaries that are missing are reported as `SKIP`.

---

## Fuzz Testing

### Architecture

Fuzz testing uses **LLVM libFuzzer** with AddressSanitizer (ASan) and UndefinedBehaviorSanitizer (UBSan). It is driven by a dedicated sub-Makefile at `tests/fuzz/Makefile`, invoked from the root Makefile via `make fuzz`.

### Platform Detection

The fuzz Makefile includes automatic platform detection for macOS:

1. Checks if the selected compiler's resource directory contains `libclang_rt.fuzzer_osx.a`.
2. If not (typically Apple Clang, which does not ship libFuzzer), it falls back to Homebrew LLVM at the path returned by `brew --prefix llvm`.
3. On macOS, it also adds linker flags (`-L` and `-Wl,-rpath`) to resolve LLVM's `libc++`.

On Linux, system Clang with libFuzzer typically works out of the box.

### Fuzz Flags

```
-fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer -g
```

Library sources are compiled directly into each fuzz binary (rather than linking the static archive) because sanitizers require instrumentation at both compile and link time.

### Fuzz Harnesses

6 fuzz targets, each implementing `LLVMFuzzerTestOneInput`:

| Target | Corpus Dir | What It Fuzzes |
|--------|-----------|----------------|
| `fuzz_parse_map` | `corpus_map/` | `.map` schema parser |
| `fuzz_parse_json` | `corpus_json/` | JSON schema parser |
| `fuzz_parse_msgpack` | `corpus_msgpack/` | MessagePack schema parser |
| `fuzz_parse_msgpack_mutator` | `corpus_msgpack_mutator/` | MessagePack parser with custom mutator |
| `fuzz_pagein` | `corpus_pagein/` | Configuration page-in (deserialization) |
| `fuzz_msgpack_decode` | `corpus_decode/` | Raw MessagePack decoding |

### Seed Corpus Generator

`tests/fuzz/gen_seeds.c` is a standalone hosted program that generates valid binary seed files for each corpus directory. It produces well-formed inputs (valid JSON schemas, valid MessagePack blobs, valid page-in buffers, etc.) that give the fuzzer a meaningful starting point. The generator runs automatically as a dependency of `make fuzz` via the `gen-seeds` target.

Corpus directories are gitignored and regenerated at build time.

### Running Fuzz Tests

`scripts/run-fuzz.sh` runs all fuzz targets sequentially:

- Default duration: 60 seconds per target.
- Configurable via argument: `scripts/run-fuzz.sh 300` for 300 seconds.
- Use `scripts/run-fuzz.sh 0` for indefinite fuzzing.
- Max input length: 4096 bytes.
- Prints the last 20 lines of libFuzzer output per target.
- Reports overall pass/fail status.

---

## Code Formatting

### clang-format Configuration

All formatting is enforced by `.clang-format` at the repository root, based on the LLVM style with project-specific overrides:

| Setting | Value | Notes |
|---------|-------|-------|
| `IndentWidth` | 4 | 4-space indent |
| `UseTab` | Never | Spaces only |
| `ColumnLimit` | 80 | 80-column hard limit |
| `BreakBeforeBraces` | Attach | K&R brace style |
| `InsertBraces` | true | Braces inserted for single-statement blocks |
| `PointerAlignment` | Right | `char *p` style |
| `BinPackParameters` | false | Function parameters one-per-line when wrapping |
| `BinPackArguments` | true | Arguments may be bin-packed |
| `AllowShortCaseLabelsOnASingleLine` | true | Compact case labels |
| `IndentPPDirectives` | BeforeHash | Preprocessor `#` indented |
| `ReflowComments` | false | Preserves section banner comments |
| `SortIncludes` | CaseSensitive | Auto-sorts includes |

### Include Ordering

Includes are automatically regrouped (`IncludeBlocks: Regroup`) into three priority groups separated by blank lines:

1. **Priority 1**: `cfgpack/` library headers (`#include "cfgpack/..."`)
2. **Priority 2**: Other local headers (`#include "tokens.h"`, etc.)
3. **Priority 3**: System headers (`#include <string.h>`, etc.)

### Formatting Targets

- `make format` -- auto-formats all `.c` and `.h` files under `src/`, `include/`, `tests/`, `examples/`, and `tools/` (excluding `third_party/`).
- `make format-check` -- dry-run with `--Werror`; exits non-zero if any file would change. Used by the pre-commit hook.

The set of files to format is discovered dynamically via `find`.

---

## Git Hooks

### Pre-Commit Hook

Located at `scripts/pre-commit` and symlinked into `.git/hooks/pre-commit` by the setup script.

The hook runs two stages on every commit:

1. **Format check** -- `make format-check`. Fails with a message directing the developer to run `make format`.
2. **Build and test** -- `make tests` followed by `scripts/run-tests.sh`. Fails with the test summary if any test fails.

The hook extracts and displays the `TOTAL:` summary line (with ANSI colors stripped) on success or failure.

It can be bypassed with `git commit --no-verify` when needed.

### Hook Installation

The symlink is created by `scripts/setup.sh`:
```bash
ln -sf ../../scripts/pre-commit "$ROOT_DIR/.git/hooks/pre-commit"
```

Using a symlink (rather than a copy) means the hook automatically stays in sync with the version in `scripts/`.

---

## Documentation Generation

Documentation uses a **Doxygen + Sphinx + Breathe** pipeline:

### Pipeline

1. **Doxygen** parses the public headers in `include/cfgpack/` and generates XML output to `build/docs/doxygen/xml/`.
2. **Breathe** (a Sphinx extension) reads the Doxygen XML and makes it available as Sphinx directives.
3. **Sphinx** renders the final HTML documentation to `build/docs/html/`.

### Doxygen Configuration

The `Doxyfile` at the repository root is configured for a C library:

| Setting | Value |
|---------|-------|
| `OPTIMIZE_OUTPUT_FOR_C` | YES |
| `INPUT` | `include/cfgpack` (public headers only) |
| `FILE_PATTERNS` | `*.h` |
| `GENERATE_XML` | YES (for Breathe) |
| `GENERATE_HTML` | NO (Sphinx handles HTML) |
| `JAVADOC_AUTOBRIEF` | YES |
| `TYPEDEF_HIDES_STRUCT` | YES |
| `HIDE_UNDOC_MEMBERS` | YES |
| `PREDEFINED` | `CFGPACK_LZ4 CFGPACK_HEATSHRINK` |
| All graph generation | NO (no Graphviz dependency) |

### Sphinx Configuration

`docs/source/conf.py` configures:

- **Extensions**: `breathe` (Doxygen bridge), `myst_parser` (Markdown support)
- **Theme**: `sphinx_rtd_theme` (Read the Docs theme)
- **Source formats**: both `.rst` (reStructuredText) and `.md` (Markdown via MyST)
- **MyST extensions**: `colon_fence`, `deflist`

### Python Dependencies

Managed via `docs/requirements.txt` and installed into an isolated venv at `docs/.venv/`:

```
sphinx>=7.0
breathe>=4.35
sphinx-rtd-theme>=2.0
myst-parser>=2.0
```

The `make docs` target automatically creates the venv and installs dependencies if they don't exist.

### Documentation Source Files

```
docs/source/
    conf.py              # Sphinx configuration
    index.rst            # Main table of contents
    getting-started.rst  # Getting started guide
    api.rst              # API overview (reStructuredText)
    api-reference.md     # API reference (Markdown)
    compression.md       # Compression documentation
    fuzz-testing.md      # Fuzz testing guide
    stack-analysis.md    # Stack usage analysis
    versioning.md        # Schema versioning
```

---

## CLI Tools

Two CLI tools are built with `make tools`:

### cfgpack-compress

**Source**: `tools/cfgpack-compress.c`

Compresses files with LZ4 or Heatshrink for use with the library's decompression support.

```
Usage: cfgpack-compress <algorithm> <input> <output>
Algorithms: lz4, heatshrink
```

- LZ4 output: 4-byte little-endian original size + raw compressed data.
- Heatshrink output: raw compressed data (window=8, lookahead=4).
- Links against vendored LZ4 and Heatshrink encoder sources directly.
- Max input/output: 64 KB.

### cfgpack-schema-pack

**Source**: `tools/cfgpack-schema-pack.c`

Converts `.map` or JSON schemas to MessagePack binary format for on-device loading.

```
Usage: cfgpack-schema-pack <input> <output>
```

- Auto-detects input format by file extension (`.json` = JSON, otherwise `.map`).
- Output is raw MessagePack binary for `cfgpack_schema_parse_msgpack()`.
- Links against the core library and `io_file.o`.

Both tools use exit code conventions: 0 = success, 1 = usage error, 2 = I/O error, 3 = processing error.

---

## Third-Party Dependencies

All third-party code is vendored in `third_party/` with no submodules or package managers:

### LZ4

```
third_party/lz4/
    lz4.c
    lz4.h
    LICENSE
```

LZ4 block-level compression/decompression. The decoder (`lz4.c`) is compiled into the core library when `CFGPACK_LZ4` is defined. The compressor is only needed by the `cfgpack-compress` tool.

### Heatshrink

```
third_party/heatshrink/
    heatshrink_decoder.c
    heatshrink_decoder.h
    heatshrink_encoder.c
    heatshrink_encoder.h
    heatshrink_common.h
    heatshrink_config.h
    LICENSE
```

Heatshrink is an LZSS-based compression library designed for embedded systems with very low memory overhead. The decoder is compiled into the core library when `CFGPACK_HEATSHRINK` is defined. The encoder is used by tests (to generate compressed test data) and the `cfgpack-compress` tool.

Both libraries are included via `-Ithird_party/lz4` and `-Ithird_party/heatshrink` in `CPPFLAGS`.

---

## Stack Analysis

The Makefile provides two targets for analyzing per-function stack usage, useful for verifying the library's suitability for stack-constrained embedded environments:

- **`make stack-usage-O0`** -- Builds the core library at `-O0` (no optimization) with Clang's `-fstack-usage` flag, then collects and sorts all `.su` files by stack size (descending).
- **`make stack-usage-Os`** -- Same at `-Os` (size-optimized, the default optimization level).

Both targets do a `clean` first to ensure all TUs are rebuilt with the stack-usage flag. The output shows each function's stack frame size, allowing developers to identify hot spots and verify that the library stays within embedded stack budgets.

---

## Compile Commands Database

`make compile_commands` generates a `compile_commands.json` at the repository root by concatenating per-TU JSON fragments produced by Clang's `-MJ` flag during compilation. This provides IDE integration (clangd, ccls, etc.) with accurate flags for each translation unit.

The per-TU fragments are written to `build/json/` during normal compilation and assembled with `sed` into a valid JSON array.

---

## Project Setup

`scripts/setup.sh` is a one-time setup script for new clones:

1. **Checks required tools**: `clang`, `clang-format`, `make`, `ar` -- reports pass/fail for each.
2. **Checks optional tools**: Looks for Homebrew LLVM on macOS (for fuzz testing), or verifies libFuzzer support on Linux.
3. **Installs the pre-commit hook**: Creates a symlink from `.git/hooks/pre-commit` to `scripts/pre-commit`.
4. **Verifies build and tests**: Runs `make tests` and `scripts/run-tests.sh` to confirm the project builds and all tests pass.
5. **Prints a summary**: Color-coded count of passed checks, warnings, and failures. Exits non-zero if any required check failed.

---

## Directory Layout

```
cfgpack/
├── Makefile                    # Root build system
├── Doxyfile                    # Doxygen configuration
├── .clang-format               # Code formatting rules
├── .gitignore                  # Git ignore rules
├── CLAUDE.md                   # AI assistant guidelines
├── include/cfgpack/            # Public headers
│   ├── cfgpack.h               #   Umbrella header
│   ├── api.h                   #   Core API
│   ├── schema.h                #   Schema types
│   ├── value.h                 #   Value types
│   ├── error.h                 #   Error codes
│   ├── config.h                #   Build configuration / compile-time limits
│   ├── msgpack.h               #   MessagePack encode/decode
│   ├── io_file.h               #   File I/O (hosted only)
│   └── decompress.h            #   Decompression API
├── src/                        # Library implementation
│   ├── core.c                  #   Core get/set/init/pageout/pagein
│   ├── schema_parser.c         #   .map and JSON schema parsing
│   ├── msgpack.c               #   MessagePack codec
│   ├── decompress.c            #   LZ4/heatshrink decompression
│   ├── io.c                    #   Buffer I/O
│   ├── io_file.c               #   File I/O (hosted, excluded from core lib)
│   ├── tokens.c                #   Tokenizer for schema parsing
│   └── wbuf.c                  #   Internal write buffer
├── tests/                      # Test files
│   ├── test.h                  #   Test framework header
│   ├── test.c                  #   Shared test infrastructure
│   ├── basic.c ... runtime.c   #   13 test binaries
│   ├── data/                   #   Test fixture files
│   └── fuzz/                   #   Fuzz testing
│       ├── Makefile            #     Fuzz build system
│       ├── gen_seeds.c         #     Seed corpus generator
│       ├── fuzz_parse_map.c    #     6 fuzz harness sources
│       ├── fuzz_parse_json.c
│       ├── fuzz_parse_msgpack.c
│       ├── fuzz_parse_msgpack_mutator.c
│       ├── fuzz_pagein.c
│       └── fuzz_msgpack_decode.c
├── tools/                      # CLI tools
│   ├── cfgpack-compress.c      #   LZ4/heatshrink compression tool
│   └── cfgpack-schema-pack.c   #   Schema-to-msgpack converter
├── examples/                   # Usage examples
│   ├── allocate-once/          #   One-shot allocation pattern
│   ├── datalogger/             #   Data logger example
│   ├── fleet_gateway/          #   Fleet gateway with schema versioning
│   ├── low_memory/             #   Low-memory embedded example
│   └── sensor_hub/             #   Sensor hub example
├── third_party/                # Vendored dependencies
│   ├── lz4/                    #   LZ4 compression library
│   └── heatshrink/             #   Heatshrink compression library
├── scripts/                    # Build/test/setup scripts
│   ├── setup.sh                #   One-time project setup
│   ├── run-tests.sh            #   Test runner with summary
│   ├── run-fuzz.sh             #   Fuzz test runner
│   └── pre-commit              #   Git pre-commit hook
└── docs/                       # Documentation sources
    ├── requirements.txt        #   Python dependencies for Sphinx
    └── source/                 #   Sphinx source files
        ├── conf.py             #     Sphinx configuration
        ├── index.rst
        ├── getting-started.rst
        ├── api.rst
        ├── api-reference.md
        ├── compression.md
        ├── fuzz-testing.md
        ├── stack-analysis.md
        └── versioning.md
```
