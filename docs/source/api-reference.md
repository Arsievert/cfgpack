# CFGPack API Reference

This document covers the complete public API surface. Include just `cfgpack/cfgpack.h`; it re-exports all public headers.

## Errors

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

## Values

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

/* Compact runtime value (strings use offset + length into external pool) */
typedef struct {
    cfgpack_type_t type;
    union {
        uint64_t u64;
        int64_t i64;
        float   f32;
        double  f64;
        struct { uint16_t offset; uint16_t len; } str;
        struct { uint16_t offset; uint8_t len; uint8_t _pad; } fstr;
    } v;
} cfgpack_value_t;
```

String data is stored in a caller-owned pool buffer; the `str` and `fstr` union members contain an offset into that pool and the current length.

## Schema

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

typedef struct {
    char message[128];
    size_t line;
} cfgpack_parse_error_t;
```

### Schema Sizing

Use `cfgpack_schema_get_sizing()` after parsing to determine how much memory to allocate for the string pool and offsets array:

```c
typedef struct {
    size_t str_pool_size; /* Total bytes needed for string pool */
    size_t str_count;     /* Number of str-type entries */
    size_t fstr_count;    /* Number of fstr-type entries */
} cfgpack_schema_sizing_t;

cfgpack_err_t cfgpack_schema_get_sizing(const cfgpack_schema_t *schema,
                                        cfgpack_schema_sizing_t *out);
```

### Parse Options

All parse functions accept a `cfgpack_parse_opts_t` struct that bundles the output schema, entry/value arrays, string pool, and error output:

```c
typedef struct {
    cfgpack_schema_t *out_schema;       /* Output schema to populate */
    cfgpack_entry_t *entries;           /* Caller-provided entry array */
    size_t max_entries;                 /* Capacity of entries array */
    cfgpack_value_t *values;           /* Caller-provided values array */
    char *str_pool;                     /* Caller-provided string pool */
    size_t str_pool_cap;               /* Capacity of string pool in bytes */
    uint16_t *str_offsets;             /* Caller-provided string offset array */
    size_t str_offsets_count;          /* Number of string offset slots */
    cfgpack_parse_error_t *err;        /* Output parse error details */
} cfgpack_parse_opts_t;
```

### Schema Measurement (Pre-Parse)

Use the measure functions to determine buffer sizes **before** parsing, without allocating any output buffers. This is the recommended approach when schema size is not known at compile time (e.g., loading from external storage). The measure pass requires only ~32 bytes of stack vs ~8KB for a full discovery parse into oversized buffers.

```c
typedef struct {
    size_t entry_count;   /* Number of entries in the schema */
    size_t str_pool_size; /* Total bytes needed for string pool */
    size_t str_count;     /* Number of str-type entries */
    size_t fstr_count;    /* Number of fstr-type entries */
} cfgpack_schema_measure_t;

/* Measure a .map schema buffer */
cfgpack_err_t cfgpack_schema_measure(const char *data, size_t data_len,
                                     cfgpack_schema_measure_t *out,
                                     cfgpack_parse_error_t *err);

/* Measure a JSON schema buffer */
cfgpack_err_t cfgpack_schema_measure_json(const char *data, size_t data_len,
                                          cfgpack_schema_measure_t *out,
                                          cfgpack_parse_error_t *err);

/* Measure a MessagePack binary schema buffer */
cfgpack_err_t cfgpack_schema_measure_msgpack(const uint8_t *data, size_t data_len,
                                             cfgpack_schema_measure_t *out,
                                             cfgpack_parse_error_t *err);
```

All three functions validate structure, types, reserved index 0, name length, and default values. Duplicate checking (which requires O(n) storage) is deferred to the subsequent parse call.

After measuring, allocate exact-sized buffers and parse:

```c
cfgpack_schema_measure_t m;
cfgpack_schema_measure(data, len, &m, &err);

cfgpack_entry_t *entries   = malloc(m.entry_count * sizeof(cfgpack_entry_t));
cfgpack_value_t *values    = malloc(m.entry_count * sizeof(cfgpack_value_t));
char            *str_pool  = malloc(m.str_pool_size);
uint16_t        *str_off   = malloc((m.str_count + m.fstr_count) * sizeof(uint16_t));

cfgpack_parse_opts_t opts = {
    .out_schema       = &schema,
    .entries          = entries,
    .max_entries      = m.entry_count,
    .values           = values,
    .str_pool         = str_pool,
    .str_pool_cap     = m.str_pool_size,
    .str_offsets      = str_off,
    .str_offsets_count = m.str_count + m.fstr_count,
    .err              = &err,
};
cfgpack_parse_schema(data, len, &opts);
```

### Parsing and Serialization

Default values are written directly into the caller-provided `values` array and `str_pool` during parsing. There is no separate defaults storage.

