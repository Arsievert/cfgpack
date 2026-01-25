# CFGPack

This repository is the C-first implementation of a MessagePack-based configuration library.

**Embedded profile (current):** no heap allocation. All buffers are caller-owned. Hard caps: max 128 schema entries; max pagein/pageout size 4096 bytes. Schema descriptions are ignored/dropped.

## What It Does
- Defines a fixed-cap schema (up to 128 entries) with typed values (u/i 8–64, f32/f64, str/fstr) and 5-char names.
- Parses `.map` specs into caller-owned schema + entries; no heap allocations.
- Supports **default values** for schema entries, automatically applied at initialization.
- Initializes runtime with caller-provided value and presence buffers; tracks presence bits and supports set/get/print/size/version.
- Supports set/get by index and by schema name with type/length validation.
- Encodes/decodes MessagePack maps; pageout to buffer or file, pagein from buffer or file, with size caps.
- **Schema versioning**: Embeds schema name in serialized blobs for version detection.
- **Remapping**: Migrates config between schema versions with index remapping and type widening.
- Returns explicit errors for parse/encode/decode/type/bounds/IO issues.

## Map Format
- First line: `<name> <version>` header (e.g., `vehicle 1` where `vehicle` is the schema name and `1` is the version). The schema name is embedded at reserved index 0 in serialized blobs for version detection during firmware upgrades.
- Lines follow the format: `INDEX NAME TYPE DEFAULT  # optional description`
  - `INDEX`: 0–65535
  - `NAME`: up to 5 characters
  - `TYPE`: one of the supported numeric/float/string types (str max 64, fstr max 16)
  - `DEFAULT`: default value for this entry (see below)
  - `# description`: optional trailing comment for documentation (not stored in binary)
- Comments: lines starting with `#` are ignored; inline `#` comments after the default value are also ignored.
- Hard caps in this profile: 128 entries, 4096-byte page size.

### Default Values
Each schema entry requires a default value specification:
- `NIL` — no default; value must be explicitly set before use
- Integer literals: `0`, `42`, `-5`, `0xFF`, `0b1010`
- Float literals: `3.14`, `-1.5e-3`, `0.0`
- Quoted strings: `"hello"`, `""`, `"default value"`

Entries with defaults are automatically marked as present when `cfgpack_init()` is called.

### Example Schema
Below is a well-documented example `.map` file demonstrating the schema format:

```
# vehicle.map - Configuration schema for a vehicle control system
# Comments start with '#' and are ignored by the parser.
# NOTE: Index 0 is reserved for schema name embedding; user entries start at 1.

vehicle 1

# ─────────────────────────────────────────────────────────────────────────────
# IDENTIFICATION
# ─────────────────────────────────────────────────────────────────────────────
1  id     u32   0        # Unique vehicle identifier, assigned at manufacture
2  model  fstr  "MX500"  # Model code, e.g. "MX500" - max 16 chars
3  vin    str   NIL      # Vehicle identification number - max 64 chars

# ─────────────────────────────────────────────────────────────────────────────
# OPERATIONAL LIMITS
# ─────────────────────────────────────────────────────────────────────────────
10 maxsp  u16   120      # Maximum speed in km/h, range 0-65535
11 minsp  u16   5        # Minimum speed in km/h before idle shutdown
12 accel  f32   2.5      # Acceleration limit in m/s^2
13 decel  f32   -3.0     # Deceleration limit in m/s^2

# ─────────────────────────────────────────────────────────────────────────────
# SENSOR CALIBRATION
# ─────────────────────────────────────────────────────────────────────────────
20 toff   i8    0        # Temperature sensor offset in degrees C, -128 to 127
21 pscal  f64   1.0      # Pressure sensor scale factor, high precision
22 flags  u8    0x07     # Sensor enable bitmask: bit0=temp, bit1=pressure, bit2=gps

# ─────────────────────────────────────────────────────────────────────────────
# TELEMETRY
# ─────────────────────────────────────────────────────────────────────────────
30 tint   u32   1000     # Telemetry reporting interval in milliseconds
31 turl   str   ""       # Telemetry endpoint URL - max 64 chars
32 tkey   fstr  NIL      # Telemetry API key - max 16 chars, stored fixed-length
```

