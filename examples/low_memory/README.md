# Low-Memory HVAC Example

Demonstrates CFGPack with the **measure API** for right-sized allocation and a
full **v1 -> v2 schema migration** covering all five migration scenarios.

- **v1**: 84 entries (3 fstr, 0 str)
- **v2**: 94 entries (4 fstr, 7 str) -- 10 new entries including zone names,
  firmware version, NTP host, contact email, and per-zone deadband

## Quick Start

```sh
make run
```

This builds the library with default `CFGPACK_MAX_ENTRIES=128`, compiles the
example, and runs it. The output walks through all seven phases (measure, load,
modify, serialize, detect, migrate, verify).

## What This Example Shows

### Measure API: Two-Phase Init

The key pattern demonstrated here is **measure-then-allocate**:

```c
cfgpack_schema_measure_t m;
cfgpack_schema_measure(map_data, map_len, &m, &err);

// m.entry_count   = 94
// m.str_count     = 7
// m.fstr_count    = 4
// m.str_pool_size = 523  (7*65 + 4*17)

entries     = malloc(m.entry_count * sizeof(cfgpack_entry_t));
values      = malloc(m.entry_count * sizeof(cfgpack_value_t));
str_pool    = malloc(m.str_pool_size);
str_offsets = malloc((m.str_count + m.fstr_count) * sizeof(uint16_t));

cfgpack_parse_opts_t opts = {
    .out_schema        = &schema,
    .entries           = entries,
    .max_entries       = m.entry_count,
    .values            = values,
    .str_pool          = str_pool,
    .str_pool_cap      = m.str_pool_size,
    .str_offsets       = str_offsets,
    .str_offsets_count = m.str_count + m.fstr_count,
    .err               = &err,
};
cfgpack_parse_schema(map_data, map_len, &opts);

cfgpack_init(&ctx, &schema, values, m.entry_count,
             str_pool, m.str_pool_size,
             str_offsets, m.str_count + m.fstr_count);
```

This eliminates all compile-time guessing. When the schema changes (v1 has 84
entries with 3 fstr; v2 has 94 entries with 7 str + 4 fstr), the code
automatically allocates the right sizes. On embedded targets, `malloc` can be
replaced with a bump allocator or pool allocator -- the sizes are known before
the first allocation.

### Schema Migration

| Scenario | What happens | Where to look |
|----------|--------------|---------------|
| **KEEP** | Entries at the same index carry over unchanged | `z0en` (idx 1) |
| **WIDEN** | Type widens from u8 -> u16 automatically | `z0sp` (idx 13) |
| **MOVE** | Entry relocated to a new index via remap table | `ahi` (idx 75 -> 92) |
| **REMOVE** | Old entries silently dropped (not in v2 schema) | `zaen` (idx 11) |
| **ADD** | New v2 entries get their schema defaults | `h0sp` (idx 85), `zn0` (idx 99) |

## Memory Usage Analysis

### Allocated Memory (from measure API)

All buffer sizes are discovered at runtime by `cfgpack_schema_measure()`.

**v1 schema (84 entries, 3 fstr, 0 str):**

| Buffer | Size | How determined |
|--------|-----:|----------------|
| `cfgpack_ctx_t` | 72 B | Fixed struct (16 B presence bitmap) |
| `cfgpack_schema_t` | 88 B | Fixed struct |
| `entries[84]` | 1,344 B | `m.entry_count * 16` |
| `values[84]` | 1,344 B | `m.entry_count * 16` |
| `str_pool` | 51 B | `m.str_pool_size` (3 fstr * 17) |
| `str_offsets` | 6 B | `(m.str_count + m.fstr_count) * 2` |
| **Total** | **2,905 B** | |

**v2 schema (94 entries, 7 str, 4 fstr):**

| Buffer | Size | How determined |
|--------|-----:|----------------|
| `cfgpack_ctx_t` | 72 B | Fixed struct (16 B presence bitmap) |
| `cfgpack_schema_t` | 88 B | Fixed struct |
| `entries[94]` | 1,504 B | `m.entry_count * 16` |
| `values[94]` | 1,504 B | `m.entry_count * 16` |
| `str_pool` | 523 B | `m.str_pool_size` (7 * 65 + 4 * 17) |
| `str_offsets` | 22 B | `(m.str_count + m.fstr_count) * 2` |
| **Total** | **3,713 B** | |

The v2 schema needs 808 B more than v1, mostly due to the 7 variable-length
`str` entries (each reserves 65 B in the pool vs 17 B for `fstr`). The measure
API discovers this automatically.

### Fixed vs Variable Costs