```c
/* Parse schema from .map buffer */
cfgpack_err_t cfgpack_parse_schema(const char *data, size_t data_len,
                                   const cfgpack_parse_opts_t *opts);

/* Parse schema from JSON buffer */
cfgpack_err_t cfgpack_schema_parse_json(const char *data, size_t data_len,
                                        const cfgpack_parse_opts_t *opts);

/* Parse schema from MessagePack binary buffer */
cfgpack_err_t cfgpack_schema_parse_msgpack(const uint8_t *data, size_t data_len,
                                           const cfgpack_parse_opts_t *opts);

/* Write schema and current values to JSON buffer */
cfgpack_err_t cfgpack_schema_write_json(const cfgpack_ctx_t *ctx,
                                        char *out, size_t out_cap, size_t *out_len,
                                        cfgpack_parse_error_t *err);

/* Write schema and current values to MessagePack binary buffer */
cfgpack_err_t cfgpack_schema_write_msgpack(const cfgpack_ctx_t *ctx,
                                           uint8_t *out, size_t out_cap, size_t *out_len,
                                           cfgpack_parse_error_t *err);

void cfgpack_schema_free(cfgpack_schema_t *schema); /* no-op for caller-owned arrays */
```

### JSON Schema Format

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

### MessagePack Binary Schema Format

Schemas can also be stored and transmitted as compact MessagePack binary, which is typically 50-60% smaller than equivalent JSON and requires no tokenizer or string-to-number conversion on the device. The wire format uses integer keys and integer type codes for maximum compactness:

```
map(3) {
  0 : str  "demo"          // name
  1 : uint 1               // version
  2 : array(N) [           // entries
    map(4) {
      0 : uint 1           // index
      1 : str  "speed"     // name
      2 : uint 1           // type (cfgpack_type_t enum: U16=1)
      3 : uint 100         // value
    }
    map(4) {
      0 : uint 2           // index
      1 : str  "label"     // name
      2 : uint 11          // type (FSTR=11)
      3 : str  "hello"     // value
    }
    map(4) {
      0 : uint 3           // index
      1 : str  "desc"      // name
      2 : uint 10          // type (STR=10)
      3 : nil              // value (no default)
    }
  ]
}
```

**Top-level keys:** 0=name, 1=version, 2=entries
**Per-entry keys:** 0=index, 1=name, 2=type, 3=value
**Type encoding:** `cfgpack_type_t` enum integer (U8=0, U16=1, U32=2, U64=3, I8=4, I16=5, I32=6, I64=7, F32=8, F64=9, STR=10, FSTR=11)

- `value` is `nil` (0xC0) for entries with no default
- Integer defaults use msgpack uint/int encoding
- Float defaults use msgpack float32 (0xCA) or float64 (0xCB) encoding
- String defaults use msgpack str encoding

Use the `cfgpack-schema-pack` CLI tool to convert `.map` or JSON schemas to msgpack binary:

```bash
./build/out/cfgpack-schema-pack input.json output.msgpack
./build/out/cfgpack-schema-pack input.map output.msgpack
```

## Runtime API

```c
#include "cfgpack/api.h"

/* Initialize runtime context. The values and string pool should already
 * contain defaults from schema parsing. Presence bitmap is embedded in
 * cfgpack_ctx_t (sized by CFGPACK_MAX_ENTRIES, default 128). */
cfgpack_err_t cfgpack_init(cfgpack_ctx_t *ctx, const cfgpack_schema_t *schema,
                           cfgpack_value_t *values, size_t values_count,
                           char *str_pool, size_t str_pool_cap,
                           uint16_t *str_offsets, size_t str_offsets_count);
void          cfgpack_free(cfgpack_ctx_t *ctx);

cfgpack_err_t cfgpack_set(cfgpack_ctx_t *ctx, uint16_t index, const cfgpack_value_t *value);
cfgpack_err_t cfgpack_get(const cfgpack_ctx_t *ctx, uint16_t index, cfgpack_value_t *out_value);
cfgpack_err_t cfgpack_set_by_name(cfgpack_ctx_t *ctx, const char *name, const cfgpack_value_t *value);
cfgpack_err_t cfgpack_get_by_name(const cfgpack_ctx_t *ctx, const char *name, cfgpack_value_t *out_value);

cfgpack_err_t cfgpack_pageout(const cfgpack_ctx_t *ctx, uint8_t *out, size_t out_cap, size_t *out_len);
cfgpack_err_t cfgpack_pagein_buf(cfgpack_ctx_t *ctx, const uint8_t *data, size_t len);

/* Schema versioning and remapping */
cfgpack_err_t cfgpack_peek_name(const uint8_t *data, size_t len, char *out_name, size_t out_cap);
cfgpack_err_t cfgpack_pagein_remap(cfgpack_ctx_t *ctx, const uint8_t *data, size_t len,
                                    const cfgpack_remap_entry_t *remap, size_t remap_count);
```

