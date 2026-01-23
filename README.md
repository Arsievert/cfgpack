# CFGPack

This repository is the C-first implementation of a MessagePack-based configuration library. The map/spec idea lives in `cfgpack.org`; this README summarizes the code layout and key APIs.

**Embedded profile (current):** no heap allocation. All buffers are caller-owned. Hard caps: max 128 schema entries; max pagein/pageout size 4096 bytes. Schema descriptions are ignored/dropped.

## What It Does
- Defines a fixed-cap schema (up to 128 entries) with typed values (u/i 8–64, f32/f64, str/fstr) and 5-char names.
- Parses `.map` specs into caller-owned schema + entries; no heap allocations.
- Initializes runtime with caller-provided value and presence buffers; tracks presence bits and supports set/get/print/size/version.
- Encodes/decodes MessagePack maps; pageout to buffer or file, pagein from buffer or file, with size caps.
- Returns explicit errors for parse/encode/decode/type/bounds/IO issues.

## Map Format
- First line: map name/version header saved with the config.
- Lines follow the format (verbatim): `[INDEX] [NAME] [TYPE] (Description)`
  - `INDEX`: 0–65535
  - `NAME`: up to 5 characters
  - `TYPE`: one of the supported numeric/float/string types (str max 64, fstr max 16)
  - `Description`: optional, not stored in the binary
- Hard caps in this profile: 128 entries, 4096-byte page size.

## Layout
- `include/cfgpack/` — public headers
  - `error.h` — error codes enum.
  - `value.h` — value types and limits (`CFGPACK_STR_MAX`, `CFGPACK_FSTR_MAX`).
  - `schema.h` — schema structs and parser/doc APIs.
  - `msgpack.h` — minimal MessagePack buffer + encode/decode helpers (fixed-capacity, caller storage).
  - `api.h` — main cfgpack runtime API (set/get/pagein/pageout/print/version/size) using caller buffers.
- `src/` — library implementation (`core.c`, `io.c`, `msgpack.c`, `schema_parser.c`).
- `tests/` — C test programs (`basic.c`, `parser.c`, `parser_bounds.c`, `runtime.c`) plus a shared harness in `tests/test.c` and sample data under `tests/data/`.
- `Makefile` — builds `build/out/libcfgpack.a` and test binaries (`build/out/basic`, `build/out/parser`, `build/out/parser_bounds`, `build/out/runtime`).

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
    CFGPACK_ERR_DECODE = -10
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
    uint16_t index;      /* 0-65535 */
    char     name[6];    /* 5 chars + NUL */
    cfgpack_type_t type; /* one of the supported types */
} cfgpack_entry_t;

typedef struct {
    char map_name[64];
    uint32_t version;
    cfgpack_entry_t *entries;
    size_t entry_count;
} cfgpack_schema_t;

cfgpack_err_t cfgpack_parse_schema(const char *path, cfgpack_schema_t *out, cfgpack_entry_t *entries, size_t max_entries, cfgpack_parse_error_t *err);
void cfgpack_schema_free(cfgpack_schema_t *schema); /* no-op for caller-owned arrays */
cfgpack_err_t cfgpack_schema_write_markdown(const cfgpack_schema_t *schema, const char *out_path, cfgpack_parse_error_t *err);
```

### Runtime API
```c
#include "cfgpack/api.h"

cfgpack_err_t cfgpack_init(cfgpack_ctx_t *ctx, const cfgpack_schema_t *schema,
                           cfgpack_value_t *values, size_t values_count,
                           uint8_t *present, size_t present_bytes);
void          cfgpack_free(cfgpack_ctx_t *ctx);

cfgpack_err_t cfgpack_set(cfgpack_ctx_t *ctx, uint16_t index, const cfgpack_value_t *value);
cfgpack_err_t cfgpack_get(const cfgpack_ctx_t *ctx, uint16_t index, cfgpack_value_t *out_value);

cfgpack_err_t cfgpack_pageout(const cfgpack_ctx_t *ctx, uint8_t *out, size_t out_cap, size_t *out_len);
cfgpack_err_t cfgpack_pageout_file(const cfgpack_ctx_t *ctx, const char *path, uint8_t *scratch, size_t scratch_cap);
cfgpack_err_t cfgpack_pagein_buf(cfgpack_ctx_t *ctx, const uint8_t *data, size_t len);
cfgpack_err_t cfgpack_pagein_file(cfgpack_ctx_t *ctx, const char *path, uint8_t *scratch, size_t scratch_cap);

cfgpack_err_t cfgpack_print(const cfgpack_ctx_t *ctx, uint16_t index);
cfgpack_err_t cfgpack_print_all(const cfgpack_ctx_t *ctx);

uint32_t cfgpack_get_version(const cfgpack_ctx_t *ctx);
size_t   cfgpack_get_size(const cfgpack_ctx_t *ctx);
```

Example (static buffers, max 128 entries):
```c
cfgpack_entry_t entries[128];
cfgpack_schema_t schema;
cfgpack_parse_error_t err;
cfgpack_value_t values[128];
uint8_t present[(128+7)/8];
uint8_t scratch[4096];

cfgpack_parse_schema("my.map", &schema, entries, 128, &err);
cfgpack_init(&ctx, &schema, values, 128, present, sizeof(present));
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

## Building
- `make` builds `build/out/libcfgpack.a`.
- `make tests` builds all test binaries (`build/out/basic`, `build/out/parser`, `build/out/parser_bounds`, `build/out/runtime`).

## Testing
- `build/out/basic` exercises set/get/print and type/length checks.
- `build/out/parser` validates schema parsing against sample data.
- `build/out/parser_bounds` covers parser edge cases (duplicate names/indices, length bounds, missing header/fields, too many entries, unknown types).
- `build/out/runtime` covers runtime bounds, pageout/pagein error paths, and full type roundtrip (writes `build/runtime_all.bin` and `build/runtime_tmp.bin`).

## Roadmap
- Rust library alongside C implementation.
- Compression for stored configs.
- Remapping between map versions to smooth schema changes.
- Generated documentation (e.g., PDF/markdown) from map specs.
- Potential modes for loading via temp file vs. in-memory live config.