| Category | Size | Notes |
|----------|-----:|-------|
| `cfgpack_ctx_t` | 72 B | Fixed; includes `CFGPACK_MAX_ENTRIES/8` = 16 B bitmap |
| `cfgpack_schema_t` | 88 B | Fixed |
| Per entry (entries + values) | 32 B | 16 B `cfgpack_entry_t` + 16 B `cfgpack_value_t` |
| Per `str` entry (pool) | 65 B | `CFGPACK_STR_MAX + 1` = 65 |
| Per `fstr` entry (pool) | 17 B | `CFGPACK_FSTR_MAX + 1` = 17 |
| Per string entry (offset) | 2 B | `sizeof(uint16_t)` |
| Schema parse buffer | 8,192 B | Reclaimable after init |

### Compile-Time Knobs

| Define | Default | This example | Effect |
|--------|--------:|-------------:|--------|
| `CFGPACK_MAX_ENTRIES` | 128 | 128 (default) | Presence bitmap: 16 B |
| `CFGPACK_SKIP_MAX_DEPTH` | 32 | 8 | Skip-value stack: 32 B vs 128 B |

`CFGPACK_MAX_ENTRIES` sets the upper limit on how many entries the library can
track (the presence bitmap in `cfgpack_ctx_t` is `ceil(MAX_ENTRIES / 8)` bytes).
With 94 entries in v2, the default of 128 provides headroom for future schema
growth. The measure API validates against this limit at runtime.

### Stack Usage

Stack consumption for the operations used in this example, at `-Os`:

| Operation | Stack (-Os) | When called |
|-----------|------------:|-------------|
| `cfgpack_schema_measure` | ~300 B | Twice (v1 + v2) |
| `cfgpack_parse_schema` | ~880 B | Twice (v1 + v2) |
| `cfgpack_init` | ~0 B | Twice (inlined) |
| `cfgpack_set_u8` | ~80 B | Configuration writes |
| `cfgpack_get_u8` | ~80 B | Configuration reads |
| `cfgpack_pageout` | ~208 B | Serialize to flash |
| `cfgpack_pagein_remap` | ~480 B | Migration (once) |
| `cfgpack_peek_name` | ~112 B | Version detection (once) |

**Runtime stack budget**: Under 512 B for all runtime operations.
**Setup stack budget**: ~880 B for schema parsing (one-time cost).

## Schema Migration Takeaways

### How the Remap Table Works

The remap table is an array of `{old_index, new_index}` pairs:

```c
static const cfgpack_remap_entry_t v1_to_v2_remap[] = {
    { 75, 92 },  /* ahi: alarm high temp moved */
    { 76, 93 },  /* alo: alarm low temp moved */
    { 77, 94 },  /* hhi: alarm high humid moved */
    { 78, 95 },  /* hlo: alarm low humid moved */
};
```

During `cfgpack_pagein_remap()`, each key in the old serialized data is checked:

1. If the key matches a remap entry, use the `new_index` to look up the
   destination in the current schema.
2. If no remap entry matches, use the original key directly as the index.
3. If the resulting index exists in the new schema, decode and store the value.
4. If not, silently skip (this is how REMOVE works).

### What's Automatic

- **Type widening** -- u8 -> u16, i8 -> i16, f32 -> f64, fstr -> str. The
  library detects the wire type and coerces to the schema type. No remap entry
  needed.
- **Unknown key skipping** -- old entries whose indices don't exist in the new
  schema are silently dropped. No remap entry needed.
- **Default restoration** -- new entries with defaults in the v2 schema that
  weren't in the old data automatically get their default values, including
  string defaults.

### What Requires Explicit Remap Entries

- **Index moves** -- if an entry's index changes between schemas, you must add a
  `{old_index, new_index}` pair. Without it, the library would try to store the
  value at the old index, which either doesn't exist (silently dropped) or maps
  to a different entry (data corruption).

### Migration Best Practices

1. **Keep indices stable** when possible. Unchanged indices need no remap entries
   and no code changes.

2. **Use the remap table only for moves**. Keep/widen/remove/add are all handled
   implicitly.

3. **The remap table is const** -- it can live in flash/ROM on embedded targets
   with no RAM cost.

4. **Chain migrations** if supporting multiple old versions:
   ```c
   if (strcmp(name, "hvac_v1") == 0)
       cfgpack_pagein_remap(&ctx, data, len, v1_to_v2, 4);
   else if (strcmp(name, "hvac_v2") == 0)
       cfgpack_pagein_buf(&ctx, data, len);
   ```

5. **Test migration with all five scenarios** -- this example verifies keep,
   widen, move, remove, and add (including string defaults) with explicit
   [OK]/[FAIL] checks.

## Files

| File | Description |
|------|-------------|
| `main.c` | Example source -- measure API + 7-phase migration demo |
| `hvac_v1.map` | v1 schema: 84 entries, 12 HVAC zones, 3 fstr |
| `hvac_v2.map` | v2 schema: 94 entries, 10 zones + 7 str + 4 fstr |
| `Makefile` | Build with default `CFGPACK_MAX_ENTRIES` and reduced `CFGPACK_SKIP_MAX_DEPTH` |