After decoding all entries from the old data, `cfgpack_pagein_remap()` restores presence for any new-schema entries that have `has_default` set but were not in the incoming payload. This ensures new entries with defaults are immediately accessible after migration without explicit code to set them. Entries without defaults that were not in the old data remain absent.

## Typed Convenience Functions

For ergonomic access without manually constructing `cfgpack_value_t` structs, use the typed inline functions. All return `cfgpack_err_t` and validate type matches at runtime.

### Setters by index

```c
cfgpack_set_u8(ctx, index, val)    cfgpack_set_i8(ctx, index, val)
cfgpack_set_u16(ctx, index, val)   cfgpack_set_i16(ctx, index, val)
cfgpack_set_u32(ctx, index, val)   cfgpack_set_i32(ctx, index, val)
cfgpack_set_u64(ctx, index, val)   cfgpack_set_i64(ctx, index, val)
cfgpack_set_f32(ctx, index, val)   cfgpack_set_f64(ctx, index, val)
cfgpack_set_str(ctx, index, str)
cfgpack_set_fstr(ctx, index, str)
```

### Setters by name

```c
cfgpack_set_u8_by_name(ctx, name, val)    cfgpack_set_i8_by_name(ctx, name, val)
cfgpack_set_u16_by_name(ctx, name, val)   cfgpack_set_i16_by_name(ctx, name, val)
cfgpack_set_u32_by_name(ctx, name, val)   cfgpack_set_i32_by_name(ctx, name, val)
cfgpack_set_u64_by_name(ctx, name, val)   cfgpack_set_i64_by_name(ctx, name, val)
cfgpack_set_f32_by_name(ctx, name, val)   cfgpack_set_f64_by_name(ctx, name, val)
cfgpack_set_str_by_name(ctx, name, str)
cfgpack_set_fstr_by_name(ctx, name, str)
```

### Getters by index

```c
cfgpack_get_u8(ctx, index, &out)    cfgpack_get_i8(ctx, index, &out)
cfgpack_get_u16(ctx, index, &out)   cfgpack_get_i16(ctx, index, &out)
cfgpack_get_u32(ctx, index, &out)   cfgpack_get_i32(ctx, index, &out)
cfgpack_get_u64(ctx, index, &out)   cfgpack_get_i64(ctx, index, &out)
cfgpack_get_f32(ctx, index, &out)   cfgpack_get_f64(ctx, index, &out)
cfgpack_get_str(ctx, index, &ptr, &len)   // returns pointer + length
cfgpack_get_fstr(ctx, index, &ptr, &len)  // returns pointer + length
```

### Getters by name

```c
cfgpack_get_u8_by_name(ctx, name, &out)    cfgpack_get_i8_by_name(ctx, name, &out)
cfgpack_get_u16_by_name(ctx, name, &out)   cfgpack_get_i16_by_name(ctx, name, &out)
cfgpack_get_u32_by_name(ctx, name, &out)   cfgpack_get_i32_by_name(ctx, name, &out)
cfgpack_get_u64_by_name(ctx, name, &out)   cfgpack_get_i64_by_name(ctx, name, &out)
cfgpack_get_f32_by_name(ctx, name, &out)   cfgpack_get_f64_by_name(ctx, name, &out)
cfgpack_get_str_by_name(ctx, name, &ptr, &len)
cfgpack_get_fstr_by_name(ctx, name, &ptr, &len)
```

## Usage Example

```c
cfgpack_entry_t entries[128];
cfgpack_schema_t schema;
cfgpack_parse_error_t err;
cfgpack_value_t values[128];
char str_pool[256];
uint16_t str_offsets[128];
uint8_t scratch[4096];

/* Parse schema — defaults are written directly into values[] and str_pool[] */
cfgpack_parse_opts_t opts = {
    .out_schema       = &schema,
    .entries          = entries,
    .max_entries      = 128,
    .values           = values,
    .str_pool         = str_pool,
    .str_pool_cap     = sizeof(str_pool),
    .str_offsets      = str_offsets,
    .str_offsets_count = 128,
    .err              = &err,
};
cfgpack_parse_schema(map_data, map_len, &opts);

/* Initialize context — values and str_pool already contain defaults from parsing */
cfgpack_ctx_t ctx;
cfgpack_init(&ctx, &schema, values, 128,
             str_pool, sizeof(str_pool), str_offsets, 128);
// At this point, entries with defaults are already present and populated
// Presence bitmap is embedded in ctx (no separate allocation needed)

// Using typed convenience functions (recommended):
uint16_t speed;
cfgpack_get_u16_by_name(&ctx, "maxsp", &speed);  // may already have default value
cfgpack_set_u16_by_name(&ctx, "maxsp", 100);

const char *model;
uint8_t model_len;
cfgpack_get_fstr_by_name(&ctx, "model", &model, &model_len);
cfgpack_set_fstr_by_name(&ctx, "model", "MX600");

// Using generic API (for dynamic type handling):
cfgpack_value_t v;
cfgpack_get_by_name(&ctx, "maxsp", &v);
v.v.u64 = 120;
cfgpack_set_by_name(&ctx, "maxsp", &v);

size_t len;
cfgpack_pageout(&ctx, scratch, sizeof(scratch), &len);
cfgpack_pagein_buf(&ctx, scratch, len);
```