**Key points illustrated:**
- **Header line**: `vehicle 1` sets map name to "vehicle" and version to 1.
- **Comments**: Lines starting with `#` are ignored; use them for section headers and documentation.
- **Inline comments**: Text after `#` on entry lines documents each field but is not stored in binary output.
- **Reserved index**: Index 0 is reserved for schema name; user-defined entries should start at index 1.
- **Index gaps**: Indices need not be contiguous (1, 2, 3, then 10, 11, ...).
- **Type variety**: Shows u8/u16/u32, i8, f32/f64, str (variable up to 64), fstr (fixed up to 16).
- **Default values**: Each entry specifies a default—`NIL` for required fields, literals for optional fields with sensible defaults.
- **Hex literals**: `0x07` shows hexadecimal notation for bitmasks.


## Layout
- `include/cfgpack/` — public headers
  - `config.h` — build configuration (`CFGPACK_EMBEDDED`/`CFGPACK_HOSTED` modes).
  - `error.h` — error codes enum.
  - `value.h` — value types and limits (`CFGPACK_STR_MAX`, `CFGPACK_FSTR_MAX`).
  - `schema.h` — schema structs and parser/doc APIs.
  - `msgpack.h` — minimal MessagePack buffer + encode/decode helpers (fixed-capacity, caller storage).
  - `api.h` — main cfgpack runtime API (set/get/pagein/pageout/print/version/size) using caller buffers.
  - `decompress.h` — optional LZ4/heatshrink decompression support.
- `src/` — library implementation (`core.c`, `io.c`, `msgpack.c`, `schema_parser.c`, `decompress.c`).
- `tests/` — C test programs (`basic.c`, `parser.c`, `parser_bounds.c`, `runtime.c`, `decompress.c`) plus a shared harness in `tests/test.c` and sample data under `tests/data/`.
- `tools/` — CLI tools source (`cfgpack-compress.c` for LZ4/heatshrink compression).
- `examples/` — complete usage examples (`datalogger/`, `sensor_hub/`).
- `third_party/` — vendored dependencies (`lz4/`, `heatshrink/`).
- `Makefile` — builds `build/out/libcfgpack.a`, test binaries, and tools.

## Public APIs (snippets)
Include just `cfgpack/cfgpack.h`; it re-exports the public API surface.

### Errors
```c
#include "cfgpack/error.h"

typedef enum {
    CFGPACK_OK = 0,
    CFGPACK_ERR_PARSE = -1,
    CFGPACK_ERR_INVALID_TYPE = -2,
    CFGPACK_ERR_DUPLICATE = -3,
    CFGPACK_ERR_BOUNDS = -4,
    CFGPACK_ERR_MISSING = -5,
    CFGPACK_ERR_TYPE_MISMATCH = -6,
    CFGPACK_ERR_STR_TOO_LONG = -7,
    CFGPACK_ERR_IO = -8,
    CFGPACK_ERR_ENCODE = -9,
    CFGPACK_ERR_DECODE = -10,
    CFGPACK_ERR_RESERVED_INDEX = -11
} cfgpack_err_t;
```

### Values
```c
#include "cfgpack/value.h"

#define CFGPACK_STR_MAX 64
#define CFGPACK_FSTR_MAX 16

typedef enum {
    CFGPACK_TYPE_U8, CFGPACK_TYPE_U16, CFGPACK_TYPE_U32, CFGPACK_TYPE_U64,
    CFGPACK_TYPE_I8, CFGPACK_TYPE_I16, CFGPACK_TYPE_I32, CFGPACK_TYPE_I64,
    CFGPACK_TYPE_F32, CFGPACK_TYPE_F64,
    CFGPACK_TYPE_STR, CFGPACK_TYPE_FSTR
} cfgpack_type_t;

typedef struct {
    cfgpack_type_t type;
    union {
        uint64_t u64;
        int64_t i64;
        float   f32;
        double  f64;
        struct { uint16_t len; char data[CFGPACK_STR_MAX + 1]; } str;
        struct { uint8_t  len; char data[CFGPACK_FSTR_MAX + 1]; } fstr;
    } v;
} cfgpack_value_t;
```

