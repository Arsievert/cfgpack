# Schema Versioning & Remapping

CFGPack supports firmware upgrades where the configuration schema changes between versions. This is handled through:

1. **Schema name embedding**: The schema name is automatically stored at reserved index 0 in serialized blobs
2. **Version detection**: Read the schema name from a blob to determine which schema version created it
3. **Index remapping**: Map old indices to new indices when loading config from an older schema version
4. **Type widening**: Automatically coerce values to wider types (e.g., u8 → u16) during remapping

## Reserved Index

Index 0 (`CFGPACK_INDEX_RESERVED_NAME`) is reserved for the schema name. User-defined schema entries should use indices starting at 1.

## Detecting Schema Version

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

## Migrating Between Schema Versions

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

## Default Restoration During Remap

After `cfgpack_pagein_remap()` decodes all entries from the old data, it automatically restores presence for any new-schema entries that have `has_default` set but were not covered by the incoming data. This means:

- New entries added in a schema upgrade that have default values are immediately accessible via `cfgpack_get()` after migration, without any explicit code to set them.
- Entries without defaults (`NIL`) that were not in the old data remain absent (`cfgpack_get()` returns `CFGPACK_ERR_MISSING`).
- If old data contains a value for an entry, that decoded value always takes precedence over the schema default.

This applies to all types including `str` and `fstr` defaults.

## Type Widening Rules

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

## Migration Workflow

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
