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

/* Write schema to JSON buffer */
cfgpack_err_t cfgpack_schema_write_json(const cfgpack_schema_t *schema,
                                        const cfgpack_value_t *values,
                                        char *out, size_t out_cap, size_t *out_len,
                                        cfgpack_parse_error_t *err);
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

## Runtime API

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

## Typed Convenience Functions

For ergonomic access without manually constructing `cfgpack_value_t` structs, use the typed inline functions. All return `cfgpack_err_t` and validate type matches at runtime.

### Setters by index

```c
cfgpack_set_u8(ctx, index, val)    cfgpack_set_i8(ctx, index, val)
cfgpack_set_u16(ctx, index, val)   cfgpack_set_i16(ctx, index, val)
cfgpack_set_u32(ctx, index, val)   cfgpack_set_i32(ctx, index, val)
cfgpack_set_u64(ctx, index, val)   cfgpack_set_i64(ctx, index, val)
cfgpack_set_f32(ctx, index, val)   cfgpack_set_f64(ctx, index, val)
cfgpack_set_str(ctx, index, str, len)
cfgpack_set_fstr(ctx, index, str, len)
```

### Setters by name

```c
cfgpack_set_u8_by_name(ctx, name, val)    cfgpack_set_i8_by_name(ctx, name, val)
cfgpack_set_u16_by_name(ctx, name, val)   cfgpack_set_i16_by_name(ctx, name, val)
cfgpack_set_u32_by_name(ctx, name, val)   cfgpack_set_i32_by_name(ctx, name, val)
cfgpack_set_u64_by_name(ctx, name, val)   cfgpack_set_i64_by_name(ctx, name, val)
cfgpack_set_f32_by_name(ctx, name, val)   cfgpack_set_f64_by_name(ctx, name, val)
cfgpack_set_str_by_name(ctx, name, str, len)
cfgpack_set_fstr_by_name(ctx, name, str, len)
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
```