### Schema
```c
#include "cfgpack/schema.h"

typedef struct {
    uint16_t index;       /* 0-65535 */
    char     name[6];     /* 5 chars + NUL */
    cfgpack_type_t type;  /* one of the supported types */
    uint8_t  has_default; /* 1 if default value exists, 0 otherwise */
} cfgpack_entry_t;

typedef struct {
    char map_name[64];
    uint32_t version;
    cfgpack_entry_t *entries;
    size_t entry_count;
} cfgpack_schema_t;

/* Parse schema from .map buffer */
cfgpack_err_t cfgpack_parse_schema(const char *data, size_t data_len,
                                   cfgpack_schema_t *out, cfgpack_entry_t *entries,
                                   size_t max_entries, cfgpack_value_t *defaults,
                                   cfgpack_parse_error_t *err);

/* Parse schema from JSON buffer */
cfgpack_err_t cfgpack_schema_parse_json(const char *data, size_t data_len,
                                        cfgpack_schema_t *out, cfgpack_entry_t *entries,
                                        size_t max_entries, cfgpack_value_t *defaults,
                                        cfgpack_parse_error_t *err);

void cfgpack_schema_free(cfgpack_schema_t *schema); /* no-op for caller-owned arrays */

/* Write schema to Markdown buffer */
cfgpack_err_t cfgpack_schema_write_markdown(const cfgpack_schema_t *schema,
                                            const cfgpack_value_t *defaults,
                                            char *out, size_t out_cap, size_t *out_len,
                                            cfgpack_parse_error_t *err);

/* Write schema to JSON buffer */
cfgpack_err_t cfgpack_schema_write_json(const cfgpack_schema_t *schema,
                                        const cfgpack_value_t *values,
                                        char *out, size_t out_cap, size_t *out_len,
                                        cfgpack_parse_error_t *err);
```

#### JSON Schema Format
Schemas can be read from and written to JSON for interoperability with other tools:
```json
{
  "name": "demo",
  "version": 1,
  "entries": [
    {"index": 1, "name": "speed", "type": "u16", "default": 100},
    {"index": 2, "name": "label", "type": "fstr", "default": "hello"},
    {"index": 3, "name": "desc", "type": "str", "default": null}
  ]
}
```
- `default` is `null` for NIL (no default value)
- Strings are JSON-escaped
- Numbers are output as JSON numbers (integers or floats)
- Note: Index 0 is reserved for schema name; user entries should start at 1
```

### Runtime API
```c
#include "cfgpack/api.h"

cfgpack_err_t cfgpack_init(cfgpack_ctx_t *ctx, const cfgpack_schema_t *schema,
                           cfgpack_value_t *values, size_t values_count,
                           const cfgpack_value_t *defaults,
                           uint8_t *present, size_t present_bytes);
void          cfgpack_free(cfgpack_ctx_t *ctx);
void          cfgpack_reset_to_defaults(cfgpack_ctx_t *ctx);

cfgpack_err_t cfgpack_set(cfgpack_ctx_t *ctx, uint16_t index, const cfgpack_value_t *value);
cfgpack_err_t cfgpack_get(const cfgpack_ctx_t *ctx, uint16_t index, cfgpack_value_t *out_value);
cfgpack_err_t cfgpack_set_by_name(cfgpack_ctx_t *ctx, const char *name, const cfgpack_value_t *value);
cfgpack_err_t cfgpack_get_by_name(const cfgpack_ctx_t *ctx, const char *name, cfgpack_value_t *out_value);

cfgpack_err_t cfgpack_pageout(const cfgpack_ctx_t *ctx, uint8_t *out, size_t out_cap, size_t *out_len);
cfgpack_err_t cfgpack_pageout_file(const cfgpack_ctx_t *ctx, const char *path, uint8_t *scratch, size_t scratch_cap);
cfgpack_err_t cfgpack_pagein_buf(cfgpack_ctx_t *ctx, const uint8_t *data, size_t len);
cfgpack_err_t cfgpack_pagein_file(cfgpack_ctx_t *ctx, const char *path, uint8_t *scratch, size_t scratch_cap);

/* Schema versioning and remapping */
cfgpack_err_t cfgpack_peek_name(const uint8_t *data, size_t len, char *out_name, size_t out_cap);
cfgpack_err_t cfgpack_pagein_remap(cfgpack_ctx_t *ctx, const uint8_t *data, size_t len,
                                    const cfgpack_remap_entry_t *remap, size_t remap_count);

