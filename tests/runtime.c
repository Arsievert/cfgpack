/* Runtime and I/O bounds tests for cfgpack (no-heap profile). */

#include "cfgpack/cfgpack.h"
#include "cfgpack/io_file.h"
#include "cfgpack/msgpack.h"
#include "test.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

static void make_schema(cfgpack_schema_t *schema, cfgpack_entry_t *entries, size_t n) {
    snprintf(schema->map_name, sizeof(schema->map_name), "test");
    schema->version = 1;
    schema->entry_count = n;
    schema->entries = entries;
    for (size_t i = 0; i < n; ++i) {
        /* Start indices at 1 to avoid conflict with reserved index 0 */
        entries[i].index = (uint16_t)(i + 1);
        snprintf(entries[i].name, sizeof(entries[i].name), "e%zu", i);
        entries[i].type = CFGPACK_TYPE_U8;
        entries[i].has_default = 0;
    }
}

TEST_CASE(test_init_bounds) {
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[2];
    cfgpack_value_t defaults[2];
    uint8_t present[1];

    make_schema(&schema, entries, 2);

    /* too few values */
    CHECK(cfgpack_init(&ctx, &schema, values, 1, defaults, present, sizeof(present)) == CFGPACK_ERR_BOUNDS);

    /* too few present bytes */
    CHECK(cfgpack_init(&ctx, &schema, values, 2, defaults, present, 0) == CFGPACK_ERR_BOUNDS);
    return (TEST_OK);
}

TEST_CASE(test_pagein_zero_len) {
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    cfgpack_value_t defaults[1];
    uint8_t present[1];

    make_schema(&schema, entries, 1);
    CHECK(cfgpack_init(&ctx, &schema, values, 1, defaults, present, sizeof(present)) == CFGPACK_OK);
    CHECK(cfgpack_pagein_buf(&ctx, NULL, 0) == CFGPACK_ERR_DECODE);
    return (TEST_OK);
}

TEST_CASE(test_pagein_file_small_scratch) {
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    cfgpack_value_t defaults[1];
    uint8_t present[1];
    uint8_t scratch[1];

    make_schema(&schema, entries, 1);
    CHECK(cfgpack_init(&ctx, &schema, values, 1, defaults, present, sizeof(present)) == CFGPACK_OK);
    CHECK(cfgpack_pagein_file(&ctx, "tests/data/sample.map", scratch, sizeof(scratch)) == CFGPACK_ERR_IO);
    return (TEST_OK);
}

TEST_CASE(test_pageout_too_large) {
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    cfgpack_value_t defaults[1];
    uint8_t present[1];
    uint8_t out[8];
    cfgpack_value_t v;
    size_t out_len = 0;

    make_schema(&schema, entries, 1);
    CHECK(cfgpack_init(&ctx, &schema, values, 1, defaults, present, sizeof(present)) == CFGPACK_OK);
    v.type = CFGPACK_TYPE_U8;
    v.v.u64 = 1;
    CHECK(cfgpack_set(&ctx, 1, &v) == CFGPACK_OK); /* index 1 (make_schema starts at 1) */
    /* buffer deliberately tiny to trigger ENCODE */
    CHECK(cfgpack_pageout(&ctx, out, sizeof(out), &out_len) == CFGPACK_ERR_ENCODE);
    return (TEST_OK);
}

static void craft_map_with_unknown_key(uint8_t *buf, size_t *len_out) {
    cfgpack_buf_t b;
    cfgpack_buf_init(&b, buf, 64);
    /* map with 1 pair: key=42, val=1 */
    cfgpack_msgpack_encode_map_header(&b, 1);
    cfgpack_msgpack_encode_uint_key(&b, 42);
    cfgpack_msgpack_encode_uint64(&b, 1);
    *len_out = b.len;
}

TEST_CASE(test_decode_unknown_key_skipped) {
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    cfgpack_value_t defaults[1];
    uint8_t present[1];
    uint8_t buf[64];
    size_t len = 0;

    make_schema(&schema, entries, 1);
    CHECK(cfgpack_init(&ctx, &schema, values, 1, defaults, present, sizeof(present)) == CFGPACK_OK);
    craft_map_with_unknown_key(buf, &len);
    /* Unknown keys are now silently skipped for forward compatibility */
    CHECK(cfgpack_pagein_buf(&ctx, buf, len) == CFGPACK_OK);
    /* No values should be present since key 42 is not in schema */
    CHECK(cfgpack_get_size(&ctx) == 0);
    return (TEST_OK);
}

