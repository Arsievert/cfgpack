# Stack Usage Analysis

This document provides per-function stack frame sizes for every cfgpack library
function, measured with `clang -fstack-usage` on an arm64 target. Use these
numbers to budget stack space when deploying cfgpack on embedded systems.

## Measurement Method

Stack frame sizes were obtained by compiling with:

```
clang -fstack-usage -std=c99 -Wall -Wextra [-O0 | -Os]
```

The `-fstack-usage` flag emits `.su` files alongside each `.o` with the exact
stack frame size for every function. Two optimization levels are shown:

- **-O0**: Worst case (no inlining, no register reuse). Useful for debug builds.
- **-Os**: Optimized for size. Representative of production embedded builds.

Run `make stack-usage-O0` or `make stack-usage-Os` to reproduce these measurements.

## Per-Function Stack Frame Sizes

### core.c — Runtime get/set operations

| Function | -O0 | -Os | Notes |
|---|---:|---:|---|
| `cfgpack_init` | 128 | 0 | Inlined at -Os |
| `cfgpack_free` | 16 | 0 | Trivial cleanup |
| `cfgpack_set` | 64 | 0 | Set value by index |
| `cfgpack_get` | 64 | 0 | Get value by index |
| `cfgpack_set_by_name` | 64 | 80 | Set by name (calls find_entry_by_name) |
| `cfgpack_get_by_name` | 64 | 80 | Get by name |
| `cfgpack_set_str` | 96 | 80 | Set string value |
| `cfgpack_get_str` | 80 | 0 | Get string value |
| `cfgpack_set_fstr` | 96 | 80 | Set fixed-string value |
| `cfgpack_get_fstr` | 80 | 0 | Get fixed-string value |
| `cfgpack_set_str_by_name` | 64 | 64 | |
| `cfgpack_get_str_by_name` | 64 | 80 | |
| `cfgpack_set_fstr_by_name` | 64 | 64 | |
| `cfgpack_get_fstr_by_name` | 64 | 80 | |
| `cfgpack_get_version` | 16 | 0 | |
| `cfgpack_get_size` | 48 | 0 | |
| `cfgpack_print` | 16 | 0 | No-op in embedded mode |
| `cfgpack_print_all` | 16 | 0 | No-op in embedded mode |

### io.c — MessagePack serialization/deserialization

| Function | -O0 | -Os | Notes |
|---|---:|---:|---|
| `cfgpack_pageout` | 128 | 96 | Serialize context to buffer |
| `cfgpack_peek_name` | 128 | 112 | Read schema name from buffer |
| `cfgpack_pagein_buf` | 48 | 0 | Deserialize from buffer |
| `cfgpack_pagein_remap` | 160 | 160 | Deserialize with index remapping |
| `decode_value` | 144 | 64 | Internal |
| `decode_value_with_coercion` | 144 | — | Inlined at -Os |
| `encode_value` | 80 | — | Inlined at -Os |

### msgpack.c — Low-level MessagePack primitives

| Function | -O0 | -Os | Notes |
|---|---:|---:|---|
| `cfgpack_msgpack_skip_value` | 240 | 160 | **Iterative** — bounded at all depths |
| `cfgpack_msgpack_encode_uint64` | 80 | 48 | |
| `cfgpack_msgpack_encode_int64` | 80 | 48 | |
| `cfgpack_msgpack_encode_f32` | 48 | 0 | |
| `cfgpack_msgpack_encode_f64` | 80 | 0 | |
| `cfgpack_msgpack_encode_str` | 64 | 64 | |
| `cfgpack_msgpack_encode_map_header` | 48 | 32 | |
| `cfgpack_msgpack_decode_uint64` | 80 | 32 | |
| `cfgpack_msgpack_decode_int64` | 96 | 32 | |
| `cfgpack_msgpack_decode_f32` | 64 | 0 | |
| `cfgpack_msgpack_decode_f64` | 80 | 32 | |
| `cfgpack_msgpack_decode_str` | 64 | 0 | |
| `cfgpack_msgpack_decode_map_header` | 48 | 0 | |

### schema_parser.c — Schema parsing (setup-time only)

| Function | -O0 | -Os | Notes |
|---|---:|---:|---|
| `cfgpack_parse_schema` | 1408 | 880 | Largest frame — two 256 B buffers |
| `cfgpack_schema_parse_json` | 1104 | 576 | JSON schema parser |
| `cfgpack_schema_measure` | 896 | 816 | Pre-parse measurement |
| `cfgpack_schema_measure_json` | 544 | 368 | JSON pre-parse measurement |
| `cfgpack_schema_write_json` | 144 | 160 | JSON schema writer |
| `cfgpack_schema_measure_msgpack` | 288 | 240 | Msgpack pre-parse measurement |
| `cfgpack_schema_parse_msgpack` | 736 | 496 | Msgpack schema parser |
| `cfgpack_schema_write_msgpack` | 192 | 128 | Msgpack schema writer |
| `cfgpack_schema_get_sizing` | 64 | 0 | |
| `cfgpack_schema_free` | 16 | 0 | |