cfgpack_err_t cfgpack_print(const cfgpack_ctx_t *ctx, uint16_t index);
cfgpack_err_t cfgpack_print_all(const cfgpack_ctx_t *ctx);

uint32_t cfgpack_get_version(const cfgpack_ctx_t *ctx);
size_t   cfgpack_get_size(const cfgpack_ctx_t *ctx);
```

### Typed Convenience Functions
For ergonomic access without manually constructing `cfgpack_value_t` structs, use the typed inline functions. All return `cfgpack_err_t` and validate type matches at runtime.

**Setters by index:**
```c
cfgpack_set_u8(ctx, index, val)    cfgpack_set_i8(ctx, index, val)
cfgpack_set_u16(ctx, index, val)   cfgpack_set_i16(ctx, index, val)
cfgpack_set_u32(ctx, index, val)   cfgpack_set_i32(ctx, index, val)
cfgpack_set_u64(ctx, index, val)   cfgpack_set_i64(ctx, index, val)
cfgpack_set_f32(ctx, index, val)   cfgpack_set_f64(ctx, index, val)
cfgpack_set_str(ctx, index, str, len)
cfgpack_set_fstr(ctx, index, str, len)
```

**Setters by name:**
```c
cfgpack_set_u8_by_name(ctx, name, val)    cfgpack_set_i8_by_name(ctx, name, val)
cfgpack_set_u16_by_name(ctx, name, val)   cfgpack_set_i16_by_name(ctx, name, val)
cfgpack_set_u32_by_name(ctx, name, val)   cfgpack_set_i32_by_name(ctx, name, val)
cfgpack_set_u64_by_name(ctx, name, val)   cfgpack_set_i64_by_name(ctx, name, val)
cfgpack_set_f32_by_name(ctx, name, val)   cfgpack_set_f64_by_name(ctx, name, val)
cfgpack_set_str_by_name(ctx, name, str, len)
cfgpack_set_fstr_by_name(ctx, name, str, len)
```

**Getters by index:**
```c
cfgpack_get_u8(ctx, index, &out)    cfgpack_get_i8(ctx, index, &out)
cfgpack_get_u16(ctx, index, &out)   cfgpack_get_i16(ctx, index, &out)
cfgpack_get_u32(ctx, index, &out)   cfgpack_get_i32(ctx, index, &out)
cfgpack_get_u64(ctx, index, &out)   cfgpack_get_i64(ctx, index, &out)
cfgpack_get_f32(ctx, index, &out)   cfgpack_get_f64(ctx, index, &out)
cfgpack_get_str(ctx, index, &ptr, &len)   // returns pointer + length
cfgpack_get_fstr(ctx, index, &ptr, &len)  // returns pointer + length
```

**Getters by name:**
```c
cfgpack_get_u8_by_name(ctx, name, &out)    cfgpack_get_i8_by_name(ctx, name, &out)
cfgpack_get_u16_by_name(ctx, name, &out)   cfgpack_get_i16_by_name(ctx, name, &out)
cfgpack_get_u32_by_name(ctx, name, &out)   cfgpack_get_i32_by_name(ctx, name, &out)
cfgpack_get_u64_by_name(ctx, name, &out)   cfgpack_get_i64_by_name(ctx, name, &out)
cfgpack_get_f32_by_name(ctx, name, &out)   cfgpack_get_f64_by_name(ctx, name, &out)
cfgpack_get_str_by_name(ctx, name, &ptr, &len)
cfgpack_get_fstr_by_name(ctx, name, &ptr, &len)
```

Example (static buffers, max 128 entries):
```c
cfgpack_entry_t entries[128];
cfgpack_value_t defaults[128];
cfgpack_schema_t schema;
cfgpack_parse_error_t err;
cfgpack_value_t values[128];
uint8_t present[(128+7)/8];
uint8_t scratch[4096];

cfgpack_parse_schema("my.map", &schema, entries, 128, defaults, &err);
cfgpack_init(&ctx, &schema, values, 128, defaults, present, sizeof(present));
// At this point, entries with defaults are already present and populated

// Using typed convenience functions (recommended):
uint16_t speed;
cfgpack_get_u16_by_name(&ctx, "maxsp", &speed);  // may already have default value
cfgpack_set_u16_by_name(&ctx, "maxsp", 100);

