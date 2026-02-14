# CFGPack

A MessagePack-based configuration library for embedded systems.

**Embedded profile:** no heap allocation. All buffers are caller-owned. Hard caps: max 128 schema entries. Schema descriptions are ignored/dropped.

## What It Does

- Defines a fixed-cap schema (up to 128 entries) with typed values (u/i 8–64, f32/f64, str/fstr) and 5-char names.
- Parses `.map` specs into caller-owned schema + entries; no heap allocations.
- Supports **default values** for schema entries, automatically applied at initialization.
- Initializes runtime with caller-provided value and presence buffers; tracks presence bits and supports set/get/print/size/version.
- Supports set/get by index and by schema name with type/length validation.
- Encodes/decodes MessagePack maps; pageout to buffer or file, pagein from buffer or file, with size caps.
- **Schema versioning**: Embeds schema name in serialized blobs for version detection.
- **Remapping**: Migrates config between schema versions with index remapping, type widening, and automatic default restoration for new entries.
- Returns explicit errors for parse/encode/decode/type/bounds/IO issues.

## Documentation

- [API Reference](docs/source/api-reference.md) — Complete API documentation (errors, values, schema, runtime, typed functions)
- [Schema Versioning](docs/source/versioning.md) — Version detection, migration, and type widening
- [Compression](docs/source/compression.md) — LZ4/heatshrink decompression support

## Map Format

- First line: `<name> <version>` header (e.g., `vehicle 1` where `vehicle` is the schema name and `1` is the version). The schema name is embedded at reserved index 0 in serialized blobs for version detection during firmware upgrades.
- Lines follow the format: `INDEX NAME TYPE DEFAULT  # optional description`
  - `INDEX`: 0–65535
  - `NAME`: up to 5 characters
  - `TYPE`: one of the supported numeric/float/string types (str max 64, fstr max 16)
  - `DEFAULT`: default value for this entry (see below)
  - `# description`: optional trailing comment for documentation (not stored in binary)
- Comments: lines starting with `#` are ignored; inline `#` comments after the default value are also ignored.
- Hard caps in this profile: 128 entries.

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
  - `config.h` — build configuration (`CFGPACK_EMBEDDED`/`CFGPACK_HOSTED` modes).
  - `error.h` — error codes enum.
  - `value.h` — value types and limits (`CFGPACK_STR_MAX`, `CFGPACK_FSTR_MAX`).
  - `schema.h` — schema structs, parser/JSON APIs, and measure functions.
  - `msgpack.h` — minimal MessagePack buffer + encode/decode helpers.
  - `api.h` — main cfgpack runtime API (set/get/pagein/pageout/print/version/size).
  - `decompress.h` — optional LZ4/heatshrink decompression support.
  - `io_file.h` — optional FILE*-based convenience wrappers for desktop/POSIX systems.
- `src/` — library implementation (`core.c`, `io.c`, `msgpack.c`, `schema_parser.c`, `decompress.c`).
- `tests/` — C test programs plus sample data under `tests/data/`.
- `tools/` — CLI tools source (`cfgpack-compress.c` for LZ4/heatshrink compression).
- `examples/` — complete usage examples (`allocate-once/`, `datalogger/`, `low_memory/`, `sensor_hub/`).
- `third_party/` — vendored dependencies (`lz4/`, `heatshrink/`).
- `Makefile` — builds `build/out/libcfgpack.a`, test binaries, and tools.

## Building

```bash
make              # builds build/out/libcfgpack.a
make tests        # builds all test binaries
make tools        # builds compression tool (build/out/cfgpack-compress)
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

  basic:         4/4 passed
  core_edge:     11/11 passed
  decompress:    8/8 passed
  io_edge:       16/16 passed
  json_edge:     8/8 passed
  json_remap:    10/10 passed
  measure:       15/15 passed
  msgpack:       16/16 passed
  parser_bounds: 23/23 passed
  parser:        3/3 passed
  runtime:       24/24 passed

TOTAL: 138/138 passed
```

## Examples

Four complete examples are provided in the `examples/` directory:

### allocate-once

Dynamic allocation example demonstrating two-phase init: `cfgpack_schema_measure()` to learn sizes (32 bytes of stack), then right-sized `malloc`. This replaces the old discovery-parse pattern that required ~8KB of temporary stack buffers.

```bash
cd examples/allocate-once && make run
```

### datalogger

Basic data logger demonstrating schema parsing, typed convenience functions, and serialization.

```bash
cd examples/datalogger && make run
```

### low_memory

HVAC zone controller demonstrating the measure API for right-sized allocation and a full v1 -> v2 schema migration covering all five migration scenarios (keep, widen, move, remove, add). Shows how `cfgpack_schema_measure()` eliminates compile-time guessing of buffer sizes.

```bash
cd examples/low_memory && make run
```

### sensor_hub

IoT sensor hub demonstrating compressed schema loading with heatshrink (~22% compression ratio).

```bash
cd examples/sensor_hub && make run
```

## Roadmap

- Rust library alongside C implementation.