TEST_CASE(test_type_mismatch_and_len) {
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[2];
    cfgpack_value_t defaults[2];
    uint8_t present[(2+7)/8];
    cfgpack_value_t v;

    make_schema(&schema, entries, 2);
    entries[1].type = CFGPACK_TYPE_STR;

    CHECK(cfgpack_init(&ctx, &schema, values, 2, defaults, present, sizeof(present)) == CFGPACK_OK);

    v.type = CFGPACK_TYPE_STR;
    v.v.str.len = CFGPACK_STR_MAX + 1;
    CHECK(cfgpack_set(&ctx, 1, &v) == CFGPACK_ERR_TYPE_MISMATCH); /* index 1 expects U8 */

    v.type = CFGPACK_TYPE_STR;
    v.v.str.len = CFGPACK_STR_MAX + 1;
    CHECK(cfgpack_set(&ctx, 2, &v) == CFGPACK_ERR_STR_TOO_LONG); /* index 2 expects STR */
    return (TEST_OK);
}

TEST_CASE(test_presence_reset) {
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[2];
    cfgpack_value_t defaults[2];
    uint8_t present[(2+7)/8];
    cfgpack_value_t v;
    uint8_t buf[128];
    size_t len = 0;

    make_schema(&schema, entries, 2);
    CHECK(cfgpack_init(&ctx, &schema, values, 2, defaults, present, sizeof(present)) == CFGPACK_OK);

    v.type = CFGPACK_TYPE_U8; v.v.u64 = 1; CHECK(cfgpack_set(&ctx, 1, &v) == CFGPACK_OK);
    v.type = CFGPACK_TYPE_U8; v.v.u64 = 2; CHECK(cfgpack_set(&ctx, 2, &v) == CFGPACK_OK);

    CHECK(cfgpack_pageout(&ctx, buf, sizeof(buf), &len) == CFGPACK_OK);

    memset(values, 0, sizeof(values));
    memset(present, 0, sizeof(present));

    /* manually craft minimal map with only entry index 1 set to 1 */
    /* key 0 is reserved for schema name, so we use key 1 */
    uint8_t tiny[] = {0x81, /* map of 1 */ 0x01, /* key=1 */ 0x01 /* val=1 */};
    CHECK(cfgpack_pagein_buf(&ctx, tiny, sizeof(tiny)) == CFGPACK_OK);
    CHECK(cfgpack_get_size(&ctx) == 1);
    CHECK(cfgpack_get(&ctx, 1, &v) == CFGPACK_OK);
    CHECK(cfgpack_get(&ctx, 2, &v) == CFGPACK_ERR_MISSING);
    return (TEST_OK);
}

TEST_CASE(test_pageout_file_roundtrip_and_io) {
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    cfgpack_value_t defaults[1];
    uint8_t present[1];
    cfgpack_value_t v;
    uint8_t scratch[64];
    const char *path = "build/runtime_tmp.bin";

    make_schema(&schema, entries, 1);
    CHECK(cfgpack_init(&ctx, &schema, values, 1, defaults, present, sizeof(present)) == CFGPACK_OK);

    v.type = CFGPACK_TYPE_U8; v.v.u64 = 9;
    CHECK(cfgpack_set(&ctx, 1, &v) == CFGPACK_OK); /* index 1 (make_schema starts at 1) */

    CHECK(cfgpack_pageout_file(&ctx, path, scratch, sizeof(scratch)) == CFGPACK_OK);

    /* clear state and read back */
    memset(values, 0, sizeof(values));
    memset(present, 0, sizeof(present));
    CHECK(cfgpack_pagein_file(&ctx, path, scratch, sizeof(scratch)) == CFGPACK_OK);
    CHECK(cfgpack_get(&ctx, 1, &v) == CFGPACK_OK);
    CHECK(v.v.u64 == 9);

    /* unwritable path (directory) should return IO error */
    CHECK(cfgpack_pageout_file(&ctx, "build", scratch, sizeof(scratch)) == CFGPACK_ERR_IO);
    return (TEST_OK);
}