const char *model;
uint8_t model_len;
cfgpack_get_fstr_by_name(&ctx, "model", &model, &model_len);
cfgpack_set_fstr_by_name(&ctx, "model", "MX600", 5);

// Using generic API (for dynamic type handling):
cfgpack_value_t v;
cfgpack_get_by_name(&ctx, "maxsp", &v);
v.v.u64 = 120;
cfgpack_set_by_name(&ctx, "maxsp", &v);

cfgpack_pageout(&ctx, scratch, sizeof(scratch), &len);
cfgpack_pagein_buf(&ctx, scratch, len);
```

### MessagePack helpers (internal-facing)
```c
#include "cfgpack/msgpack.h"

void cfgpack_buf_init(cfgpack_buf_t *buf, uint8_t *storage, size_t cap);
cfgpack_err_t cfgpack_buf_append(cfgpack_buf_t *buf, const void *src, size_t len);

cfgpack_err_t cfgpack_msgpack_encode_uint64(cfgpack_buf_t *buf, uint64_t v);
cfgpack_err_t cfgpack_msgpack_encode_int64(cfgpack_buf_t *buf, int64_t v);
cfgpack_err_t cfgpack_msgpack_encode_f32(cfgpack_buf_t *buf, float v);
cfgpack_err_t cfgpack_msgpack_encode_f64(cfgpack_buf_t *buf, double v);
cfgpack_err_t cfgpack_msgpack_encode_str(cfgpack_buf_t *buf, const char *s, size_t len);
cfgpack_err_t cfgpack_msgpack_encode_map_header(cfgpack_buf_t *buf, uint32_t count);
cfgpack_err_t cfgpack_msgpack_encode_uint_key(cfgpack_buf_t *buf, uint64_t v);
cfgpack_err_t cfgpack_msgpack_encode_str_key(cfgpack_buf_t *buf, const char *s, size_t len);

void cfgpack_reader_init(cfgpack_reader_t *r, const uint8_t *data, size_t len);
cfgpack_err_t cfgpack_msgpack_decode_uint64(cfgpack_reader_t *r, uint64_t *out);
cfgpack_err_t cfgpack_msgpack_decode_int64(cfgpack_reader_t *r, int64_t *out);
cfgpack_err_t cfgpack_msgpack_decode_f32(cfgpack_reader_t *r, float *out);
cfgpack_err_t cfgpack_msgpack_decode_f64(cfgpack_reader_t *r, double *out);
cfgpack_err_t cfgpack_msgpack_decode_str(cfgpack_reader_t *r, const uint8_t **ptr, uint32_t *len);
cfgpack_err_t cfgpack_msgpack_decode_map_header(cfgpack_reader_t *r, uint32_t *count);
```

## Schema Versioning & Remapping

CFGPack supports firmware upgrades where the configuration schema changes between versions. This is handled through:

1. **Schema name embedding**: The schema name is automatically stored at reserved index 0 in serialized blobs
2. **Version detection**: Read the schema name from a blob to determine which schema version created it
3. **Index remapping**: Map old indices to new indices when loading config from an older schema version
4. **Type widening**: Automatically coerce values to wider types (e.g., u8 → u16) during remapping

### Reserved Index

Index 0 (`CFGPACK_INDEX_RESERVED_NAME`) is reserved for the schema name. User-defined schema entries should use indices starting at 1.

### Detecting Schema Version

Use `cfgpack_peek_name()` to read the schema name from a serialized blob without fully loading it:

```c
uint8_t blob[4096];
size_t blob_len;
// ... load blob from storage ...

char name[64];
cfgpack_err_t err = cfgpack_peek_name(blob, blob_len, name, sizeof(name));
if (err == CFGPACK_OK) {
    printf("Config was created with schema: %s\n", name);
} else if (err == CFGPACK_ERR_MISSING) {
    printf("No schema name in blob (legacy format)\n");
}
```

### Migrating Between Schema Versions

When loading config from an older schema version, use `cfgpack_pagein_remap()` with a remap table:

```c
// Old schema "sensor_v1" had:
//   index 1: temp (u8)
//   index 2: humid (u8)
//
// New schema "sensor_v2" has:
//   index 1: temp (u16)   -- widened type, same index
//   index 5: humid (u16)  -- moved to new index, widened type
//   index 6: press (u16)  -- new field

