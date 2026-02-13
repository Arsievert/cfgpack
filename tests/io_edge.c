/* File I/O error paths and advanced remap/coercion tests for cfgpack. */

#include "cfgpack/cfgpack.h"
#include "cfgpack/io_file.h"
#include "cfgpack/msgpack.h"
#include "test.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static void make_schema(cfgpack_schema_t *schema,
                        cfgpack_entry_t *entries,
                        size_t n) {
    snprintf(schema->map_name, sizeof(schema->map_name), "test");
    schema->version = 1;
    schema->entry_count = n;
    schema->entries = entries;
    for (size_t i = 0; i < n; ++i) {
        entries[i].index = (uint16_t)(i + 1);
        snprintf(entries[i].name, sizeof(entries[i].name), "e%zu", i);
        entries[i].type = CFGPACK_TYPE_U8;
        entries[i].has_default = 0;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 1. cfgpack_pagein_file with nonexistent path -> ERR_IO
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_pagein_file_nonexistent) {
    LOG_SECTION("pagein_file with nonexistent path");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    uint8_t scratch[64];
    char str_pool[1];
    uint16_t str_offsets[1];

    make_schema(&schema, entries, 1);
    CHECK(cfgpack_init(&ctx, &schema, values, 1, str_pool, sizeof(str_pool),
                       str_offsets, 0) == CFGPACK_OK);
    LOG("Context initialized");

    LOG("Testing pagein_file with nonexistent path");
    CHECK(cfgpack_pagein_file(&ctx, "/tmp/cfgpack_does_not_exist.bin",
                              scratch, sizeof(scratch)) == CFGPACK_ERR_IO);
    LOG("Correctly returned: CFGPACK_ERR_IO");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 2. cfgpack_parse_schema_file with nonexistent path -> ERR_IO
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_parse_schema_file_nonexistent) {
    LOG_SECTION("parse_schema_file with nonexistent path");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t values[8];
    char str_pool[128];
    uint16_t str_offsets[4];
    char scratch[256];
    cfgpack_parse_error_t err;

    LOG("Calling cfgpack_parse_schema_file with bad path");
    CHECK(cfgpack_parse_schema_file("/tmp/cfgpack_nope.map", &schema, entries,
                                    8, values, str_pool, sizeof(str_pool),
                                    str_offsets, 4, scratch, sizeof(scratch),
                                    &err) == CFGPACK_ERR_IO);
    LOG("Correctly returned: CFGPACK_ERR_IO");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 3. cfgpack_schema_parse_json_file with nonexistent path -> ERR_IO
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_json_parse_file_nonexistent) {
    LOG_SECTION("schema_parse_json_file with nonexistent path");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t values[8];
    char str_pool[128];
    uint16_t str_offsets[4];
    char scratch[256];
    cfgpack_parse_error_t err;

    LOG("Calling cfgpack_schema_parse_json_file with bad path");
    CHECK(cfgpack_schema_parse_json_file("/tmp/cfgpack_nope.json", &schema,
                                         entries, 8, values, str_pool,
                                         sizeof(str_pool), str_offsets, 4,
                                         scratch, sizeof(scratch),
                                         &err) == CFGPACK_ERR_IO);
    LOG("Correctly returned: CFGPACK_ERR_IO");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 4. cfgpack_schema_write_json_file with invalid path -> ERR_IO
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_json_write_file_bad_path) {
    LOG_SECTION("schema_write_json_file with invalid path");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    char str_pool[1];
    uint16_t str_offsets[1];
    char scratch[512];
    cfgpack_parse_error_t err;

    make_schema(&schema, entries, 1);
    CHECK(cfgpack_init(&ctx, &schema, values, 1, str_pool, sizeof(str_pool),
                       str_offsets, 0) == CFGPACK_OK);
    LOG("Context initialized");

    LOG("Attempting write_json_file to nonexistent directory");
    CHECK(cfgpack_schema_write_json_file(&ctx,
                                         "/tmp/cfgpack_no_such_dir/out.json",
                                         scratch, sizeof(scratch),
                                         &err) == CFGPACK_ERR_IO);
    LOG("Correctly returned: CFGPACK_ERR_IO");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 5. Remap with string types (str and fstr)
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_remap_string_types) {
    LOG_SECTION("Remap with str and fstr entries (old -> new index)");

    /* Old schema: str at 10, fstr at 11
     * New schema: str at 20, fstr at 21
     * Remap: 10->20, 11->21
     */
    cfgpack_schema_t old_schema, new_schema;
    cfgpack_entry_t old_entries[2], new_entries[2];
    cfgpack_ctx_t old_ctx, new_ctx;
    cfgpack_value_t old_values[2], new_values[2];
    uint8_t buf[128];
    size_t len = 0;
    char old_str_pool[256], new_str_pool[256];
    uint16_t old_str_offsets[2], new_str_offsets[2];

    LOG("Setting up old schema: str@10, fstr@11");
    snprintf(old_schema.map_name, sizeof(old_schema.map_name), "old");
    old_schema.version = 1;
    old_schema.entry_count = 2;
    old_schema.entries = old_entries;
    old_entries[0].index = 10;
    snprintf(old_entries[0].name, sizeof(old_entries[0].name), "s");
    old_entries[0].type = CFGPACK_TYPE_STR;
    old_entries[0].has_default = 0;
    old_entries[1].index = 11;
    snprintf(old_entries[1].name, sizeof(old_entries[1].name), "fs");
    old_entries[1].type = CFGPACK_TYPE_FSTR;
    old_entries[1].has_default = 0;

    LOG("Setting up new schema: str@20, fstr@21");
    snprintf(new_schema.map_name, sizeof(new_schema.map_name), "new");
    new_schema.version = 2;
    new_schema.entry_count = 2;
    new_schema.entries = new_entries;
    new_entries[0].index = 20;
    snprintf(new_entries[0].name, sizeof(new_entries[0].name), "s");
    new_entries[0].type = CFGPACK_TYPE_STR;
    new_entries[0].has_default = 0;
    new_entries[1].index = 21;
    snprintf(new_entries[1].name, sizeof(new_entries[1].name), "fs");
    new_entries[1].type = CFGPACK_TYPE_FSTR;
    new_entries[1].has_default = 0;

    CHECK(cfgpack_init(&old_ctx, &old_schema, old_values, 2, old_str_pool,
                       sizeof(old_str_pool), old_str_offsets, 2) == CFGPACK_OK);
    CHECK(cfgpack_init(&new_ctx, &new_schema, new_values, 2, new_str_pool,
                       sizeof(new_str_pool), new_str_offsets, 2) == CFGPACK_OK);
    LOG("Both contexts initialized");

    CHECK(cfgpack_set_str(&old_ctx, 10, "hello") == CFGPACK_OK);
    CHECK(cfgpack_set_fstr(&old_ctx, 11, "world") == CFGPACK_OK);
    LOG("Set old values: str='hello', fstr='world'");

    CHECK(cfgpack_pageout(&old_ctx, buf, sizeof(buf), &len) == CFGPACK_OK);
    LOG("Pageout succeeded, len=%zu", len);

    cfgpack_remap_entry_t remap[] = {{10, 20}, {11, 21}};
    CHECK(cfgpack_pagein_remap(&new_ctx, buf, len, remap, 2) == CFGPACK_OK);
    LOG("Pagein with remap succeeded");

    const char *str_out;
    uint16_t str_len;
    CHECK(cfgpack_get_str(&new_ctx, 20, &str_out, &str_len) == CFGPACK_OK);
    CHECK(str_len == 5);
    CHECK(strncmp(str_out, "hello", 5) == 0);
    LOG("str@20 = '%s' (correct)", str_out);

    const char *fstr_out;
    uint8_t fstr_len;
    CHECK(cfgpack_get_fstr(&new_ctx, 21, &fstr_out, &fstr_len) == CFGPACK_OK);
    CHECK(fstr_len == 5);
    CHECK(strncmp(fstr_out, "world", 5) == 0);
    LOG("fstr@21 = '%s' (correct)", fstr_out);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 6. Remap with 3+ entries simultaneously
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_remap_multiple_entries) {
    LOG_SECTION("Remap table with 3+ remapped entries simultaneously");

    /* Old schema: u8@10, u16@11, u32@12
     * New schema: u8@20, u16@21, u32@22
     * Remap: 10->20, 11->21, 12->22
     */
    cfgpack_schema_t old_schema, new_schema;
    cfgpack_entry_t old_entries[3], new_entries[3];
    cfgpack_ctx_t old_ctx, new_ctx;
    cfgpack_value_t old_values[3], new_values[3];
    uint8_t buf[128];
    size_t len = 0;
    char old_str_pool[1], new_str_pool[1];
    uint16_t old_str_offsets[1], new_str_offsets[1];
    cfgpack_value_t v;

    LOG("Setting up old schema: u8@10, u16@11, u32@12");
    snprintf(old_schema.map_name, sizeof(old_schema.map_name), "old");
    old_schema.version = 1;
    old_schema.entry_count = 3;
    old_schema.entries = old_entries;
    old_entries[0] = (cfgpack_entry_t){.index = 10, .type = CFGPACK_TYPE_U8, .has_default = 0};
    snprintf(old_entries[0].name, sizeof(old_entries[0].name), "a");
    old_entries[1] = (cfgpack_entry_t){.index = 11, .type = CFGPACK_TYPE_U16, .has_default = 0};
    snprintf(old_entries[1].name, sizeof(old_entries[1].name), "b");
    old_entries[2] = (cfgpack_entry_t){.index = 12, .type = CFGPACK_TYPE_U32, .has_default = 0};
    snprintf(old_entries[2].name, sizeof(old_entries[2].name), "c");

    LOG("Setting up new schema: u8@20, u16@21, u32@22");
    snprintf(new_schema.map_name, sizeof(new_schema.map_name), "new");
    new_schema.version = 2;
    new_schema.entry_count = 3;
    new_schema.entries = new_entries;
    new_entries[0] = (cfgpack_entry_t){.index = 20, .type = CFGPACK_TYPE_U8, .has_default = 0};
    snprintf(new_entries[0].name, sizeof(new_entries[0].name), "a");
    new_entries[1] = (cfgpack_entry_t){.index = 21, .type = CFGPACK_TYPE_U16, .has_default = 0};
    snprintf(new_entries[1].name, sizeof(new_entries[1].name), "b");
    new_entries[2] = (cfgpack_entry_t){.index = 22, .type = CFGPACK_TYPE_U32, .has_default = 0};
    snprintf(new_entries[2].name, sizeof(new_entries[2].name), "c");

    CHECK(cfgpack_init(&old_ctx, &old_schema, old_values, 3, old_str_pool,
                       sizeof(old_str_pool), old_str_offsets, 0) == CFGPACK_OK);
    CHECK(cfgpack_init(&new_ctx, &new_schema, new_values, 3, new_str_pool,
                       sizeof(new_str_pool), new_str_offsets, 0) == CFGPACK_OK);

    CHECK(cfgpack_set_u8(&old_ctx, 10, 42) == CFGPACK_OK);
    CHECK(cfgpack_set_u16(&old_ctx, 11, 1000) == CFGPACK_OK);
    CHECK(cfgpack_set_u32(&old_ctx, 12, 100000) == CFGPACK_OK);
    LOG("Set old values: u8=42, u16=1000, u32=100000");

    CHECK(cfgpack_pageout(&old_ctx, buf, sizeof(buf), &len) == CFGPACK_OK);
    LOG("Pageout succeeded, len=%zu", len);

    cfgpack_remap_entry_t remap[] = {{10, 20}, {11, 21}, {12, 22}};
    CHECK(cfgpack_pagein_remap(&new_ctx, buf, len, remap, 3) == CFGPACK_OK);
    LOG("Pagein with remap succeeded");

    CHECK(cfgpack_get(&new_ctx, 20, &v) == CFGPACK_OK && v.v.u64 == 42);
    LOG("u8@20 = %" PRIu64 " (correct)", v.v.u64);
    CHECK(cfgpack_get(&new_ctx, 21, &v) == CFGPACK_OK && v.v.u64 == 1000);
    LOG("u16@21 = %" PRIu64 " (correct)", v.v.u64);
    CHECK(cfgpack_get(&new_ctx, 22, &v) == CFGPACK_OK && v.v.u64 == 100000);
    LOG("u32@22 = %" PRIu64 " (correct)", v.v.u64);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 7. Key in payload not in remap table is used as-is
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_remap_unmapped_key) {
    LOG_SECTION("Unmapped key passes through remap unchanged");

    /* Old schema: u8@1, u8@2
     * New schema: u8@1, u8@2
     * Remap: only 1->1 (explicit), key 2 is not in remap -> used as-is
     */
    cfgpack_schema_t old_schema, new_schema;
    cfgpack_entry_t old_entries[2], new_entries[2];
    cfgpack_ctx_t old_ctx, new_ctx;
    cfgpack_value_t old_values[2], new_values[2];
    uint8_t buf[64];
    size_t len = 0;
    char old_str_pool[1], new_str_pool[1];
    uint16_t old_str_offsets[1], new_str_offsets[1];
    cfgpack_value_t v;

    make_schema(&old_schema, old_entries, 2);
    snprintf(old_schema.map_name, sizeof(old_schema.map_name), "old");

    make_schema(&new_schema, new_entries, 2);
    snprintf(new_schema.map_name, sizeof(new_schema.map_name), "new");
    new_schema.version = 2;

    CHECK(cfgpack_init(&old_ctx, &old_schema, old_values, 2, old_str_pool,
                       sizeof(old_str_pool), old_str_offsets, 0) == CFGPACK_OK);
    CHECK(cfgpack_init(&new_ctx, &new_schema, new_values, 2, new_str_pool,
                       sizeof(new_str_pool), new_str_offsets, 0) == CFGPACK_OK);

    CHECK(cfgpack_set_u8(&old_ctx, 1, 10) == CFGPACK_OK);
    CHECK(cfgpack_set_u8(&old_ctx, 2, 20) == CFGPACK_OK);
    LOG("Set old values: 1=10, 2=20");

    CHECK(cfgpack_pageout(&old_ctx, buf, sizeof(buf), &len) == CFGPACK_OK);

    /* Only remap key 1->1 (identity); key 2 is NOT in remap table */
    cfgpack_remap_entry_t remap[] = {{1, 1}};
    CHECK(cfgpack_pagein_remap(&new_ctx, buf, len, remap, 1) == CFGPACK_OK);
    LOG("Pagein with partial remap succeeded");

    CHECK(cfgpack_get(&new_ctx, 1, &v) == CFGPACK_OK && v.v.u64 == 10);
    LOG("Key 1 = %" PRIu64 " (remapped, correct)", v.v.u64);
    CHECK(cfgpack_get(&new_ctx, 2, &v) == CFGPACK_OK && v.v.u64 == 20);
    LOG("Key 2 = %" PRIu64 " (unmapped pass-through, correct)", v.v.u64);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 8. Signed widening coercion: i8->i16, i16->i32, i32->i64
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_coerce_signed_widening) {
    LOG_SECTION("Signed widening coercion: i8->i16, i16->i32, i32->i64");

    /* Test i8 -> i16 */
    {
        cfgpack_schema_t old_s, new_s;
        cfgpack_entry_t old_e[1], new_e[1];
        cfgpack_ctx_t old_c, new_c;
        cfgpack_value_t old_v[1], new_v[1];
        uint8_t buf[64];
        size_t len = 0;
        char osp[1], nsp[1];
        uint16_t oso[1], nso[1];
        cfgpack_value_t v;

        make_schema(&old_s, old_e, 1);
        old_e[0].type = CFGPACK_TYPE_I8;
        make_schema(&new_s, new_e, 1);
        new_e[0].type = CFGPACK_TYPE_I16;

        CHECK(cfgpack_init(&old_c, &old_s, old_v, 1, osp, 1, oso, 0) == CFGPACK_OK);
        CHECK(cfgpack_init(&new_c, &new_s, new_v, 1, nsp, 1, nso, 0) == CFGPACK_OK);

        CHECK(cfgpack_set_i8(&old_c, 1, -42) == CFGPACK_OK);
        CHECK(cfgpack_pageout(&old_c, buf, sizeof(buf), &len) == CFGPACK_OK);
        CHECK(cfgpack_pagein_remap(&new_c, buf, len, NULL, 0) == CFGPACK_OK);
        CHECK(cfgpack_get(&new_c, 1, &v) == CFGPACK_OK);
        CHECK(v.v.i64 == -42);
        LOG("i8(-42) -> i16: %" PRId64 " (correct)", v.v.i64);
    }

    /* Test i16 -> i32 */
    {
        cfgpack_schema_t old_s, new_s;
        cfgpack_entry_t old_e[1], new_e[1];
        cfgpack_ctx_t old_c, new_c;
        cfgpack_value_t old_v[1], new_v[1];
        uint8_t buf[64];
        size_t len = 0;
        char osp[1], nsp[1];
        uint16_t oso[1], nso[1];
        cfgpack_value_t v;

        make_schema(&old_s, old_e, 1);
        old_e[0].type = CFGPACK_TYPE_I16;
        make_schema(&new_s, new_e, 1);
        new_e[0].type = CFGPACK_TYPE_I32;

        CHECK(cfgpack_init(&old_c, &old_s, old_v, 1, osp, 1, oso, 0) == CFGPACK_OK);
        CHECK(cfgpack_init(&new_c, &new_s, new_v, 1, nsp, 1, nso, 0) == CFGPACK_OK);

        CHECK(cfgpack_set_i16(&old_c, 1, -1000) == CFGPACK_OK);
        CHECK(cfgpack_pageout(&old_c, buf, sizeof(buf), &len) == CFGPACK_OK);
        CHECK(cfgpack_pagein_remap(&new_c, buf, len, NULL, 0) == CFGPACK_OK);
        CHECK(cfgpack_get(&new_c, 1, &v) == CFGPACK_OK);
        CHECK(v.v.i64 == -1000);
        LOG("i16(-1000) -> i32: %" PRId64 " (correct)", v.v.i64);
    }

    /* Test i32 -> i64 */
    {
        cfgpack_schema_t old_s, new_s;
        cfgpack_entry_t old_e[1], new_e[1];
        cfgpack_ctx_t old_c, new_c;
        cfgpack_value_t old_v[1], new_v[1];
        uint8_t buf[64];
        size_t len = 0;
        char osp[1], nsp[1];
        uint16_t oso[1], nso[1];
        cfgpack_value_t v;

        make_schema(&old_s, old_e, 1);
        old_e[0].type = CFGPACK_TYPE_I32;
        make_schema(&new_s, new_e, 1);
        new_e[0].type = CFGPACK_TYPE_I64;

        CHECK(cfgpack_init(&old_c, &old_s, old_v, 1, osp, 1, oso, 0) == CFGPACK_OK);
        CHECK(cfgpack_init(&new_c, &new_s, new_v, 1, nsp, 1, nso, 0) == CFGPACK_OK);

        CHECK(cfgpack_set_i32(&old_c, 1, -100000) == CFGPACK_OK);
        CHECK(cfgpack_pageout(&old_c, buf, sizeof(buf), &len) == CFGPACK_OK);
        CHECK(cfgpack_pagein_remap(&new_c, buf, len, NULL, 0) == CFGPACK_OK);
        CHECK(cfgpack_get(&new_c, 1, &v) == CFGPACK_OK);
        CHECK(v.v.i64 == -100000);
        LOG("i32(-100000) -> i64: %" PRId64 " (correct)", v.v.i64);
    }

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 9. Cross-sign widening: u8->i16, u16->i32
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_coerce_unsigned_to_signed) {
    LOG_SECTION("Unsigned-to-signed cross-sign widening");

    /* Test u8 -> i16 */
    {
        cfgpack_schema_t old_s, new_s;
        cfgpack_entry_t old_e[1], new_e[1];
        cfgpack_ctx_t old_c, new_c;
        cfgpack_value_t old_v[1], new_v[1];
        uint8_t buf[64];
        size_t len = 0;
        char osp[1], nsp[1];
        uint16_t oso[1], nso[1];
        cfgpack_value_t v;

        make_schema(&old_s, old_e, 1);
        old_e[0].type = CFGPACK_TYPE_U8;
        make_schema(&new_s, new_e, 1);
        new_e[0].type = CFGPACK_TYPE_I16;

        CHECK(cfgpack_init(&old_c, &old_s, old_v, 1, osp, 1, oso, 0) == CFGPACK_OK);
        CHECK(cfgpack_init(&new_c, &new_s, new_v, 1, nsp, 1, nso, 0) == CFGPACK_OK);

        CHECK(cfgpack_set_u8(&old_c, 1, 200) == CFGPACK_OK);
        CHECK(cfgpack_pageout(&old_c, buf, sizeof(buf), &len) == CFGPACK_OK);
        CHECK(cfgpack_pagein_remap(&new_c, buf, len, NULL, 0) == CFGPACK_OK);
        CHECK(cfgpack_get(&new_c, 1, &v) == CFGPACK_OK);
        CHECK(v.v.i64 == 200);
        LOG("u8(200) -> i16: %" PRId64 " (correct)", v.v.i64);
    }

    /* Test u16 -> i32 */
    {
        cfgpack_schema_t old_s, new_s;
        cfgpack_entry_t old_e[1], new_e[1];
        cfgpack_ctx_t old_c, new_c;
        cfgpack_value_t old_v[1], new_v[1];
        uint8_t buf[64];
        size_t len = 0;
        char osp[1], nsp[1];
        uint16_t oso[1], nso[1];
        cfgpack_value_t v;

        make_schema(&old_s, old_e, 1);
        old_e[0].type = CFGPACK_TYPE_U16;
        make_schema(&new_s, new_e, 1);
        new_e[0].type = CFGPACK_TYPE_I32;

        CHECK(cfgpack_init(&old_c, &old_s, old_v, 1, osp, 1, oso, 0) == CFGPACK_OK);
        CHECK(cfgpack_init(&new_c, &new_s, new_v, 1, nsp, 1, nso, 0) == CFGPACK_OK);

        CHECK(cfgpack_set_u16(&old_c, 1, 50000) == CFGPACK_OK);
        CHECK(cfgpack_pageout(&old_c, buf, sizeof(buf), &len) == CFGPACK_OK);
        CHECK(cfgpack_pagein_remap(&new_c, buf, len, NULL, 0) == CFGPACK_OK);
        CHECK(cfgpack_get(&new_c, 1, &v) == CFGPACK_OK);
        CHECK(v.v.i64 == 50000);
        LOG("u16(50000) -> i32: %" PRId64 " (correct)", v.v.i64);
    }

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 10. f32 -> f64 widening
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_coerce_f32_to_f64) {
    LOG_SECTION("f32 -> f64 widening via pagein_remap");

    cfgpack_schema_t old_s, new_s;
    cfgpack_entry_t old_e[1], new_e[1];
    cfgpack_ctx_t old_c, new_c;
    cfgpack_value_t old_v[1], new_v[1];
    uint8_t buf[64];
    size_t len = 0;
    char osp[1], nsp[1];
    uint16_t oso[1], nso[1];
    cfgpack_value_t v;

    make_schema(&old_s, old_e, 1);
    old_e[0].type = CFGPACK_TYPE_F32;
    make_schema(&new_s, new_e, 1);
    new_e[0].type = CFGPACK_TYPE_F64;

    CHECK(cfgpack_init(&old_c, &old_s, old_v, 1, osp, 1, oso, 0) == CFGPACK_OK);
    CHECK(cfgpack_init(&new_c, &new_s, new_v, 1, nsp, 1, nso, 0) == CFGPACK_OK);

    CHECK(cfgpack_set_f32(&old_c, 1, 3.25f) == CFGPACK_OK);
    LOG("Set old f32 = 3.25");

    CHECK(cfgpack_pageout(&old_c, buf, sizeof(buf), &len) == CFGPACK_OK);
    LOG("Pageout succeeded, len=%zu", len);

    CHECK(cfgpack_pagein_remap(&new_c, buf, len, NULL, 0) == CFGPACK_OK);
    LOG("Pagein with f32->f64 coercion succeeded");

    CHECK(cfgpack_get(&new_c, 1, &v) == CFGPACK_OK);
    CHECK(fabs(v.v.f64 - 3.25) < 1e-6);
    LOG("f64 = %f (correct)", v.v.f64);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 11. fstr -> str widening
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_coerce_fstr_to_str) {
    LOG_SECTION("fstr -> str widening via pagein_remap");

    cfgpack_schema_t old_s, new_s;
    cfgpack_entry_t old_e[1], new_e[1];
    cfgpack_ctx_t old_c, new_c;
    cfgpack_value_t old_v[1], new_v[1];
    uint8_t buf[64];
    size_t len = 0;
    char old_sp[32], new_sp[128];
    uint16_t old_so[1], new_so[1];

    LOG("Old schema: fstr@1, New schema: str@1");
    make_schema(&old_s, old_e, 1);
    old_e[0].type = CFGPACK_TYPE_FSTR;
    make_schema(&new_s, new_e, 1);
    new_e[0].type = CFGPACK_TYPE_STR;

    CHECK(cfgpack_init(&old_c, &old_s, old_v, 1, old_sp, sizeof(old_sp),
                       old_so, 1) == CFGPACK_OK);
    CHECK(cfgpack_init(&new_c, &new_s, new_v, 1, new_sp, sizeof(new_sp),
                       new_so, 1) == CFGPACK_OK);

    CHECK(cfgpack_set_fstr(&old_c, 1, "short") == CFGPACK_OK);
    LOG("Set old fstr = 'short'");

    CHECK(cfgpack_pageout(&old_c, buf, sizeof(buf), &len) == CFGPACK_OK);
    LOG("Pageout succeeded, len=%zu", len);

    CHECK(cfgpack_pagein_remap(&new_c, buf, len, NULL, 0) == CFGPACK_OK);
    LOG("Pagein with fstr->str coercion succeeded");

    const char *str_out;
    uint16_t str_len;
    CHECK(cfgpack_get_str(&new_c, 1, &str_out, &str_len) == CFGPACK_OK);
    CHECK(str_len == 5);
    CHECK(strncmp(str_out, "short", 5) == 0);
    LOG("str@1 = '%s' len=%u (correct)", str_out, str_len);

    return TEST_OK;
}

int main(void) {
    test_result_t overall = TEST_OK;

    overall |= (test_case_result("pagein_file_nonexistent",
                                 test_pagein_file_nonexistent()) != TEST_OK);
    overall |= (test_case_result("parse_schema_file_nonexistent",
                                 test_parse_schema_file_nonexistent()) !=
                TEST_OK);
    overall |= (test_case_result("json_parse_file_nonexistent",
                                 test_json_parse_file_nonexistent()) !=
                TEST_OK);
    overall |= (test_case_result("json_write_file_bad_path",
                                 test_json_write_file_bad_path()) != TEST_OK);
    overall |= (test_case_result("remap_string_types",
                                 test_remap_string_types()) != TEST_OK);
    overall |= (test_case_result("remap_multiple_entries",
                                 test_remap_multiple_entries()) != TEST_OK);
    overall |= (test_case_result("remap_unmapped_key",
                                 test_remap_unmapped_key()) != TEST_OK);
    overall |= (test_case_result("coerce_signed_widening",
                                 test_coerce_signed_widening()) != TEST_OK);
    overall |= (test_case_result("coerce_unsigned_to_signed",
                                 test_coerce_unsigned_to_signed()) != TEST_OK);
    overall |= (test_case_result("coerce_f32_to_f64",
                                 test_coerce_f32_to_f64()) != TEST_OK);
    overall |= (test_case_result("coerce_fstr_to_str",
                                 test_coerce_fstr_to_str()) != TEST_OK);

    if (overall == TEST_OK) {
        printf(COLOR_GREEN "ALL PASS" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "SOME FAIL" COLOR_RESET "\n");
    }
    return (overall);
}