TEST_CASE(test_pageout_empty_map) {
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[2];
    cfgpack_value_t defaults[2];
    uint8_t present[(2+7)/8];
    uint8_t buf[32];
    size_t len = 0;

    make_schema(&schema, entries, 2);
    /* Set a schema name */
    snprintf(schema.map_name, sizeof(schema.map_name), "test");
    CHECK(cfgpack_init(&ctx, &schema, values, 2, defaults, present, sizeof(present)) == CFGPACK_OK);
    CHECK(cfgpack_pageout(&ctx, buf, sizeof(buf), &len) == CFGPACK_OK);
    /* Expect map with 1 entry (schema name at key 0): fixmap(1), key=0, fixstr("test") */
    /* 0x81 (map 1), 0x00 (key 0), 0xa4 (fixstr 4), "test" */
    CHECK(len == 7);
    CHECK(buf[0] == 0x81); /* map of 1 */
    CHECK(buf[1] == 0x00); /* key 0 */
    CHECK(buf[2] == 0xa4); /* fixstr of len 4 */
    CHECK(memcmp(&buf[3], "test", 4) == 0);
    return (TEST_OK);
}

TEST_CASE(test_pageout_min_buffer) {
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    cfgpack_value_t defaults[1];
    uint8_t present[1];
    uint8_t buf[32]; /* enough for schema name + 1 value */
    uint8_t small[11];
    cfgpack_value_t v;
    size_t len = 0;

    make_schema(&schema, entries, 1);
    /* Short schema name to keep buffer small */
    snprintf(schema.map_name, sizeof(schema.map_name), "x");
    CHECK(cfgpack_init(&ctx, &schema, values, 1, defaults, present, sizeof(present)) == CFGPACK_OK);
    v.type = CFGPACK_TYPE_U8; v.v.u64 = 1; CHECK(cfgpack_set(&ctx, 1, &v) == CFGPACK_OK);

    CHECK(cfgpack_pageout(&ctx, buf, sizeof(buf), &len) == CFGPACK_OK);
    CHECK(len <= sizeof(buf));
    CHECK(cfgpack_pageout(&ctx, small, sizeof(small), &len) == CFGPACK_ERR_ENCODE);
    return (TEST_OK);
}

TEST_CASE(test_pagein_type_mismatch_map_payload) {
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    cfgpack_value_t defaults[1];
    uint8_t present[1];
    uint8_t buf[16];
    cfgpack_buf_t b;

    make_schema(&schema, entries, 1);
    entries[0].type = CFGPACK_TYPE_STR;
    CHECK(cfgpack_init(&ctx, &schema, values, 1, defaults, present, sizeof(present)) == CFGPACK_OK);

    cfgpack_buf_init(&b, buf, sizeof(buf));
    CHECK(cfgpack_msgpack_encode_map_header(&b, 1) == CFGPACK_OK);
    CHECK(cfgpack_msgpack_encode_uint_key(&b, 1) == CFGPACK_OK); /* index 1 (make_schema starts at 1) */
    CHECK(cfgpack_msgpack_encode_uint64(&b, 5) == CFGPACK_OK); /* wrong type: number instead of str */

    CHECK(cfgpack_pagein_buf(&ctx, buf, b.len) == CFGPACK_ERR_TYPE_MISMATCH);
    return (TEST_OK);
}

TEST_CASE(test_pagein_declared_pairs_exceeds_payload) {
    cfgpack_ctx_t ctx;
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_value_t values[1];
    cfgpack_value_t defaults[1];
    uint8_t present[1];
    /* map of 1: key=1 (our entry index), but value is missing */
    uint8_t buf[] = {0x81, 0x01 /* key=1, missing value */};

    make_schema(&schema, entries, 1);
    CHECK(cfgpack_init(&ctx, &schema, values, 1, defaults, present, sizeof(present)) == CFGPACK_OK);
    CHECK(cfgpack_pagein_buf(&ctx, buf, sizeof(buf)) == CFGPACK_ERR_DECODE);
    return (TEST_OK);
}