// Define remap table: old_index -> new_index
cfgpack_remap_entry_t remap[] = {
    {1, 1},  // temp: index unchanged (type widening handled automatically)
    {2, 5},  // humid: moved from index 2 to index 5
};

// Load with remapping
cfgpack_err_t err = cfgpack_pagein_remap(&ctx, blob, blob_len, remap, 2);
if (err == CFGPACK_OK) {
    // Old values loaded into new schema positions
    // New fields (like press at index 6) retain their defaults
}
```

### Type Widening Rules

During remapping, values can be automatically widened to larger types:

| From | To (allowed) |
|------|--------------|
| u8   | u16, u32, u64 |
| u16  | u32, u64 |
| u32  | u64 |
| i8   | i16, i32, i64 |
| i16  | i32, i64 |
| i32  | i64 |
| f32  | f64 |
| fstr | str (if length fits) |

Narrowing conversions (e.g., u16 → u8) return `CFGPACK_ERR_TYPE_MISMATCH`.

### Migration Workflow

A typical firmware upgrade migration:

```c
// 1. Read schema name from stored config
char stored_name[64];
cfgpack_err_t err = cfgpack_peek_name(flash_data, flash_len, stored_name, sizeof(stored_name));

// 2. Compare with current schema
if (strcmp(stored_name, current_schema.map_name) == 0) {
    // Same schema version - load directly
    cfgpack_pagein_buf(&ctx, flash_data, flash_len);
} else if (strcmp(stored_name, "myapp_v1") == 0) {
    // Old v1 schema - apply v1->v2 remap
    cfgpack_pagein_remap(&ctx, flash_data, flash_len, v1_to_v2_remap, remap_count);
} else {
    // Unknown schema - use defaults
    printf("Unknown config version, using defaults\n");
}
```

## Examples

Two complete examples are provided in the `examples/` directory:

### datalogger

A basic data logger configuration demonstrating:
- Parsing schema from `.map` file
- Typed convenience functions for get/set
- Generic API for dynamic iteration
- Serialization to MessagePack and JSON export

```bash
cd examples/datalogger
make run
```

### sensor_hub

An IoT sensor hub demonstrating compressed schema loading:
- JSON schema with 66 entries, compressed with heatshrink at build time
- Runtime decompression of schema from external file
- Generic setter API by index (`cfgpack_set(&ctx, index, &val)`)
- Round-trip verification

```bash
cd examples/sensor_hub
make run
```

The sensor_hub example shows a typical embedded workflow where configuration schemas are compressed to save flash space. The heatshrink compression achieves ~22% ratio (4245 → 949 bytes).

## Building
- `make` builds `build/out/libcfgpack.a`.
- `make tests` builds all test binaries.
- `make tools` builds the compression tool (`build/out/cfgpack-compress`).

### Build Modes

CFGPack supports two build modes to balance embedded constraints with desktop development convenience:

| Mode | Default | stdio | Print Functions | Float Formatting |
|------|---------|-------|-----------------|------------------|
| `CFGPACK_EMBEDDED` | Yes | Not linked | Silent no-ops | Minimal (9 digits) |
| `CFGPACK_HOSTED` | No | Linked | Full printf | snprintf (%.17g) |

**Embedded mode** (default):
- No `<stdio.h>` dependency in the core library
- `cfgpack_print()` and `cfgpack_print_all()` return `CFGPACK_OK` without output
- Float-to-string uses a minimal formatter (integer + 9 fractional digits, no scientific notation)
- Ideal for microcontrollers where stdio is unavailable or expensive

**Hosted mode** (`-DCFGPACK_HOSTED`):
- Full `printf` support for debug output
- `cfgpack_print()` and `cfgpack_print_all()` write to stdout
- Float formatting uses `snprintf` with full precision
- Used automatically for tests and tools

To compile your application in hosted mode:
```bash
$(CC) -DCFGPACK_HOSTED -Iinclude myapp.c -Lbuild/out -lcfgpack
```

Note: Applications can still use their own `printf` in embedded mode by including `<stdio.h>` directly in their source files. The library itself simply doesn't depend on stdio.

## Testing

Run `make tests` to build the test binaries, then `./scripts/run-tests.sh` to execute them:

```bash
make tests
./scripts/run-tests.sh
```

The test runner outputs a summary to the console and writes detailed logs to `build/test.log`:

```
Running tests...

  basic:         5/5 passed
  decompress:    8/8 passed
  parser_bounds: 24/24 passed
  parser:        3/3 passed
  runtime:       21/21 passed