## Dynamic Allocation Example

When schema size is not known at compile time, use `cfgpack_schema_measure()` to learn buffer sizes before allocating. This is the pattern used in the `examples/allocate-once/` example.

```c
/* 1. Measure — only 32 bytes of stack */
cfgpack_schema_measure_t m;
cfgpack_parse_error_t err;
cfgpack_schema_measure(map_data, map_len, &m, &err);

/* 2. Allocate right-sized buffers */
cfgpack_entry_t *entries  = malloc(m.entry_count * sizeof(cfgpack_entry_t));
cfgpack_value_t *values   = malloc(m.entry_count * sizeof(cfgpack_value_t));
char            *str_pool = malloc(m.str_pool_size);
uint16_t        *str_off  = malloc((m.str_count + m.fstr_count) * sizeof(uint16_t));

/* 3. Parse into exact-sized buffers */
cfgpack_schema_t schema;
cfgpack_parse_opts_t opts = {
    .out_schema        = &schema,
    .entries           = entries,
    .max_entries       = m.entry_count,
    .values            = values,
    .str_pool          = str_pool,
    .str_pool_cap      = m.str_pool_size,
    .str_offsets       = str_off,
    .str_offsets_count = m.str_count + m.fstr_count,
    .err               = &err,
};
cfgpack_parse_schema(map_data, map_len, &opts);

/* 4. Initialize context — identical to the static example from here on */
cfgpack_ctx_t ctx;
cfgpack_init(&ctx, &schema, values, m.entry_count,
             str_pool, m.str_pool_size, str_off, m.str_count + m.fstr_count);
```

The same pattern works with `cfgpack_schema_measure_json()` / `cfgpack_schema_parse_json()` for JSON schemas and `cfgpack_schema_measure_msgpack()` / `cfgpack_schema_parse_msgpack()` for MessagePack binary schemas.

## File I/O Wrappers (Optional)

These functions use `FILE*` operations and are provided for convenience on desktop/POSIX systems. For embedded systems without file I/O, use the buffer-based functions in `api.h` and `schema.h` instead. To use these, compile and link `src/io_file.c` with your project.

```c
#include "cfgpack/io_file.h"

/* Measure a .map schema from a file */
cfgpack_err_t cfgpack_schema_measure_file(const char *path,
                                          cfgpack_schema_measure_t *out,
                                          char *scratch, size_t scratch_cap,
                                          cfgpack_parse_error_t *err);

/* Measure a JSON schema from a file */
cfgpack_err_t cfgpack_schema_measure_json_file(const char *path,
                                               cfgpack_schema_measure_t *out,
                                               char *scratch, size_t scratch_cap,
                                               cfgpack_parse_error_t *err);

/* Parse a .map schema from a file */
cfgpack_err_t cfgpack_parse_schema_file(const char *path,
                                        const cfgpack_parse_opts_t *opts,
                                        char *scratch, size_t scratch_cap);

/* Parse a JSON schema from a file */
cfgpack_err_t cfgpack_schema_parse_json_file(const char *path,
                                             const cfgpack_parse_opts_t *opts,
                                             char *scratch, size_t scratch_cap);

/* Write schema and values as JSON to a file */
cfgpack_err_t cfgpack_schema_write_json_file(const cfgpack_ctx_t *ctx, const char *path,
                                             char *scratch, size_t scratch_cap,
                                             cfgpack_parse_error_t *err);

/* Encode to a file using caller scratch buffer */
cfgpack_err_t cfgpack_pageout_file(const cfgpack_ctx_t *ctx, const char *path,
                                   uint8_t *scratch, size_t scratch_cap);

/* Decode from a file using caller scratch buffer */
cfgpack_err_t cfgpack_pagein_file(cfgpack_ctx_t *ctx, const char *path,
                                  uint8_t *scratch, size_t scratch_cap);
```

## MessagePack Helpers (Internal-Facing)

These are lower-level functions used internally. They're exposed for advanced use cases.

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
cfgpack_err_t cfgpack_msgpack_skip_value(cfgpack_reader_t *r);
```
