# CFGPack

[![CI](https://github.com/Arsievert/cfgpack/actions/workflows/ci.yml/badge.svg)](https://github.com/Arsievert/cfgpack/actions/workflows/ci.yml)
[![Coverage](https://img.shields.io/endpoint?url=https://arsievert.github.io/cfgpack/coverage.json)](https://arsievert.github.io/cfgpack/)

A MessagePack-based configuration library for embedded systems.

**Design constraints:** no heap allocation. All buffers are caller-owned. Hard caps: max 128 schema entries. Schema descriptions are ignored/dropped.

**Thread safety:** All core operations operate exclusively on the caller-provided `cfgpack_ctx_t` — no global state is used. Distinct contexts may be used concurrently from different threads without synchronization. Concurrent access to the *same* context requires external locking. Exception: `cfgpack_pagein_heatshrink()` uses a static decoder instance and is not thread-safe even across distinct contexts; the LZ4 path has no such limitation.

## What It Does

- Defines a fixed-cap schema (up to 128 entries) with typed values (u/i 8–64, f32/f64, str/fstr) and 5-char names.
- Parses `.map`, JSON, and MessagePack binary schemas into caller-owned buffers; no heap allocations.
- Supports **default values** for schema entries, automatically applied at initialization.
- Supports set/get by index and by schema name with type/length validation.
- Encodes/decodes MessagePack maps; pageout to buffer or file, pagein from buffer or file, with size caps.
- **CRC-32C integrity checking**: All serialized blobs include a 4-byte CRC-32C (Castagnoli) trailer, verified automatically on pagein.
- **Measure-then-allocate**: `cfgpack_pageout_measure()` computes exact serialized size before encoding, matching the existing schema measure pattern.
- **Schema versioning**: Embeds schema name in serialized blobs for version detection.
- **Remapping**: Migrates config between schema versions with index remapping, type widening, and automatic default restoration for new entries.

## Recommended Workflow

The intended workflow is to author schemas as `.map` text files, convert them
to compact MessagePack binary at build time, and optionally compress them.
Your application then loads the binary blob directly — no text parsing on the
device.

![Build-time pipeline: vehicle.map source is converted by cfgpack-schema-pack into a vehicle.msgpack schema blob, which can optionally be compressed via cfgpack-compress (LZ4 or heatshrink) into vehicle.msgpack.lz4](docs/build_pipeline.svg)

```bash
# 1. Write your schema as a .map file (see Map Format below)

# 2. Validate your schema
./build/out/cfgpack-schema-validate vehicle.map

# 3. Convert to MessagePack binary (50-60% smaller than JSON, no tokenizer needed)
./build/out/cfgpack-schema-pack vehicle.map vehicle.msgpack

# 4. (Optional) Compress with LZ4 or heatshrink
./build/out/cfgpack-compress lz4 vehicle.msgpack vehicle.msgpack.lz4
```

On the device:

```c
/* Decompress if needed (LZ4 example) */
LZ4_decompress_safe(compressed_data, scratch, compressed_len,
                    decompressed_size);

/* Measure, allocate, and parse the msgpack schema */
cfgpack_schema_measure_msgpack(data, len, &m, &err);  /* measure first */
/* ... allocate buffers from m ... */
cfgpack_schema_parse_msgpack(data, len, &opts);        /* then parse */
```

This keeps the `.map` files human-readable in your repo while shipping the
smallest possible binary to the device. See [`examples/fleet_gateway/`](examples/fleet_gateway/)
for a complete example with LZ4-compressed msgpack schemas and multi-version
migration.

## Device Boot Lifecycle

CFGPack uses two distinct binary formats — they are not interchangeable:

| | Schema blob | Config blob |
|---|---|---|
| **Produced by** | `cfgpack_schema_write_msgpack()` / `cfgpack-schema-pack` tool | `cfgpack_pageout()` |
| **Consumed by** | `cfgpack_schema_parse_msgpack()` | `cfgpack_pagein_buf()` / `cfgpack_pagein_remap()` |
| **CRC-32C** | No | Yes (4-byte little-endian trailer) |
| **Contains** | Entry definitions (indices, names, types, defaults) | Runtime values keyed by index |
| **When used** | Boot — load schema from firmware image | Runtime — persist and restore config values |
| **Compression** | Decompress with LZ4/heatshrink directly | `cfgpack_pagein_lz4()` / `cfgpack_pagein_heatshrink()` |

Every boot loads the schema, then takes one of three paths depending on what's in flash:

![Device boot lifecycle: shared schema init phase decompresses (if needed), measures, parses, and initializes the context; then branches into three paths — first boot (no saved config, defaults used), same-version boot (cfgpack_pagein_buf with CRC verification), or firmware upgrade (peek_name then pagein_remap with index translation); all paths converge to the running application, which calls cfgpack_pageout on changes](docs/boot_lifecycle.svg)

**First boot** — No saved config in flash. Schema defaults are applied by `cfgpack_init()`. The application runs with defaults and eventually calls `cfgpack_pageout()` to persist changes. No pagein needed, CRC not involved.

**Same-version boot** — Flash contains a config blob from `cfgpack_pageout()`. Call `cfgpack_pagein_buf()` to load it. CRC-32C is verified automatically — if corrupt, `CFGPACK_ERR_CRC` is returned and the app can fall back to defaults.

**Firmware upgrade** — Flash contains a config blob from an older schema version. Load the new schema (already part of firmware), call `cfgpack_peek_name()` to identify the old version, select a remap table, and call `cfgpack_pagein_remap()` to load old values with index translation. Type widening is automatic; removed entries are skipped; new entries keep schema defaults. See [Schema Versioning](docs/versioning.md) and [`examples/fleet_gateway/`](examples/fleet_gateway/).

## Documentation

- [API Reference](docs/api-reference.md) — Complete API documentation (errors, values, schema, runtime, typed functions)
- [Schema Versioning](docs/versioning.md) — Version detection, migration, and type widening
- [Compression](docs/compression.md) — LZ4/heatshrink decompression support
- [LittleFS](docs/littlefs.md) — LittleFS flash storage wrappers
- [Stack Analysis](docs/stack-analysis.md) — Per-function stack frame sizes for embedded budgeting
- [Fuzz Testing](docs/fuzz-testing.md) — libFuzzer harnesses for parser and decode robustness

## Map Format

- First line: `<name> <version>` header (e.g., `vehicle 1` where `vehicle` is the schema name and `1` is the version). The schema name is embedded at reserved index 0 in serialized blobs for version detection during firmware upgrades.
- Lines follow the format: `INDEX NAME TYPE DEFAULT  # optional description`
  - `INDEX`: 0–65535
  - `NAME`: up to 5 characters (hard limit — longer names will fail to parse)
  - `TYPE`: one of the supported numeric/float/string types (str max 64, fstr max 16)
  - `DEFAULT`: default value for this entry (see below)
  - `# description`: optional trailing comment for documentation (not stored in binary)
- Comments: lines starting with `#` are ignored; inline `#` comments after the default value are also ignored.

### Default Values

Each schema entry requires a default value specification:
- `NIL` — no default; value must be explicitly set before use
- Integer literals: `0`, `42`, `-5`, `0xFF`, `0b1010`
- Float literals: `3.14`, `-1.5e-3`, `0.0`
- Quoted strings: `"hello"`, `""`, `"default value"`

Entries with defaults are automatically marked as present when `cfgpack_init()` is called.

### Example Schema

```
# vehicle.map - Configuration schema for a vehicle control system
# NOTE: Index 0 is reserved for schema name embedding; user entries start at 1.

vehicle 1

# IDENTIFICATION
1  id     u32   0        # Unique vehicle identifier
2  model  fstr  "MX500"  # Model code (max 16 chars)
3  vin    str   NIL      # Vehicle identification number (max 64 chars)

# OPERATIONAL LIMITS
10 maxsp  u16   120      # Maximum speed in km/h
11 minsp  u16   5        # Minimum speed before idle shutdown
12 accel  f32   2.5      # Acceleration limit in m/s^2
13 decel  f32   -3.0     # Deceleration limit in m/s^2

# SENSOR CALIBRATION
20 toff   i8    0        # Temperature sensor offset in degrees C
21 pscal  f64   1.0      # Pressure sensor scale factor
22 flags  u8    0x07     # Sensor enable bitmask
```

## Layout

- `include/cfgpack/` — public headers
  - `cfgpack.h` — umbrella header; include just this to get all public APIs.
  - `config.h` — build configuration (`CFGPACK_EMBEDDED`/`CFGPACK_HOSTED` modes).
  - `error.h` — error codes enum.
  - `value.h` — value types and limits (`CFGPACK_STR_MAX`, `CFGPACK_FSTR_MAX`).
  - `schema.h` — schema structs, parser/JSON APIs, and measure functions.
  - `msgpack.h` — minimal MessagePack buffer + encode/decode helpers.
  - `api.h` — main cfgpack runtime API (set/get/pagein/pageout/print/version/size).
  - `decompress.h` — optional LZ4/heatshrink decompression support.
  - `io_file.h` — optional FILE*-based convenience wrappers for desktop/POSIX systems.
  - `io_littlefs.h` — optional LittleFS-based convenience wrappers for flash storage.
- `src/` — library implementation (`core.c`, `crc32.c`, `io.c`, `io_file.c`, `io_littlefs.c`, `msgpack.c`, `schema_parser.c`, `tokens.c`, `wbuf.c`, `decompress.c`).
- `tests/` — C test programs plus sample data under `tests/data/`.
- `tools/` — CLI tools source (`cfgpack-compress.c` for LZ4/heatshrink compression, `cfgpack-schema-pack.c` for converting schemas to msgpack binary, `cfgpack-schema-validate.c` for schema validation).
- `examples/` — complete usage examples (`allocate-once/`, `datalogger/`, `flash_config/`, `fleet_gateway/`, `low_memory/`, `sensor_hub/`).
- `third_party/` — vendored dependencies (`lz4/`, `heatshrink/`, `littlefs/`).
- `Makefile` — builds `build/out/libcfgpack.a`, test binaries, and tools.

## Building

```bash
make              # builds build/out/libcfgpack.a
make tests        # builds all test binaries
make tools        # builds CLI tools (cfgpack-compress, cfgpack-schema-pack, cfgpack-schema-validate)
```

### Build Modes

| Mode | Default | stdio | Print Functions | Float Formatting |
|------|---------|-------|-----------------|------------------|
| `CFGPACK_EMBEDDED` | Yes | Not linked | Silent no-ops | Minimal (9 digits) |
| `CFGPACK_HOSTED` | No | Linked | Full printf | snprintf (%.17g) |

To compile in hosted mode:
```bash
$(CC) -DCFGPACK_HOSTED -Iinclude myapp.c -Lbuild/out -lcfgpack
```

## Testing

```bash
make tests
./scripts/run-tests.sh
```

Output:
```
Running tests...

  basic:          4/4 passed
  core_edge:      11/11 passed
  coverage:       27/27 passed
  decompress:     8/8 passed
  io_edge:        16/16 passed
  io_littlefs:    8/8 passed
  json_edge:      8/8 passed
  json_remap:     10/10 passed
  measure:        15/15 passed
  msgpack:        16/16 passed
  msgpack_decode: 11/11 passed
  msgpack_schema: 17/17 passed
  null_args:      40/40 passed
  parser_bounds:  23/23 passed
  parser:         3/3 passed
  runtime:        24/24 passed

TOTAL: 241/241 passed
```

### Fuzz Testing

Six [libFuzzer](https://llvm.org/docs/LibFuzzer.html) harnesses exercise the parsers and decode paths with randomized input. AddressSanitizer and UndefinedBehaviorSanitizer are enabled by default.

**Prerequisites (macOS):** Apple Clang does not ship libFuzzer. Install Homebrew LLVM:
```bash
brew install llvm
```
The build auto-detects Homebrew LLVM when the default `clang` lacks libFuzzer support.

**Build and run:**
```bash
make fuzz                  # build harnesses + generate seed corpus
scripts/run-fuzz.sh        # 60s per target (default)
scripts/run-fuzz.sh 10     # 10s per target
scripts/run-fuzz.sh 0      # run indefinitely (Ctrl-C to stop)
```

| Target | What it fuzzes |
|--------|----------------|
| `fuzz_parse_map` | `.map` text schema parser |
| `fuzz_parse_json` | JSON schema parser |
| `fuzz_parse_msgpack` | MessagePack binary schema parser |
| `fuzz_parse_msgpack_mutator` | Structure-aware msgpack schema fuzzer (custom mutator) |
| `fuzz_pagein` | `cfgpack_pagein_buf()` against a fixed schema |
| `fuzz_msgpack_decode` | All low-level msgpack decode functions |

See [Fuzz Testing](docs/fuzz-testing.md) for detailed documentation on the harness architecture, seed corpus generation, and crash investigation.

## Examples

Six complete examples are provided in [`examples/`](examples/). Each has its own Makefile — run with `cd examples/<name> && make run`.