TOTAL: 61/61 passed
Full log: build/test.log
```

### Test Binaries
- `build/out/basic` — set/get/print and type/length checks
- `build/out/decompress` — LZ4 and heatshrink decompression roundtrips
- `build/out/parser` — schema parsing against sample data
- `build/out/parser_bounds` — parser edge cases (duplicate names/indices, length bounds, missing header/fields, too many entries, unknown types)
- `build/out/runtime` — runtime bounds, pageout/pagein error paths, and full type roundtrip

## Compression Support

CFGPack supports decompression of stored configs using LZ4 or heatshrink algorithms. This is useful when config blobs are stored compressed in flash to save space. Compression must be done externally (e.g., at build time or on the host); only decompression is supported on the device.

Both LZ4 and heatshrink are enabled by default. To disable for minimal embedded builds, edit the `CFLAGS` in the Makefile to remove `-DCFGPACK_LZ4` and/or `-DCFGPACK_HEATSHRINK`.

### API

```c
#include "cfgpack/decompress.h"

/* Decompress LZ4 data and load into context.
 * decompressed_size must be known (stored alongside compressed data). */
cfgpack_err_t cfgpack_pagein_lz4(cfgpack_ctx_t *ctx, const uint8_t *data, size_t len,
                                  size_t decompressed_size);

/* Decompress heatshrink data and load into context.
 * Encoder must use window=8, lookahead=4 to match decoder config. */
cfgpack_err_t cfgpack_pagein_heatshrink(cfgpack_ctx_t *ctx, const uint8_t *data, size_t len);
```

### Usage Example

```c
// LZ4 example - decompressed size must be stored with the compressed blob
uint8_t compressed[2048];
size_t compressed_len = read_from_flash(compressed);
size_t decompressed_size = read_size_header();  // stored alongside blob

cfgpack_err_t err = cfgpack_pagein_lz4(&ctx, compressed, compressed_len, decompressed_size);
if (err != CFGPACK_OK) {
    // Handle decompression or decode error
}

// Heatshrink example - no size header needed (streaming)
err = cfgpack_pagein_heatshrink(&ctx, compressed, compressed_len);
```

### Implementation Notes

- **Static buffer**: Both decompression functions use a shared internal 4096-byte buffer (matching `PAGE_CAP`). Decompressed data cannot exceed this size.
- **Not thread-safe**: The shared static buffer means these functions cannot be called concurrently from multiple threads.
- **Heatshrink parameters**: The decoder is configured with window=8 bits (256 bytes) and lookahead=4 bits (16 bytes). The encoder must use matching parameters.
- **Vendored sources**: LZ4 and heatshrink source files are vendored in `third_party/` to avoid external dependencies.

### Compression Workflow

A typical workflow for storing compressed config:

1. **At build time or on host**: Serialize config with `cfgpack_pageout()`, compress with LZ4 or heatshrink, store compressed blob + size metadata in flash image
2. **On device boot**: Read compressed blob from flash, decompress with `cfgpack_pagein_lz4()` or `cfgpack_pagein_heatshrink()`

### Compression Tool

The `cfgpack-compress` CLI tool compresses files for build-time or host-side use:

```bash
make tools
./build/out/cfgpack-compress <algorithm> <input> <output>
```

Where `<algorithm>` is either `lz4` or `heatshrink`.

Example:
```bash
# Compress a serialized config blob
./build/out/cfgpack-compress lz4 config.bin config.lz4
./build/out/cfgpack-compress heatshrink config.bin config.hs
```

### Third-Party Libraries

LZ4 and heatshrink sources are vendored in `third_party/` for self-contained builds:

```
third_party/
  lz4/
    lz4.h, lz4.c              # BSD-2-Clause license
  heatshrink/
    heatshrink_config.h       # window=8, lookahead=4
    heatshrink_decoder.h/c    # Used by library
    heatshrink_encoder.h/c    # Used by compression tool and tests
                              # ISC license
```

## Roadmap
- Rust library alongside C implementation.