TEST_CASE(test_pageout_pagein_all_types) {
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[12];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[12];
    cfgpack_value_t defaults[12];
    uint8_t present[(12 + 7) / 8];
    uint8_t scratch[512];
    const char *path = "build/runtime_all.bin";
    cfgpack_value_t out;

    make_schema(&schema, entries, 12);
    /* make_schema assigns indices 1-12, update types accordingly */
    entries[0].type = CFGPACK_TYPE_U8;
    entries[1].type = CFGPACK_TYPE_U16;
    entries[2].type = CFGPACK_TYPE_U32;
    entries[3].type = CFGPACK_TYPE_U64;
    entries[4].type = CFGPACK_TYPE_I8;
    entries[5].type = CFGPACK_TYPE_I16;
    entries[6].type = CFGPACK_TYPE_I32;
    entries[7].type = CFGPACK_TYPE_I64;
    entries[8].type = CFGPACK_TYPE_F32;
    entries[9].type = CFGPACK_TYPE_F64;
    entries[10].type = CFGPACK_TYPE_STR;
    entries[11].type = CFGPACK_TYPE_FSTR;

    CHECK(cfgpack_init(&ctx, &schema, values, 12, defaults, present, sizeof(present)) == CFGPACK_OK);

    cfgpack_value_t v;
    /* indices are 1-12 (make_schema starts at 1) */
    v.type = CFGPACK_TYPE_U8; v.v.u64 = 1; CHECK(cfgpack_set(&ctx, 1, &v) == CFGPACK_OK);
    v.type = CFGPACK_TYPE_U16; v.v.u64 = 2; CHECK(cfgpack_set(&ctx, 2, &v) == CFGPACK_OK);
    v.type = CFGPACK_TYPE_U32; v.v.u64 = 3; CHECK(cfgpack_set(&ctx, 3, &v) == CFGPACK_OK);
    v.type = CFGPACK_TYPE_U64; v.v.u64 = 4; CHECK(cfgpack_set(&ctx, 4, &v) == CFGPACK_OK);
    v.type = CFGPACK_TYPE_I8; v.v.i64 = -1; CHECK(cfgpack_set(&ctx, 5, &v) == CFGPACK_OK);
    v.type = CFGPACK_TYPE_I16; v.v.i64 = -2; CHECK(cfgpack_set(&ctx, 6, &v) == CFGPACK_OK);
    v.type = CFGPACK_TYPE_I32; v.v.i64 = -3; CHECK(cfgpack_set(&ctx, 7, &v) == CFGPACK_OK);
    v.type = CFGPACK_TYPE_I64; v.v.i64 = -4; CHECK(cfgpack_set(&ctx, 8, &v) == CFGPACK_OK);
    v.type = CFGPACK_TYPE_F32; v.v.f32 = 1.25f; CHECK(cfgpack_set(&ctx, 9, &v) == CFGPACK_OK);
    v.type = CFGPACK_TYPE_F64; v.v.f64 = 2.5; CHECK(cfgpack_set(&ctx, 10, &v) == CFGPACK_OK);
    v.type = CFGPACK_TYPE_STR; v.v.str.len = 3; memcpy(v.v.str.data, "foo", 3); v.v.str.data[3] = '\0'; CHECK(cfgpack_set(&ctx, 11, &v) == CFGPACK_OK);
    v.type = CFGPACK_TYPE_FSTR; v.v.fstr.len = 3; memcpy(v.v.fstr.data, "bar", 3); v.v.fstr.data[3] = '\0'; CHECK(cfgpack_set(&ctx, 12, &v) == CFGPACK_OK);

    CHECK(cfgpack_pageout_file(&ctx, path, scratch, sizeof(scratch)) == CFGPACK_OK);

    /* Clear state and read back from file. */
    memset(values, 0, sizeof(values));
    memset(present, 0, sizeof(present));

    CHECK(cfgpack_pagein_file(&ctx, path, scratch, sizeof(scratch)) == CFGPACK_OK);

    CHECK(cfgpack_get(&ctx, 1, &out) == CFGPACK_OK && out.v.u64 == 1);
    CHECK(cfgpack_get(&ctx, 2, &out) == CFGPACK_OK && out.v.u64 == 2);
    CHECK(cfgpack_get(&ctx, 3, &out) == CFGPACK_OK && out.v.u64 == 3);
    CHECK(cfgpack_get(&ctx, 4, &out) == CFGPACK_OK && out.v.u64 == 4);
    CHECK(cfgpack_get(&ctx, 5, &out) == CFGPACK_OK && out.v.i64 == -1);
    CHECK(cfgpack_get(&ctx, 6, &out) == CFGPACK_OK && out.v.i64 == -2);
    CHECK(cfgpack_get(&ctx, 7, &out) == CFGPACK_OK && out.v.i64 == -3);
    CHECK(cfgpack_get(&ctx, 8, &out) == CFGPACK_OK && out.v.i64 == -4);
    CHECK(cfgpack_get(&ctx, 9, &out) == CFGPACK_OK && fabsf(out.v.f32 - 1.25f) < 1e-6f);
    CHECK(cfgpack_get(&ctx, 10, &out) == CFGPACK_OK && fabs(out.v.f64 - 2.5) < 1e-9);
    CHECK(cfgpack_get(&ctx, 11, &out) == CFGPACK_OK && out.v.str.len == 3 && strncmp(out.v.str.data, "foo", 3) == 0);
    CHECK(cfgpack_get(&ctx, 12, &out) == CFGPACK_OK && out.v.fstr.len == 3 && strncmp(out.v.fstr.data, "bar", 3) == 0);
    return (TEST_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Remapping Feature Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST_CASE(test_peek_name) {
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    cfgpack_value_t defaults[1];
    uint8_t present[1];
    uint8_t buf[64];
    size_t len = 0;
    char name[32];

    make_schema(&schema, entries, 1);
    snprintf(schema.map_name, sizeof(schema.map_name), "myschema");
    CHECK(cfgpack_init(&ctx, &schema, values, 1, defaults, present, sizeof(present)) == CFGPACK_OK);

    CHECK(cfgpack_pageout(&ctx, buf, sizeof(buf), &len) == CFGPACK_OK);
    CHECK(cfgpack_peek_name(buf, len, name, sizeof(name)) == CFGPACK_OK);
    CHECK(strcmp(name, "myschema") == 0);
    return (TEST_OK);
}

TEST_CASE(test_peek_name_missing) {
    /* Craft a blob without key 0 (for backward compat with old blobs) */
    uint8_t buf[8];
    cfgpack_buf_t b;
    char name[32];

    cfgpack_buf_init(&b, buf, sizeof(buf));
    cfgpack_msgpack_encode_map_header(&b, 1);
    cfgpack_msgpack_encode_uint_key(&b, 5); /* key != 0 */
    cfgpack_msgpack_encode_uint64(&b, 42);

    CHECK(cfgpack_peek_name(buf, b.len, name, sizeof(name)) == CFGPACK_ERR_MISSING);
    return (TEST_OK);
}

TEST_CASE(test_peek_name_bounds) {
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    cfgpack_value_t defaults[1];
    uint8_t present[1];
    uint8_t buf[64];
    size_t len = 0;
    char small[4]; /* too small for "myschema" + null */

    make_schema(&schema, entries, 1);
    snprintf(schema.map_name, sizeof(schema.map_name), "myschema");
    CHECK(cfgpack_init(&ctx, &schema, values, 1, defaults, present, sizeof(present)) == CFGPACK_OK);

    CHECK(cfgpack_pageout(&ctx, buf, sizeof(buf), &len) == CFGPACK_OK);
    CHECK(cfgpack_peek_name(buf, len, small, sizeof(small)) == CFGPACK_ERR_BOUNDS);
    return (TEST_OK);
}

TEST_CASE(test_remap_basic) {
    /* Old schema: entry at index 10
     * New schema: entry at index 20
     * Remap: 10 -> 20
     */
    cfgpack_schema_t old_schema, new_schema;
    cfgpack_entry_t old_entries[1], new_entries[1];
    cfgpack_ctx_t old_ctx, new_ctx;
    cfgpack_value_t old_values[1], new_values[1];
    cfgpack_value_t old_defaults[1], new_defaults[1];
    uint8_t old_present[1], new_present[1];
    uint8_t buf[64];
    size_t len = 0;
    cfgpack_value_t v;

    /* Setup old schema with index 10 */
    old_schema.map_name[0] = '\0';
    snprintf(old_schema.map_name, sizeof(old_schema.map_name), "old");
    old_schema.version = 1;
    old_schema.entry_count = 1;
    old_schema.entries = old_entries;
    old_entries[0].index = 10;
    snprintf(old_entries[0].name, sizeof(old_entries[0].name), "val");
    old_entries[0].type = CFGPACK_TYPE_U8;
    old_entries[0].has_default = 0;

    /* Setup new schema with index 20 */
    new_schema.map_name[0] = '\0';
    snprintf(new_schema.map_name, sizeof(new_schema.map_name), "new");
    new_schema.version = 2;
    new_schema.entry_count = 1;
    new_schema.entries = new_entries;
    new_entries[0].index = 20;
    snprintf(new_entries[0].name, sizeof(new_entries[0].name), "val");
    new_entries[0].type = CFGPACK_TYPE_U8;
    new_entries[0].has_default = 0;

    CHECK(cfgpack_init(&old_ctx, &old_schema, old_values, 1, old_defaults, old_present, sizeof(old_present)) == CFGPACK_OK);
    CHECK(cfgpack_init(&new_ctx, &new_schema, new_values, 1, new_defaults, new_present, sizeof(new_present)) == CFGPACK_OK);

    /* Set value in old context and pageout */
    v.type = CFGPACK_TYPE_U8; v.v.u64 = 42;
    CHECK(cfgpack_set(&old_ctx, 10, &v) == CFGPACK_OK);
    CHECK(cfgpack_pageout(&old_ctx, buf, sizeof(buf), &len) == CFGPACK_OK);

    /* Pagein to new context with remap */
    cfgpack_remap_entry_t remap[] = { {10, 20} };
    CHECK(cfgpack_pagein_remap(&new_ctx, buf, len, remap, 1) == CFGPACK_OK);

    /* Verify value at new index */
    CHECK(cfgpack_get(&new_ctx, 20, &v) == CFGPACK_OK);
    CHECK(v.v.u64 == 42);
    return (TEST_OK);
}

TEST_CASE(test_remap_type_widening) {
    /* Old schema: u8 at index 1
     * New schema: u16 at index 1
     * Should succeed (widening allowed)
     */
    cfgpack_schema_t old_schema, new_schema;
    cfgpack_entry_t old_entries[1], new_entries[1];
    cfgpack_ctx_t old_ctx, new_ctx;
    cfgpack_value_t old_values[1], new_values[1];
    cfgpack_value_t old_defaults[1], new_defaults[1];
    uint8_t old_present[1], new_present[1];
    uint8_t buf[64];
    size_t len = 0;
    cfgpack_value_t v;

    /* Old schema: u8 */
    old_schema.map_name[0] = '\0';
    snprintf(old_schema.map_name, sizeof(old_schema.map_name), "old");
    old_schema.version = 1;
    old_schema.entry_count = 1;
    old_schema.entries = old_entries;
    old_entries[0].index = 1;
    snprintf(old_entries[0].name, sizeof(old_entries[0].name), "val");
    old_entries[0].type = CFGPACK_TYPE_U8;
    old_entries[0].has_default = 0;

    /* New schema: u16 */
    new_schema.map_name[0] = '\0';
    snprintf(new_schema.map_name, sizeof(new_schema.map_name), "new");
    new_schema.version = 2;
    new_schema.entry_count = 1;
    new_schema.entries = new_entries;
    new_entries[0].index = 1;
    snprintf(new_entries[0].name, sizeof(new_entries[0].name), "val");
    new_entries[0].type = CFGPACK_TYPE_U16;
    new_entries[0].has_default = 0;

    CHECK(cfgpack_init(&old_ctx, &old_schema, old_values, 1, old_defaults, old_present, sizeof(old_present)) == CFGPACK_OK);
    CHECK(cfgpack_init(&new_ctx, &new_schema, new_values, 1, new_defaults, new_present, sizeof(new_present)) == CFGPACK_OK);

    /* Set u8 value in old context */
    v.type = CFGPACK_TYPE_U8; v.v.u64 = 200;
    CHECK(cfgpack_set(&old_ctx, 1, &v) == CFGPACK_OK);
    CHECK(cfgpack_pageout(&old_ctx, buf, sizeof(buf), &len) == CFGPACK_OK);

    /* Pagein to new context - widening u8->u16 should succeed */
    CHECK(cfgpack_pagein_remap(&new_ctx, buf, len, NULL, 0) == CFGPACK_OK);
    CHECK(cfgpack_get(&new_ctx, 1, &v) == CFGPACK_OK);
    CHECK(v.v.u64 == 200);
    return (TEST_OK);
}

TEST_CASE(test_remap_type_narrowing_rejected) {
    /* Old schema: u16 at index 1
     * New schema: u8 at index 1
     * Should fail (narrowing not allowed)
     */
    cfgpack_schema_t old_schema, new_schema;
    cfgpack_entry_t old_entries[1], new_entries[1];
    cfgpack_ctx_t old_ctx, new_ctx;
    cfgpack_value_t old_values[1], new_values[1];
    cfgpack_value_t old_defaults[1], new_defaults[1];
    uint8_t old_present[1], new_present[1];
    uint8_t buf[64];
    size_t len = 0;
    cfgpack_value_t v;

    /* Old schema: u16 */
    old_schema.map_name[0] = '\0';
    snprintf(old_schema.map_name, sizeof(old_schema.map_name), "old");
    old_schema.version = 1;
    old_schema.entry_count = 1;
    old_schema.entries = old_entries;
    old_entries[0].index = 1;
    snprintf(old_entries[0].name, sizeof(old_entries[0].name), "val");
    old_entries[0].type = CFGPACK_TYPE_U16;
    old_entries[0].has_default = 0;

    /* New schema: u8 */
    new_schema.map_name[0] = '\0';
    snprintf(new_schema.map_name, sizeof(new_schema.map_name), "new");
    new_schema.version = 2;
    new_schema.entry_count = 1;
    new_schema.entries = new_entries;
    new_entries[0].index = 1;
    snprintf(new_entries[0].name, sizeof(new_entries[0].name), "val");
    new_entries[0].type = CFGPACK_TYPE_U8;
    new_entries[0].has_default = 0;

    CHECK(cfgpack_init(&old_ctx, &old_schema, old_values, 1, old_defaults, old_present, sizeof(old_present)) == CFGPACK_OK);
    CHECK(cfgpack_init(&new_ctx, &new_schema, new_values, 1, new_defaults, new_present, sizeof(new_present)) == CFGPACK_OK);

    /* Set u16 value in old context */
    v.type = CFGPACK_TYPE_U16; v.v.u64 = 1000;
    CHECK(cfgpack_set(&old_ctx, 1, &v) == CFGPACK_OK);
    CHECK(cfgpack_pageout(&old_ctx, buf, sizeof(buf), &len) == CFGPACK_OK);

    /* Pagein to new context - narrowing u16->u8 should fail */
    CHECK(cfgpack_pagein_remap(&new_ctx, buf, len, NULL, 0) == CFGPACK_ERR_TYPE_MISMATCH);
    return (TEST_OK);
}

TEST_CASE(test_remap_reserved_index_skipped) {
    /* Key 0 in blob is schema name, not a config value */
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    cfgpack_value_t defaults[1];
    uint8_t present[1];
    uint8_t buf[64];
    size_t len = 0;

    make_schema(&schema, entries, 1);
    /* Entry has index 0 which conflicts with reserved */
    entries[0].index = 1; /* Use index 1 instead */
    snprintf(schema.map_name, sizeof(schema.map_name), "test");
    CHECK(cfgpack_init(&ctx, &schema, values, 1, defaults, present, sizeof(present)) == CFGPACK_OK);

    /* Craft blob with key 0 = string (schema name) */
    cfgpack_buf_t b;
    cfgpack_buf_init(&b, buf, sizeof(buf));
    cfgpack_msgpack_encode_map_header(&b, 2);
    cfgpack_msgpack_encode_uint_key(&b, 0);
    cfgpack_msgpack_encode_str(&b, "test", 4);
    cfgpack_msgpack_encode_uint_key(&b, 1);
    cfgpack_msgpack_encode_uint64(&b, 99);
    len = b.len;

    CHECK(cfgpack_pagein_buf(&ctx, buf, len) == CFGPACK_OK);
    /* Key 0 should be skipped, key 1 should be loaded */
    cfgpack_value_t v;
    CHECK(cfgpack_get(&ctx, 1, &v) == CFGPACK_OK);
    CHECK(v.v.u64 == 99);
    return (TEST_OK);
}

TEST_CASE(test_roundtrip_with_name) {
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[2];
    cfgpack_value_t defaults[2];
    uint8_t present[1];
    uint8_t buf[128];
    size_t len = 0;
    cfgpack_value_t v;
    char name[32];

    make_schema(&schema, entries, 2);
    entries[0].index = 1;
    entries[1].index = 2;
    snprintf(schema.map_name, sizeof(schema.map_name), "config_v1");
    CHECK(cfgpack_init(&ctx, &schema, values, 2, defaults, present, sizeof(present)) == CFGPACK_OK);

    v.type = CFGPACK_TYPE_U8; v.v.u64 = 10;
    CHECK(cfgpack_set(&ctx, 1, &v) == CFGPACK_OK);
    v.type = CFGPACK_TYPE_U8; v.v.u64 = 20;
    CHECK(cfgpack_set(&ctx, 2, &v) == CFGPACK_OK);

    CHECK(cfgpack_pageout(&ctx, buf, sizeof(buf), &len) == CFGPACK_OK);

    /* Verify name is in blob */
    CHECK(cfgpack_peek_name(buf, len, name, sizeof(name)) == CFGPACK_OK);
    CHECK(strcmp(name, "config_v1") == 0);

    /* Clear and read back */
    memset(values, 0, sizeof(values));
    memset(present, 0, sizeof(present));
    CHECK(cfgpack_pagein_buf(&ctx, buf, len) == CFGPACK_OK);

    CHECK(cfgpack_get(&ctx, 1, &v) == CFGPACK_OK && v.v.u64 == 10);
    CHECK(cfgpack_get(&ctx, 2, &v) == CFGPACK_OK && v.v.u64 == 20);
    return (TEST_OK);
}

int main(void) {
    test_result_t overall = TEST_OK;

    overall |= (test_case_result("init_bounds", test_init_bounds()) != TEST_OK);
    overall |= (test_case_result("pagein_zero_len", test_pagein_zero_len()) != TEST_OK);
    overall |= (test_case_result("pagein_file_small_scratch", test_pagein_file_small_scratch()) != TEST_OK);
    overall |= (test_case_result("pageout_too_large", test_pageout_too_large()) != TEST_OK);
    overall |= (test_case_result("decode_unknown_key_skipped", test_decode_unknown_key_skipped()) != TEST_OK);
    overall |= (test_case_result("type_mismatch_and_len", test_type_mismatch_and_len()) != TEST_OK);
    overall |= (test_case_result("presence_reset", test_presence_reset()) != TEST_OK);
    overall |= (test_case_result("pageout_file_roundtrip_and_io", test_pageout_file_roundtrip_and_io()) != TEST_OK);
    overall |= (test_case_result("pageout_empty_map", test_pageout_empty_map()) != TEST_OK);
    overall |= (test_case_result("pageout_min_buffer", test_pageout_min_buffer()) != TEST_OK);
    overall |= (test_case_result("pagein_type_mismatch_map_payload", test_pagein_type_mismatch_map_payload()) != TEST_OK);
    overall |= (test_case_result("pagein_declared_pairs_exceeds_payload", test_pagein_declared_pairs_exceeds_payload()) != TEST_OK);
    overall |= (test_case_result("pageout_pagein_all_types", test_pageout_pagein_all_types()) != TEST_OK);

    /* Remapping feature tests */
    overall |= (test_case_result("peek_name", test_peek_name()) != TEST_OK);
    overall |= (test_case_result("peek_name_missing", test_peek_name_missing()) != TEST_OK);
    overall |= (test_case_result("peek_name_bounds", test_peek_name_bounds()) != TEST_OK);
    overall |= (test_case_result("remap_basic", test_remap_basic()) != TEST_OK);
    overall |= (test_case_result("remap_type_widening", test_remap_type_widening()) != TEST_OK);
    overall |= (test_case_result("remap_type_narrowing_rejected", test_remap_type_narrowing_rejected()) != TEST_OK);
    overall |= (test_case_result("remap_reserved_index_skipped", test_remap_reserved_index_skipped()) != TEST_OK);
    overall |= (test_case_result("roundtrip_with_name", test_roundtrip_with_name()) != TEST_OK);

    if (overall == TEST_OK) {
        printf(COLOR_GREEN "ALL PASS" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "SOME FAIL" COLOR_RESET "\n");
    }
    return (overall);
}
