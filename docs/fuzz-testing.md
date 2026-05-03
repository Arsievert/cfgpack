# Fuzz Testing

CFGPack includes six [libFuzzer](https://llvm.org/docs/LibFuzzer.html) harnesses that exercise the parsers and decode paths with randomized input. All harnesses are compiled with AddressSanitizer (ASan) and UndefinedBehaviorSanitizer (UBSan) to catch memory errors and undefined behavior at runtime.

## Why Fuzz the Parsers?

CFGPack's schema parsers (`.map`, JSON, and MessagePack binary) accept external input that may be corrupted, truncated, or adversarial — especially when loading configuration from flash storage or receiving it over a network. The `cfgpack_pagein_buf()` path also deserializes untrusted MessagePack data. Fuzzing these entry points provides confidence that malformed input is rejected cleanly rather than triggering buffer overflows, out-of-bounds reads, or undefined behavior.

## Fuzz Targets

Six harness files live in `tests/fuzz/`:

| Harness | Source | What it exercises |
|---------|--------|-------------------|
| `fuzz_parse_map` | `fuzz_parse_map.c` | `.map` text schema parser (`cfgpack_parse_schema`) |
| `fuzz_parse_json` | `fuzz_parse_json.c` | JSON schema parser (`cfgpack_schema_parse_json`) |
| `fuzz_parse_msgpack` | `fuzz_parse_msgpack.c` | MessagePack binary schema parser (`cfgpack_schema_parse_msgpack`) |
| `fuzz_parse_msgpack_mutator` | `fuzz_parse_msgpack_mutator.c` | Structure-aware msgpack schema fuzzer using `LLVMFuzzerCustomMutator` — generates valid msgpack schema blobs with targeted corruption to reach deeper parser paths; on successful parse, exercises init and pageout/pagein roundtrip |
| `fuzz_pagein` | `fuzz_pagein.c` | `cfgpack_pagein_buf()` and `cfgpack_pagein_remap()` against a fixed schema — wraps fuzzer input with a valid CRC-32C trailer to exercise decoder paths, and also feeds raw input to exercise the CRC rejection path |
| `fuzz_msgpack_decode` | `fuzz_msgpack_decode.c` | All low-level msgpack decode functions (`cfgpack_msgpack_decode_uint64`, `_int64`, `_f32`, `_f64`, `_str`, `_map_header`, `_skip_value`) |

Each harness implements libFuzzer's `LLVMFuzzerTestOneInput` entry point, allocates a stack-based `cfgpack_ctx_t`, and feeds the fuzzer-provided data directly to the target function. The `fuzz_parse_msgpack_mutator` harness additionally implements `LLVMFuzzerCustomMutator` to generate structurally valid msgpack schema blobs with 16 corruption modes (truncation, bitflips, wrong counts, type mismatches, duplicate names/indices, etc.), enabling coverage of parser code paths that random bytes alone are unlikely to reach. When parsing succeeds, the harness also initializes a runtime context and performs a `cfgpack_pageout`/`cfgpack_pagein_buf` roundtrip, exercising the encode and decode I/O paths with fuzzer-derived schema data. All harnesses are self-contained and do not use the heap.

## Prerequisites

### Linux

Any recent Clang (11+) ships libFuzzer. No extra setup needed.

```bash
sudo apt install clang   # Debian/Ubuntu
```

### macOS

Apple Clang does **not** ship libFuzzer. Install the full LLVM toolchain via Homebrew:

```bash
brew install llvm
```

The build system auto-detects Homebrew LLVM when the system `clang` lacks libFuzzer support. You do not need to manually set `CC`.

## Building

From the project root:

```bash
make fuzz
```

This delegates to the sub-makefile at `tests/fuzz/Makefile`, which:

1. Detects whether `CC` has libFuzzer support. On macOS, if Apple Clang is detected, it automatically switches to Homebrew LLVM.
2. Builds the seed corpus generator (`gen_seeds`) and runs it to populate the corpus directories.
3. Compiles all six fuzz harnesses with `-fsanitize=fuzzer,address,undefined`.

Binaries are placed in `build/out/`:

```
build/out/fuzz_parse_map
build/out/fuzz_parse_json
build/out/fuzz_parse_msgpack
build/out/fuzz_parse_msgpack_mutator
build/out/fuzz_pagein
build/out/fuzz_msgpack_decode
build/out/gen_seeds
```

### Why harnesses compile sources directly

Fuzz harnesses compile the library source files directly (`$(LIBSRC)`) rather than linking against `libcfgpack.a`. This is required because AddressSanitizer and UBSan instrument code at compile time — both the harness and the library code must be compiled with `-fsanitize=...` flags for the sanitizers to detect issues in library code.

## Seed Corpus

The `gen_seeds.c` program generates valid seed files across six corpus directories:

| Directory | Seeds | Description |
|-----------|------:|-------------|
| `tests/fuzz/corpus_map/` | 1 | A valid `.map` schema file |
| `tests/fuzz/corpus_json/` | 3 | Valid JSON schemas (minimal, typical, all types) |
| `tests/fuzz/corpus_msgpack/` | 1 | A valid msgpack binary schema |
| `tests/fuzz/corpus_msgpack_mutator/` | 4 | Small random byte sequences that parameterize the custom mutator |
| `tests/fuzz/corpus_pagein/` | 2 | Valid serialized config blobs (empty + populated) |
| `tests/fuzz/corpus_decode/` | 10 | Individual msgpack-encoded values (uint, int, float, string, map, etc.) |

Seeds are regenerated automatically every time `make fuzz` runs (the `fuzz` target depends on `gen-seeds`). Starting from valid inputs helps the fuzzer reach deeper code paths faster.

## Running

### Using the runner script

The `scripts/run-fuzz.sh` script runs all six targets sequentially with colored output:

```bash
scripts/run-fuzz.sh          # 60s per target (default)
scripts/run-fuzz.sh 300      # 300s per target
scripts/run-fuzz.sh 0        # run indefinitely (Ctrl-C to stop)
```

The script sets `-max_len=4096` and `-print_final_stats=1` for each target. Exit code is non-zero if any target crashes.

### Running a single target directly

You can run any harness directly with libFuzzer flags:

```bash
build/out/fuzz_parse_msgpack tests/fuzz/corpus_msgpack/ \
    -max_total_time=120 \
    -max_len=4096 \
    -print_final_stats=1
```

Useful libFuzzer flags:

| Flag | Description |
|------|-------------|
| `-max_total_time=N` | Stop after N seconds (0 = indefinite) |
| `-max_len=N` | Maximum input size in bytes |
| `-jobs=N` | Run N fuzzing jobs in parallel |
| `-workers=N` | Number of parallel worker processes |
| `-print_final_stats=1` | Print coverage and execution stats at exit |
| `-artifact_prefix=crashes/` | Save crash files to a directory |

See [libFuzzer documentation](https://llvm.org/docs/LibFuzzer.html) for the full list.

## Investigating Crashes

When libFuzzer finds a crash, it writes a reproducer file (e.g., `crash-<hash>`) to the current directory (or the path set by `-artifact_prefix`).

### Reproducing a crash

Run the harness with the crash file as an argument (not a directory):

```bash
build/out/fuzz_parse_msgpack crash-435c5524aa57e3619dd857148000af58d295e4f4
```

ASan will print a detailed report showing the crash type (heap-buffer-overflow, stack-buffer-overflow, use-after-free, etc.), the exact source location, and a stack trace.

### Debugging with lldb

```bash
lldb -- build/out/fuzz_parse_msgpack crash-435c5524aa57e3619dd857148000af58d295e4f4
(lldb) run
```

ASan stops at the exact point of the memory error. Use `bt` for a backtrace.

### Minimizing a crash input

libFuzzer can shrink a crash reproducer to its minimal triggering input:

```bash
build/out/fuzz_parse_msgpack -minimize_crash=1 -max_total_time=60 crash-<hash>
```

## Architecture

### Sub-makefile design

Fuzz build logic lives in `tests/fuzz/Makefile`, invoked by the root makefile via:

```makefile
fuzz:
	@$(MAKE) -C tests/fuzz fuzz ROOT=$(CURDIR) BUILD=$(CURDIR)/$(BUILD) OUT=$(CURDIR)/$(OUT) CC=$(CC)
```

This keeps the default `make` / `make tests` path free of fuzz-related overhead (no Homebrew detection, no LLVM checks). The sub-makefile receives absolute paths for `ROOT`, `BUILD`, and `OUT` so all paths resolve correctly from its working directory (`tests/fuzz/`).

On macOS, the sub-makefile uses `override CC` to replace Apple Clang with Homebrew LLVM. The `override` is necessary because the parent passes `CC=clang` on the command line, which takes precedence over regular variable assignments in the sub-makefile.
