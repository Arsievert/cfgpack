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
    LOG_SECTION("Init bounds checking");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[2];
    cfgpack_value_t defaults[2];
    uint8_t present[1];

    LOG("Creating schema with 2 entries");
    make_schema(&schema, entries, 2);

    LOG("Testing init with too few values (1 instead of 2)");
    CHECK(cfgpack_init(&ctx, &schema, values, 1, defaults, present, sizeof(present)) == CFGPACK_ERR_BOUNDS);
    LOG("Correctly rejected: CFGPACK_ERR_BOUNDS");

    LOG("Testing init with too few present bytes (0 instead of 1)");
    CHECK(cfgpack_init(&ctx, &schema, values, 2, defaults, present, 0) == CFGPACK_ERR_BOUNDS);
    LOG("Correctly rejected: CFGPACK_ERR_BOUNDS");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_pagein_zero_len) {
    LOG_SECTION("Pagein with zero-length buffer");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    cfgpack_value_t defaults[1];
    uint8_t present[1];

    LOG("Creating schema with 1 entry");
    make_schema(&schema, entries, 1);

    LOG("Initializing context");
    CHECK(cfgpack_init(&ctx, &schema, values, 1, defaults, present, sizeof(present)) == CFGPACK_OK);
    LOG("Context initialized successfully");

    LOG("Testing pagein with NULL buffer and 0 length");
    CHECK(cfgpack_pagein_buf(&ctx, NULL, 0) == CFGPACK_ERR_DECODE);
    LOG("Correctly rejected: CFGPACK_ERR_DECODE");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_pagein_file_small_scratch) {
    LOG_SECTION("Pagein file with undersized scratch buffer");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    cfgpack_value_t defaults[1];
    uint8_t present[1];
    uint8_t scratch[1];

    LOG("Creating schema with 1 entry");
    make_schema(&schema, entries, 1);

    LOG("Initializing context");
    CHECK(cfgpack_init(&ctx, &schema, values, 1, defaults, present, sizeof(present)) == CFGPACK_OK);
    LOG("Context initialized successfully");

    LOG("Testing pagein_file with 1-byte scratch buffer");
    CHECK(cfgpack_pagein_file(&ctx, "tests/data/sample.map", scratch, sizeof(scratch)) == CFGPACK_ERR_IO);
    LOG("Correctly rejected: CFGPACK_ERR_IO");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_pageout_too_large) {
    LOG_SECTION("Pageout with undersized output buffer");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    cfgpack_value_t defaults[1];
    uint8_t present[1];
    uint8_t out[8];
    cfgpack_value_t v;
    size_t out_len = 0;

    LOG("Creating schema with 1 entry");
    make_schema(&schema, entries, 1);

    LOG("Initializing context");
    CHECK(cfgpack_init(&ctx, &schema, values, 1, defaults, present, sizeof(present)) == CFGPACK_OK);
    LOG("Context initialized successfully");

    LOG("Setting value at index 1 (u8 = 1)");
    v.type = CFGPACK_TYPE_U8;
    v.v.u64 = 1;
    CHECK(cfgpack_set(&ctx, 1, &v) == CFGPACK_OK);
    LOG("Value set successfully");

    LOG("Testing pageout with 8-byte buffer (too small for map + schema name)");
    CHECK(cfgpack_pageout(&ctx, out, sizeof(out), &out_len) == CFGPACK_ERR_ENCODE);
    LOG("Correctly rejected: CFGPACK_ERR_ENCODE");

    LOG("Test completed successfully");
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
    LOG_SECTION("Decoding with unknown key (forward compatibility)");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    cfgpack_value_t defaults[1];
    uint8_t present[1];
    uint8_t buf[64];
    size_t len = 0;

    LOG("Creating schema with 1 entry at index 1");
    make_schema(&schema, entries, 1);

    LOG("Initializing context");
    CHECK(cfgpack_init(&ctx, &schema, values, 1, defaults, present, sizeof(present)) == CFGPACK_OK);
    LOG("Context initialized successfully");

    LOG("Crafting msgpack with unknown key 42");
    craft_map_with_unknown_key(buf, &len);
    LOG_HEX("Crafted buffer", buf, len);

    LOG("Pagein should succeed (unknown keys silently skipped)");
    CHECK(cfgpack_pagein_buf(&ctx, buf, len) == CFGPACK_OK);
    LOG("Pagein succeeded");

    LOG("Verifying no values present (key 42 not in schema)");
    CHECK(cfgpack_get_size(&ctx) == 0);
    LOG("get_size = 0 (correct)");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_type_mismatch_and_len) {
    LOG_SECTION("Type mismatch and string length validation");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[2];
    cfgpack_value_t defaults[2];
    uint8_t present[(2+7)/8];
    cfgpack_value_t v;

    LOG("Creating schema: index 1 = u8, index 2 = str");
    make_schema(&schema, entries, 2);
    entries[1].type = CFGPACK_TYPE_STR;

    LOG("Initializing context");
    CHECK(cfgpack_init(&ctx, &schema, values, 2, defaults, present, sizeof(present)) == CFGPACK_OK);
    LOG("Context initialized successfully");

    LOG("Testing type mismatch: setting str at index 1 (expects u8)");
    v.type = CFGPACK_TYPE_STR;
    v.v.str.len = CFGPACK_STR_MAX + 1;
    CHECK(cfgpack_set(&ctx, 1, &v) == CFGPACK_ERR_TYPE_MISMATCH);
    LOG("Correctly rejected: CFGPACK_ERR_TYPE_MISMATCH");

    LOG("Testing string too long at index 2 (str, len=%d > max=%d)", CFGPACK_STR_MAX + 1, CFGPACK_STR_MAX);
    v.type = CFGPACK_TYPE_STR;
    v.v.str.len = CFGPACK_STR_MAX + 1;
    CHECK(cfgpack_set(&ctx, 2, &v) == CFGPACK_ERR_STR_TOO_LONG);
    LOG("Correctly rejected: CFGPACK_ERR_STR_TOO_LONG");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_presence_reset) {
    LOG_SECTION("Presence tracking reset on pagein");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[2];
    cfgpack_value_t defaults[2];
    uint8_t present[(2+7)/8];
    cfgpack_value_t v;
    uint8_t buf[128];
    size_t len = 0;

    LOG("Creating schema with 2 entries");
    make_schema(&schema, entries, 2);

    LOG("Initializing context");
    CHECK(cfgpack_init(&ctx, &schema, values, 2, defaults, present, sizeof(present)) == CFGPACK_OK);
    LOG("Context initialized successfully");

    LOG("Setting both values: index 1 = 1, index 2 = 2");
    v.type = CFGPACK_TYPE_U8; v.v.u64 = 1; CHECK(cfgpack_set(&ctx, 1, &v) == CFGPACK_OK);
    v.type = CFGPACK_TYPE_U8; v.v.u64 = 2; CHECK(cfgpack_set(&ctx, 2, &v) == CFGPACK_OK);
    LOG("Both values set, get_size = %zu", cfgpack_get_size(&ctx));

    LOG("Pageout to buffer");
    CHECK(cfgpack_pageout(&ctx, buf, sizeof(buf), &len) == CFGPACK_OK);
    LOG("Pageout succeeded, len = %zu", len);
    LOG_HEX("Pageout buffer", buf, len);

    LOG("Clearing values and present bitmap");
    memset(values, 0, sizeof(values));
    memset(present, 0, sizeof(present));

    LOG("Crafting minimal map with only index 1 set to 1");
    uint8_t tiny[] = {0x81, /* map of 1 */ 0x01, /* key=1 */ 0x01 /* val=1 */};
    LOG_HEX("Tiny map", tiny, sizeof(tiny));

    LOG("Pagein minimal map");
    CHECK(cfgpack_pagein_buf(&ctx, tiny, sizeof(tiny)) == CFGPACK_OK);
    LOG("Pagein succeeded, get_size = %zu", cfgpack_get_size(&ctx));

    CHECK(cfgpack_get_size(&ctx) == 1);
    LOG("Verifying index 1 is present");
    CHECK(cfgpack_get(&ctx, 1, &v) == CFGPACK_OK);
    LOG("Index 1: u8 = %" PRIu64, v.v.u64);

    LOG("Verifying index 2 is missing");
    CHECK(cfgpack_get(&ctx, 2, &v) == CFGPACK_ERR_MISSING);
    LOG("Index 2 correctly missing: CFGPACK_ERR_MISSING");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_pageout_file_roundtrip_and_io) {
    LOG_SECTION("File I/O roundtrip and error handling");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    cfgpack_value_t defaults[1];
    uint8_t present[1];
    cfgpack_value_t v;
    uint8_t scratch[64];
    const char *path = "build/runtime_tmp.bin";

    LOG("Creating schema with 1 entry");
    make_schema(&schema, entries, 1);

    LOG("Initializing context");
    CHECK(cfgpack_init(&ctx, &schema, values, 1, defaults, present, sizeof(present)) == CFGPACK_OK);
    LOG("Context initialized successfully");

    LOG("Setting value at index 1 (u8 = 9)");
    v.type = CFGPACK_TYPE_U8; v.v.u64 = 9;
    CHECK(cfgpack_set(&ctx, 1, &v) == CFGPACK_OK);
    LOG("Value set successfully");

    LOG("Pageout to file: %s", path);
    CHECK(cfgpack_pageout_file(&ctx, path, scratch, sizeof(scratch)) == CFGPACK_OK);
    LOG("Pageout to file succeeded");

    LOG("Clearing state and reading back");
    memset(values, 0, sizeof(values));
    memset(present, 0, sizeof(present));

    LOG("Pagein from file: %s", path);
    CHECK(cfgpack_pagein_file(&ctx, path, scratch, sizeof(scratch)) == CFGPACK_OK);
    LOG("Pagein from file succeeded");

    LOG("Verifying value at index 1");
    CHECK(cfgpack_get(&ctx, 1, &v) == CFGPACK_OK);
    CHECK(v.v.u64 == 9);
    LOG("Value verified: u8 = %" PRIu64, v.v.u64);

    LOG("Testing pageout to directory (should fail)");
    CHECK(cfgpack_pageout_file(&ctx, "build", scratch, sizeof(scratch)) == CFGPACK_ERR_IO);
    LOG("Correctly rejected: CFGPACK_ERR_IO");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_pageout_empty_map) {
    LOG_SECTION("Pageout with no values set (only schema name)");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[2];
    cfgpack_value_t defaults[2];
    uint8_t present[(2+7)/8];
    uint8_t buf[32];
    size_t len = 0;

    LOG("Creating schema with 2 entries, name='test'");
    make_schema(&schema, entries, 2);
    snprintf(schema.map_name, sizeof(schema.map_name), "test");

    LOG("Initializing context");
    CHECK(cfgpack_init(&ctx, &schema, values, 2, defaults, present, sizeof(present)) == CFGPACK_OK);
    LOG("Context initialized successfully");

    LOG("Pageout with no values set");
    CHECK(cfgpack_pageout(&ctx, buf, sizeof(buf), &len) == CFGPACK_OK);
    LOG("Pageout succeeded, len = %zu", len);
    LOG_HEX("Output buffer", buf, len);

    LOG("Verifying output structure:");
    LOG("  Expected: map(1), key=0, fixstr(4), 'test'");
    CHECK(len == 7);
    LOG("  len = 7 (correct)");
    CHECK(buf[0] == 0x81); /* map of 1 */
    LOG("  buf[0] = 0x81 (map of 1)");
    CHECK(buf[1] == 0x00); /* key 0 */
    LOG("  buf[1] = 0x00 (key 0 = schema name)");
    CHECK(buf[2] == 0xa4); /* fixstr of len 4 */
    LOG("  buf[2] = 0xa4 (fixstr len 4)");
    CHECK(memcmp(&buf[3], "test", 4) == 0);
    LOG("  buf[3..6] = 'test'");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_pageout_min_buffer) {
    LOG_SECTION("Pageout buffer size boundary check");

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

    LOG("Creating schema with 1 entry, short name='x'");
    make_schema(&schema, entries, 1);
    snprintf(schema.map_name, sizeof(schema.map_name), "x");

    LOG("Initializing context");
    CHECK(cfgpack_init(&ctx, &schema, values, 1, defaults, present, sizeof(present)) == CFGPACK_OK);
    LOG("Context initialized successfully");

    LOG("Setting value at index 1 (u8 = 1)");
    v.type = CFGPACK_TYPE_U8; v.v.u64 = 1; CHECK(cfgpack_set(&ctx, 1, &v) == CFGPACK_OK);
    LOG("Value set successfully");

    LOG("Pageout with sufficient buffer (32 bytes)");
    CHECK(cfgpack_pageout(&ctx, buf, sizeof(buf), &len) == CFGPACK_OK);
    LOG("Pageout succeeded, len = %zu", len);
    LOG_HEX("Output buffer", buf, len);

    LOG("Testing pageout with 11-byte buffer (too small)");
    CHECK(cfgpack_pageout(&ctx, small, sizeof(small), &len) == CFGPACK_ERR_ENCODE);
    LOG("Correctly rejected: CFGPACK_ERR_ENCODE");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_pagein_type_mismatch_map_payload) {
    LOG_SECTION("Pagein with type mismatch in payload");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    cfgpack_value_t defaults[1];
    uint8_t present[1];
    uint8_t buf[16];
    cfgpack_buf_t b;

    LOG("Creating schema: index 1 = str");
    make_schema(&schema, entries, 1);
    entries[0].type = CFGPACK_TYPE_STR;

    LOG("Initializing context");
    CHECK(cfgpack_init(&ctx, &schema, values, 1, defaults, present, sizeof(present)) == CFGPACK_OK);
    LOG("Context initialized successfully");

    LOG("Crafting msgpack with number at index 1 (expects str)");
    cfgpack_buf_init(&b, buf, sizeof(buf));
    CHECK(cfgpack_msgpack_encode_map_header(&b, 1) == CFGPACK_OK);
    CHECK(cfgpack_msgpack_encode_uint_key(&b, 1) == CFGPACK_OK);
    CHECK(cfgpack_msgpack_encode_uint64(&b, 5) == CFGPACK_OK); /* wrong type: number instead of str */
    LOG_HEX("Crafted buffer", buf, b.len);

    LOG("Pagein should fail with type mismatch");
    CHECK(cfgpack_pagein_buf(&ctx, buf, b.len) == CFGPACK_ERR_TYPE_MISMATCH);
    LOG("Correctly rejected: CFGPACK_ERR_TYPE_MISMATCH");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_pagein_declared_pairs_exceeds_payload) {
    LOG_SECTION("Pagein with truncated payload (map declares more pairs than present)");

    cfgpack_ctx_t ctx;
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_value_t values[1];
    cfgpack_value_t defaults[1];
    uint8_t present[1];
    /* map of 1: key=1 (our entry index), but value is missing */
    uint8_t buf[] = {0x81, 0x01 /* key=1, missing value */};

    LOG("Creating schema with 1 entry");
    make_schema(&schema, entries, 1);

    LOG("Initializing context");
    CHECK(cfgpack_init(&ctx, &schema, values, 1, defaults, present, sizeof(present)) == CFGPACK_OK);
    LOG("Context initialized successfully");

    LOG("Crafting truncated msgpack: map(1) with key but no value");
    LOG_HEX("Truncated buffer", buf, sizeof(buf));

    LOG("Pagein should fail with decode error");
    CHECK(cfgpack_pagein_buf(&ctx, buf, sizeof(buf)) == CFGPACK_ERR_DECODE);
    LOG("Correctly rejected: CFGPACK_ERR_DECODE");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_pageout_pagein_all_types) {
    LOG_SECTION("Roundtrip all supported types via file I/O");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[12];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[12];
    cfgpack_value_t defaults[12];
    uint8_t present[(12 + 7) / 8];
    uint8_t scratch[512];
    const char *path = "build/runtime_all.bin";
    cfgpack_value_t out;

    LOG("Creating schema with 12 entries (all types)");
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
    LOG("Types: u8, u16, u32, u64, i8, i16, i32, i64, f32, f64, str, fstr");

    LOG("Initializing context");
    CHECK(cfgpack_init(&ctx, &schema, values, 12, defaults, present, sizeof(present)) == CFGPACK_OK);
    LOG("Context initialized successfully");

    LOG("Setting all 12 values:");
    cfgpack_value_t v;
    /* indices are 1-12 (make_schema starts at 1) */
    v.type = CFGPACK_TYPE_U8; v.v.u64 = 1; CHECK(cfgpack_set(&ctx, 1, &v) == CFGPACK_OK);
    LOG("  [1] u8 = 1");
    v.type = CFGPACK_TYPE_U16; v.v.u64 = 2; CHECK(cfgpack_set(&ctx, 2, &v) == CFGPACK_OK);
    LOG("  [2] u16 = 2");
    v.type = CFGPACK_TYPE_U32; v.v.u64 = 3; CHECK(cfgpack_set(&ctx, 3, &v) == CFGPACK_OK);
    LOG("  [3] u32 = 3");
    v.type = CFGPACK_TYPE_U64; v.v.u64 = 4; CHECK(cfgpack_set(&ctx, 4, &v) == CFGPACK_OK);
    LOG("  [4] u64 = 4");
    v.type = CFGPACK_TYPE_I8; v.v.i64 = -1; CHECK(cfgpack_set(&ctx, 5, &v) == CFGPACK_OK);
    LOG("  [5] i8 = -1");
    v.type = CFGPACK_TYPE_I16; v.v.i64 = -2; CHECK(cfgpack_set(&ctx, 6, &v) == CFGPACK_OK);
    LOG("  [6] i16 = -2");
    v.type = CFGPACK_TYPE_I32; v.v.i64 = -3; CHECK(cfgpack_set(&ctx, 7, &v) == CFGPACK_OK);
    LOG("  [7] i32 = -3");
    v.type = CFGPACK_TYPE_I64; v.v.i64 = -4; CHECK(cfgpack_set(&ctx, 8, &v) == CFGPACK_OK);
    LOG("  [8] i64 = -4");
    v.type = CFGPACK_TYPE_F32; v.v.f32 = 1.25f; CHECK(cfgpack_set(&ctx, 9, &v) == CFGPACK_OK);
    LOG("  [9] f32 = 1.25");
    v.type = CFGPACK_TYPE_F64; v.v.f64 = 2.5; CHECK(cfgpack_set(&ctx, 10, &v) == CFGPACK_OK);
    LOG("  [10] f64 = 2.5");
    v.type = CFGPACK_TYPE_STR; v.v.str.len = 3; memcpy(v.v.str.data, "foo", 3); v.v.str.data[3] = '\0'; CHECK(cfgpack_set(&ctx, 11, &v) == CFGPACK_OK);
    LOG("  [11] str = \"foo\"");
    v.type = CFGPACK_TYPE_FSTR; v.v.fstr.len = 3; memcpy(v.v.fstr.data, "bar", 3); v.v.fstr.data[3] = '\0'; CHECK(cfgpack_set(&ctx, 12, &v) == CFGPACK_OK);
    LOG("  [12] fstr = \"bar\"");

    LOG("Pageout to file: %s", path);
    CHECK(cfgpack_pageout_file(&ctx, path, scratch, sizeof(scratch)) == CFGPACK_OK);
    LOG("Pageout succeeded");

    LOG("Clearing state and reading back from file");
    memset(values, 0, sizeof(values));
    memset(present, 0, sizeof(present));

    CHECK(cfgpack_pagein_file(&ctx, path, scratch, sizeof(scratch)) == CFGPACK_OK);
    LOG("Pagein succeeded");

    LOG("Verifying all 12 values:");
    CHECK(cfgpack_get(&ctx, 1, &out) == CFGPACK_OK && out.v.u64 == 1);
    LOG("  [1] u8 = %" PRIu64 " (ok)", out.v.u64);
    CHECK(cfgpack_get(&ctx, 2, &out) == CFGPACK_OK && out.v.u64 == 2);
    LOG("  [2] u16 = %" PRIu64 " (ok)", out.v.u64);
    CHECK(cfgpack_get(&ctx, 3, &out) == CFGPACK_OK && out.v.u64 == 3);
    LOG("  [3] u32 = %" PRIu64 " (ok)", out.v.u64);
    CHECK(cfgpack_get(&ctx, 4, &out) == CFGPACK_OK && out.v.u64 == 4);
    LOG("  [4] u64 = %" PRIu64 " (ok)", out.v.u64);
    CHECK(cfgpack_get(&ctx, 5, &out) == CFGPACK_OK && out.v.i64 == -1);
    LOG("  [5] i8 = %" PRId64 " (ok)", out.v.i64);
    CHECK(cfgpack_get(&ctx, 6, &out) == CFGPACK_OK && out.v.i64 == -2);
    LOG("  [6] i16 = %" PRId64 " (ok)", out.v.i64);
    CHECK(cfgpack_get(&ctx, 7, &out) == CFGPACK_OK && out.v.i64 == -3);
    LOG("  [7] i32 = %" PRId64 " (ok)", out.v.i64);
    CHECK(cfgpack_get(&ctx, 8, &out) == CFGPACK_OK && out.v.i64 == -4);
    LOG("  [8] i64 = %" PRId64 " (ok)", out.v.i64);
    CHECK(cfgpack_get(&ctx, 9, &out) == CFGPACK_OK && fabsf(out.v.f32 - 1.25f) < 1e-6f);
    LOG("  [9] f32 = %f (ok)", (double)out.v.f32);
    CHECK(cfgpack_get(&ctx, 10, &out) == CFGPACK_OK && fabs(out.v.f64 - 2.5) < 1e-9);
    LOG("  [10] f64 = %f (ok)", out.v.f64);
    CHECK(cfgpack_get(&ctx, 11, &out) == CFGPACK_OK && out.v.str.len == 3 && strncmp(out.v.str.data, "foo", 3) == 0);
    LOG("  [11] str = \"%s\" (ok)", out.v.str.data);
    CHECK(cfgpack_get(&ctx, 12, &out) == CFGPACK_OK && out.v.fstr.len == 3 && strncmp(out.v.fstr.data, "bar", 3) == 0);
    LOG("  [12] fstr = \"%s\" (ok)", out.v.fstr.data);

    LOG("Test completed successfully");
    return (TEST_OK);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Remapping Feature Tests
 * ═══════════════════════════════════════════════════════════════════════════ */

TEST_CASE(test_peek_name) {
    LOG_SECTION("Peek schema name from blob");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    cfgpack_value_t defaults[1];
    uint8_t present[1];
    uint8_t buf[64];
    size_t len = 0;
    char name[32];

    LOG("Creating schema with name='myschema'");
    make_schema(&schema, entries, 1);
    snprintf(schema.map_name, sizeof(schema.map_name), "myschema");

    LOG("Initializing context");
    CHECK(cfgpack_init(&ctx, &schema, values, 1, defaults, present, sizeof(present)) == CFGPACK_OK);
    LOG("Context initialized successfully");

    LOG("Pageout to buffer");
    CHECK(cfgpack_pageout(&ctx, buf, sizeof(buf), &len) == CFGPACK_OK);
    LOG("Pageout succeeded, len = %zu", len);
    LOG_HEX("Output buffer", buf, len);

    LOG("Peeking schema name from buffer");
    CHECK(cfgpack_peek_name(buf, len, name, sizeof(name)) == CFGPACK_OK);
    LOG("Peeked name: '%s'", name);
    CHECK(strcmp(name, "myschema") == 0);
    LOG("Name matches expected 'myschema'");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_peek_name_missing) {
    LOG_SECTION("Peek name from blob without schema name (key 0)");

    uint8_t buf[8];
    cfgpack_buf_t b;
    char name[32];

    LOG("Crafting blob without key 0 (backward compat scenario)");
    cfgpack_buf_init(&b, buf, sizeof(buf));
    cfgpack_msgpack_encode_map_header(&b, 1);
    cfgpack_msgpack_encode_uint_key(&b, 5); /* key != 0 */
    cfgpack_msgpack_encode_uint64(&b, 42);
    LOG_HEX("Crafted buffer", buf, b.len);

    LOG("Peek name should return MISSING");
    CHECK(cfgpack_peek_name(buf, b.len, name, sizeof(name)) == CFGPACK_ERR_MISSING);
    LOG("Correctly returned: CFGPACK_ERR_MISSING");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_peek_name_bounds) {
    LOG_SECTION("Peek name with undersized output buffer");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    cfgpack_value_t defaults[1];
    uint8_t present[1];
    uint8_t buf[64];
    size_t len = 0;
    char small[4]; /* too small for "myschema" + null */

    LOG("Creating schema with name='myschema' (8 chars)");
    make_schema(&schema, entries, 1);
    snprintf(schema.map_name, sizeof(schema.map_name), "myschema");

    LOG("Initializing context");
    CHECK(cfgpack_init(&ctx, &schema, values, 1, defaults, present, sizeof(present)) == CFGPACK_OK);
    LOG("Context initialized successfully");

    LOG("Pageout to buffer");
    CHECK(cfgpack_pageout(&ctx, buf, sizeof(buf), &len) == CFGPACK_OK);
    LOG("Pageout succeeded, len = %zu", len);

    LOG("Peeking with 4-byte buffer (too small for 'myschema' + null)");
    CHECK(cfgpack_peek_name(buf, len, small, sizeof(small)) == CFGPACK_ERR_BOUNDS);
    LOG("Correctly returned: CFGPACK_ERR_BOUNDS");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_remap_basic) {
    LOG_SECTION("Basic index remapping (10 -> 20)");

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

    LOG("Setting up old schema: 'val' at index 10 (u8)");
    old_schema.map_name[0] = '\0';
    snprintf(old_schema.map_name, sizeof(old_schema.map_name), "old");
    old_schema.version = 1;
    old_schema.entry_count = 1;
    old_schema.entries = old_entries;
    old_entries[0].index = 10;
    snprintf(old_entries[0].name, sizeof(old_entries[0].name), "val");
    old_entries[0].type = CFGPACK_TYPE_U8;
    old_entries[0].has_default = 0;

    LOG("Setting up new schema: 'val' at index 20 (u8)");
    new_schema.map_name[0] = '\0';
    snprintf(new_schema.map_name, sizeof(new_schema.map_name), "new");
    new_schema.version = 2;
    new_schema.entry_count = 1;
    new_schema.entries = new_entries;
    new_entries[0].index = 20;
    snprintf(new_entries[0].name, sizeof(new_entries[0].name), "val");
    new_entries[0].type = CFGPACK_TYPE_U8;
    new_entries[0].has_default = 0;

    LOG("Initializing both contexts");
    CHECK(cfgpack_init(&old_ctx, &old_schema, old_values, 1, old_defaults, old_present, sizeof(old_present)) == CFGPACK_OK);
    CHECK(cfgpack_init(&new_ctx, &new_schema, new_values, 1, new_defaults, new_present, sizeof(new_present)) == CFGPACK_OK);
    LOG("Both contexts initialized");

    LOG("Setting value in old context: index 10 = 42");
    v.type = CFGPACK_TYPE_U8; v.v.u64 = 42;
    CHECK(cfgpack_set(&old_ctx, 10, &v) == CFGPACK_OK);
    LOG("Value set successfully");

    LOG("Pageout from old context");
    CHECK(cfgpack_pageout(&old_ctx, buf, sizeof(buf), &len) == CFGPACK_OK);
    LOG("Pageout succeeded, len = %zu", len);
    LOG_HEX("Old schema buffer", buf, len);

    LOG("Pagein to new context with remap: 10 -> 20");
    cfgpack_remap_entry_t remap[] = { {10, 20} };
    CHECK(cfgpack_pagein_remap(&new_ctx, buf, len, remap, 1) == CFGPACK_OK);
    LOG("Pagein with remap succeeded");

    LOG("Verifying value at new index 20");
    CHECK(cfgpack_get(&new_ctx, 20, &v) == CFGPACK_OK);
    CHECK(v.v.u64 == 42);
    LOG("Value at index 20: u8 = %" PRIu64 " (correct)", v.v.u64);

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_remap_type_widening) {
    LOG_SECTION("Type widening (u8 -> u16)");

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

    LOG("Setting up old schema: 'val' at index 1 (u8)");
    old_schema.map_name[0] = '\0';
    snprintf(old_schema.map_name, sizeof(old_schema.map_name), "old");
    old_schema.version = 1;
    old_schema.entry_count = 1;
    old_schema.entries = old_entries;
    old_entries[0].index = 1;
    snprintf(old_entries[0].name, sizeof(old_entries[0].name), "val");
    old_entries[0].type = CFGPACK_TYPE_U8;
    old_entries[0].has_default = 0;

    LOG("Setting up new schema: 'val' at index 1 (u16)");
    new_schema.map_name[0] = '\0';
    snprintf(new_schema.map_name, sizeof(new_schema.map_name), "new");
    new_schema.version = 2;
    new_schema.entry_count = 1;
    new_schema.entries = new_entries;
    new_entries[0].index = 1;
    snprintf(new_entries[0].name, sizeof(new_entries[0].name), "val");
    new_entries[0].type = CFGPACK_TYPE_U16;
    new_entries[0].has_default = 0;

    LOG("Initializing both contexts");
    CHECK(cfgpack_init(&old_ctx, &old_schema, old_values, 1, old_defaults, old_present, sizeof(old_present)) == CFGPACK_OK);
    CHECK(cfgpack_init(&new_ctx, &new_schema, new_values, 1, new_defaults, new_present, sizeof(new_present)) == CFGPACK_OK);
    LOG("Both contexts initialized");

    LOG("Setting u8 value in old context: index 1 = 200");
    v.type = CFGPACK_TYPE_U8; v.v.u64 = 200;
    CHECK(cfgpack_set(&old_ctx, 1, &v) == CFGPACK_OK);
    LOG("Value set successfully");

    LOG("Pageout from old context");
    CHECK(cfgpack_pageout(&old_ctx, buf, sizeof(buf), &len) == CFGPACK_OK);
    LOG("Pageout succeeded, len = %zu", len);

    LOG("Pagein to new context (widening u8 -> u16 should succeed)");
    CHECK(cfgpack_pagein_remap(&new_ctx, buf, len, NULL, 0) == CFGPACK_OK);
    LOG("Pagein succeeded");

    LOG("Verifying value at index 1");
    CHECK(cfgpack_get(&new_ctx, 1, &v) == CFGPACK_OK);
    CHECK(v.v.u64 == 200);
    LOG("Value at index 1: u16 = %" PRIu64 " (correct)", v.v.u64);

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_remap_type_narrowing_rejected) {
    LOG_SECTION("Type narrowing rejected (u16 -> u8)");

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

    LOG("Setting up old schema: 'val' at index 1 (u16)");
    old_schema.map_name[0] = '\0';
    snprintf(old_schema.map_name, sizeof(old_schema.map_name), "old");
    old_schema.version = 1;
    old_schema.entry_count = 1;
    old_schema.entries = old_entries;
    old_entries[0].index = 1;
    snprintf(old_entries[0].name, sizeof(old_entries[0].name), "val");
    old_entries[0].type = CFGPACK_TYPE_U16;
    old_entries[0].has_default = 0;

    LOG("Setting up new schema: 'val' at index 1 (u8)");
    new_schema.map_name[0] = '\0';
    snprintf(new_schema.map_name, sizeof(new_schema.map_name), "new");
    new_schema.version = 2;
    new_schema.entry_count = 1;
    new_schema.entries = new_entries;
    new_entries[0].index = 1;
    snprintf(new_entries[0].name, sizeof(new_entries[0].name), "val");
    new_entries[0].type = CFGPACK_TYPE_U8;
    new_entries[0].has_default = 0;

    LOG("Initializing both contexts");
    CHECK(cfgpack_init(&old_ctx, &old_schema, old_values, 1, old_defaults, old_present, sizeof(old_present)) == CFGPACK_OK);
    CHECK(cfgpack_init(&new_ctx, &new_schema, new_values, 1, new_defaults, new_present, sizeof(new_present)) == CFGPACK_OK);
    LOG("Both contexts initialized");

    LOG("Setting u16 value in old context: index 1 = 1000");
    v.type = CFGPACK_TYPE_U16; v.v.u64 = 1000;
    CHECK(cfgpack_set(&old_ctx, 1, &v) == CFGPACK_OK);
    LOG("Value set successfully");

    LOG("Pageout from old context");
    CHECK(cfgpack_pageout(&old_ctx, buf, sizeof(buf), &len) == CFGPACK_OK);
    LOG("Pageout succeeded, len = %zu", len);

    LOG("Pagein to new context (narrowing u16 -> u8 should fail)");
    CHECK(cfgpack_pagein_remap(&new_ctx, buf, len, NULL, 0) == CFGPACK_ERR_TYPE_MISMATCH);
    LOG("Correctly rejected: CFGPACK_ERR_TYPE_MISMATCH");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_remap_reserved_index_skipped) {
    LOG_SECTION("Reserved index 0 (schema name) skipped during pagein");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    cfgpack_value_t defaults[1];
    uint8_t present[1];
    uint8_t buf[64];
    size_t len = 0;

    LOG("Creating schema with entry at index 1");
    make_schema(&schema, entries, 1);
    entries[0].index = 1; /* Use index 1 instead */
    snprintf(schema.map_name, sizeof(schema.map_name), "test");

    LOG("Initializing context");
    CHECK(cfgpack_init(&ctx, &schema, values, 1, defaults, present, sizeof(present)) == CFGPACK_OK);
    LOG("Context initialized successfully");

    LOG("Crafting blob with key 0 = schema name, key 1 = 99");
    cfgpack_buf_t b;
    cfgpack_buf_init(&b, buf, sizeof(buf));
    cfgpack_msgpack_encode_map_header(&b, 2);
    cfgpack_msgpack_encode_uint_key(&b, 0);
    cfgpack_msgpack_encode_str(&b, "test", 4);
    cfgpack_msgpack_encode_uint_key(&b, 1);
    cfgpack_msgpack_encode_uint64(&b, 99);
    len = b.len;
    LOG_HEX("Crafted buffer", buf, len);

    LOG("Pagein should succeed, skipping key 0");
    CHECK(cfgpack_pagein_buf(&ctx, buf, len) == CFGPACK_OK);
    LOG("Pagein succeeded");

    LOG("Verifying key 1 was loaded");
    cfgpack_value_t v;
    CHECK(cfgpack_get(&ctx, 1, &v) == CFGPACK_OK);
    CHECK(v.v.u64 == 99);
    LOG("Value at index 1: u8 = %" PRIu64 " (correct)", v.v.u64);

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_roundtrip_with_name) {
    LOG_SECTION("Full roundtrip with schema name verification");

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

    LOG("Creating schema: name='config_v1', 2 entries at indices 1,2");
    make_schema(&schema, entries, 2);
    entries[0].index = 1;
    entries[1].index = 2;
    snprintf(schema.map_name, sizeof(schema.map_name), "config_v1");

    LOG("Initializing context");
    CHECK(cfgpack_init(&ctx, &schema, values, 2, defaults, present, sizeof(present)) == CFGPACK_OK);
    LOG("Context initialized successfully");

    LOG("Setting values: index 1 = 10, index 2 = 20");
    v.type = CFGPACK_TYPE_U8; v.v.u64 = 10;
    CHECK(cfgpack_set(&ctx, 1, &v) == CFGPACK_OK);
    v.type = CFGPACK_TYPE_U8; v.v.u64 = 20;
    CHECK(cfgpack_set(&ctx, 2, &v) == CFGPACK_OK);
    LOG("Values set successfully");

    LOG("Pageout to buffer");
    CHECK(cfgpack_pageout(&ctx, buf, sizeof(buf), &len) == CFGPACK_OK);
    LOG("Pageout succeeded, len = %zu", len);
    LOG_HEX("Output buffer", buf, len);

    LOG("Peeking schema name from buffer");
    CHECK(cfgpack_peek_name(buf, len, name, sizeof(name)) == CFGPACK_OK);
    CHECK(strcmp(name, "config_v1") == 0);
    LOG("Peeked name: '%s' (correct)", name);

    LOG("Clearing state and reading back");
    memset(values, 0, sizeof(values));
    memset(present, 0, sizeof(present));

    LOG("Pagein from buffer");
    CHECK(cfgpack_pagein_buf(&ctx, buf, len) == CFGPACK_OK);
    LOG("Pagein succeeded");

    LOG("Verifying values");
    CHECK(cfgpack_get(&ctx, 1, &v) == CFGPACK_OK && v.v.u64 == 10);
    LOG("  [1] u8 = %" PRIu64 " (ok)", v.v.u64);
    CHECK(cfgpack_get(&ctx, 2, &v) == CFGPACK_OK && v.v.u64 == 20);
    LOG("  [2] u8 = %" PRIu64 " (ok)", v.v.u64);

    LOG("Test completed successfully");
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