### decompress.c — Decompression wrappers

| Function | -O0 | -Os | Notes |
|---|---:|---:|---|
| `cfgpack_pagein_lz4` | 80 | 48 | LZ4 decompress + pagein |
| `cfgpack_pagein_heatshrink` | 112 | 96 | Heatshrink decompress + pagein |

### tokens.c — String tokenizer (internal)

| Function | -O0 | -Os | Notes |
|---|---:|---:|---|
| `tokens_create` | 48 | 32 | |
| `tokens_find` | 96 | 96 | |
| `tokens_destroy` | 32 | 32 | |

## Worst-Case Call Chain Depths

These are the maximum total stack usage for public API entry points,
computed by summing frame sizes along the deepest call chain.

### Runtime operations (called frequently)

| API Call | -O0 worst case | -Os worst case |
|---|---:|---:|
| `cfgpack_set` / `cfgpack_get` | ~128 B | ~80 B |
| `cfgpack_set_by_name` / `cfgpack_get_by_name` | ~176 B | ~128 B |
| `cfgpack_set_str` / `cfgpack_set_fstr` | ~160 B | ~80 B |
| `cfgpack_pageout` | ~336 B | ~208 B |
| `cfgpack_pagein_buf` | ~432 B | ~320 B |
| `cfgpack_pagein_remap` | ~544 B | ~480 B |
| `cfgpack_pagein_lz4` | ~512 B | ~368 B |
| `cfgpack_pagein_heatshrink` | ~544 B | ~416 B |

### Setup operations (called once at startup)

| API Call | -O0 worst case | -Os worst case |
|---|---:|---:|
| `cfgpack_init` | ~128 B | ~0 B |
| `cfgpack_parse_schema` | ~1,408 B | ~880 B |
| `cfgpack_schema_parse_json` | ~1,104 B | ~576 B |
| `cfgpack_schema_parse_msgpack` | ~736 B | ~496 B |
| `cfgpack_schema_measure` | ~896 B | ~816 B |
| `cfgpack_schema_measure_msgpack` | ~288 B | ~240 B |

## Configuration Knobs Affecting Stack Usage

### `CFGPACK_MAX_ENTRIES` (default: 128)

Defined in `include/cfgpack/config.h`. Controls the size of the inline
presence bitmap in `cfgpack_ctx_t`:

```
CFGPACK_PRESENCE_BYTES = ceil(CFGPACK_MAX_ENTRIES / 8)
```

At default 128 entries: 16 bytes in the context struct. This does not
directly affect stack usage (the bitmap is in the context, not on the stack),
but reducing it shrinks the context structure.

### `CFGPACK_SKIP_MAX_DEPTH` (default: 32)

Defined in `include/cfgpack/config.h`. Controls the maximum nesting depth
that `cfgpack_msgpack_skip_value()` can handle. Each level costs 4 bytes
(one `uint32_t` counter), so:

| CFGPACK_SKIP_MAX_DEPTH | Stack cost |
|---:|---:|
| 8 | 32 B |
| 16 | 64 B |
| 32 (default) | 128 B |
| 64 | 256 B |

For typical configuration data with 1-2 levels of nesting, a depth of 8
is sufficient. Reduce this on very constrained targets.

Override at compile time:

```
-DCFGPACK_SKIP_MAX_DEPTH=8
```

### `MAX_LINE_LEN` (default: 256, internal to schema_parser.c)

Controls the maximum line length for `.map` schema files. Two buffers of
this size exist on the stack in `cfgpack_parse_schema()` and
`cfgpack_schema_measure()`. Reducing to 128 would save ~256 B per function,
but this is setup-time code and not called at runtime.

## Recommendations for Embedded Targets

1. **Runtime stack budget**: With `-Os`, all runtime operations (get/set,
   pageout, pagein) stay under **~500 B** of stack. Budget 512 B for the
   cfgpack runtime call chain.

2. **Setup stack budget**: Schema parsing needs up to **~880 B** at `-Os`.
   If parsing schemas on the device, budget 1 KB. If schemas are parsed
   on a host and only binary config is loaded, the parser is not linked.

3. **Always compile with `-Os` or `-O2`**: Stack usage drops significantly
   versus `-O0` due to inlining and register allocation. Many small
   functions (getters, setters, init) collapse to 0 B frames at `-Os`.

4. **Reduce `CFGPACK_SKIP_MAX_DEPTH`** on very constrained targets. A
   value of 8 saves 96 B versus the default 32 and is sufficient for
   typical 1-2 level config maps.

5. **Use `make stack-usage-Os`** to verify stack sizes any time the code
   changes or compiler flags are adjusted.
