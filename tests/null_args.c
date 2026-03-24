/* Tests for NULL / bad argument handling (CFGPACK_ERR_ARGS paths). */

#include "cfgpack/cfgpack.h"

#include "test.h"

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

/* Helper: create a fully initialized context with 1 u8, 1 str, 1 fstr */
static cfgpack_err_t make_ctx(cfgpack_ctx_t *ctx,
                              cfgpack_schema_t *schema,
                              cfgpack_entry_t *entries,
                              cfgpack_value_t *values,
                              char *str_pool,
                              size_t str_pool_cap,
                              uint16_t *str_offsets) {
    make_schema(schema, entries, 3);
    entries[0].type = CFGPACK_TYPE_U8;
    entries[1].type = CFGPACK_TYPE_STR;
    entries[2].type = CFGPACK_TYPE_FSTR;
    return (cfgpack_init(ctx, schema, values, 3, str_pool, str_pool_cap,
                         str_offsets, 2));
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 1. cfgpack_init NULL args
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_init_null_ctx) {
    LOG_SECTION("cfgpack_init with NULL ctx");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_value_t values[1];
    char str_pool[1];
    uint16_t str_offsets[1];

    make_schema(&schema, entries, 1);

    CHECK(cfgpack_init(NULL, &schema, values, 1, str_pool, sizeof(str_pool),
                       str_offsets, 0) == CFGPACK_ERR_ARGS);
    LOG("NULL ctx -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

TEST_CASE(test_init_null_schema) {
    LOG_SECTION("cfgpack_init with NULL schema");

    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    char str_pool[1];
    uint16_t str_offsets[1];

    CHECK(cfgpack_init(&ctx, NULL, values, 1, str_pool, sizeof(str_pool),
                       str_offsets, 0) == CFGPACK_ERR_ARGS);
    LOG("NULL schema -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 2. cfgpack_set / cfgpack_get NULL args
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_set_null_ctx) {
    LOG_SECTION("cfgpack_set with NULL ctx");

    cfgpack_value_t v = {.type = CFGPACK_TYPE_U8, .v.u64 = 1};
    CHECK(cfgpack_set(NULL, 1, &v) == CFGPACK_ERR_ARGS);
    LOG("NULL ctx -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

TEST_CASE(test_set_null_value) {
    LOG_SECTION("cfgpack_set with NULL value");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    char str_pool[1];
    uint16_t str_offsets[1];

    make_schema(&schema, entries, 1);
    CHECK(cfgpack_init(&ctx, &schema, values, 1, str_pool, sizeof(str_pool),
                       str_offsets, 0) == CFGPACK_OK);

    CHECK(cfgpack_set(&ctx, 1, NULL) == CFGPACK_ERR_ARGS);
    LOG("NULL value -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

TEST_CASE(test_get_null_ctx) {
    LOG_SECTION("cfgpack_get with NULL ctx");

    cfgpack_value_t v;
    CHECK(cfgpack_get(NULL, 1, &v) == CFGPACK_ERR_ARGS);
    LOG("NULL ctx -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

TEST_CASE(test_get_null_out) {
    LOG_SECTION("cfgpack_get with NULL out_value");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    char str_pool[1];
    uint16_t str_offsets[1];

    make_schema(&schema, entries, 1);
    CHECK(cfgpack_init(&ctx, &schema, values, 1, str_pool, sizeof(str_pool),
                       str_offsets, 0) == CFGPACK_OK);

    CHECK(cfgpack_get(&ctx, 1, NULL) == CFGPACK_ERR_ARGS);
    LOG("NULL out_value -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 3. cfgpack_set_by_name / cfgpack_get_by_name NULL args
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_set_by_name_null_ctx) {
    LOG_SECTION("cfgpack_set_by_name with NULL ctx");

    cfgpack_value_t v = {.type = CFGPACK_TYPE_U8, .v.u64 = 1};
    CHECK(cfgpack_set_by_name(NULL, "e0", &v) == CFGPACK_ERR_ARGS);
    LOG("NULL ctx -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

TEST_CASE(test_set_by_name_null_name) {
    LOG_SECTION("cfgpack_set_by_name with NULL name");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    char str_pool[1];
    uint16_t str_offsets[1];

    make_schema(&schema, entries, 1);
    CHECK(cfgpack_init(&ctx, &schema, values, 1, str_pool, sizeof(str_pool),
                       str_offsets, 0) == CFGPACK_OK);

    cfgpack_value_t v = {.type = CFGPACK_TYPE_U8, .v.u64 = 1};
    CHECK(cfgpack_set_by_name(&ctx, NULL, &v) == CFGPACK_ERR_ARGS);
    LOG("NULL name -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

TEST_CASE(test_get_by_name_null_ctx) {
    LOG_SECTION("cfgpack_get_by_name with NULL ctx");

    cfgpack_value_t v;
    CHECK(cfgpack_get_by_name(NULL, "e0", &v) == CFGPACK_ERR_ARGS);
    LOG("NULL ctx -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

TEST_CASE(test_get_by_name_null_name) {
    LOG_SECTION("cfgpack_get_by_name with NULL name");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    cfgpack_value_t v;
    char str_pool[1];
    uint16_t str_offsets[1];

    make_schema(&schema, entries, 1);
    CHECK(cfgpack_init(&ctx, &schema, values, 1, str_pool, sizeof(str_pool),
                       str_offsets, 0) == CFGPACK_OK);

    CHECK(cfgpack_get_by_name(&ctx, NULL, &v) == CFGPACK_ERR_ARGS);
    LOG("NULL name -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 4. cfgpack_set_str / cfgpack_get_str NULL args
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_set_str_null_ctx) {
    LOG_SECTION("cfgpack_set_str with NULL ctx");

    CHECK(cfgpack_set_str(NULL, 1, "hello") == CFGPACK_ERR_ARGS);
    LOG("NULL ctx -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

TEST_CASE(test_set_str_null_str) {
    LOG_SECTION("cfgpack_set_str with NULL str");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[3];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[3];
    char str_pool[256];
    uint16_t str_offsets[2];

    CHECK(make_ctx(&ctx, &schema, entries, values, str_pool, sizeof(str_pool),
                   str_offsets) == CFGPACK_OK);

    CHECK(cfgpack_set_str(&ctx, 2, NULL) == CFGPACK_ERR_ARGS);
    LOG("NULL str -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

TEST_CASE(test_get_str_null_ctx) {
    LOG_SECTION("cfgpack_get_str with NULL ctx");

    const char *out;
    uint16_t len;
    CHECK(cfgpack_get_str(NULL, 1, &out, &len) == CFGPACK_ERR_ARGS);
    LOG("NULL ctx -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

TEST_CASE(test_get_str_null_out) {
    LOG_SECTION("cfgpack_get_str with NULL out pointer");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[3];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[3];
    char str_pool[256];
    uint16_t str_offsets[2];

    CHECK(make_ctx(&ctx, &schema, entries, values, str_pool, sizeof(str_pool),
                   str_offsets) == CFGPACK_OK);

    uint16_t len;
    CHECK(cfgpack_get_str(&ctx, 2, NULL, &len) == CFGPACK_ERR_ARGS);
    LOG("NULL out -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 5. cfgpack_set_fstr / cfgpack_get_fstr NULL args
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_set_fstr_null_ctx) {
    LOG_SECTION("cfgpack_set_fstr with NULL ctx");

    CHECK(cfgpack_set_fstr(NULL, 1, "hi") == CFGPACK_ERR_ARGS);
    LOG("NULL ctx -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

TEST_CASE(test_set_fstr_null_str) {
    LOG_SECTION("cfgpack_set_fstr with NULL str");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[3];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[3];
    char str_pool[256];
    uint16_t str_offsets[2];

    CHECK(make_ctx(&ctx, &schema, entries, values, str_pool, sizeof(str_pool),
                   str_offsets) == CFGPACK_OK);

    CHECK(cfgpack_set_fstr(&ctx, 3, NULL) == CFGPACK_ERR_ARGS);
    LOG("NULL str -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

TEST_CASE(test_get_fstr_null_ctx) {
    LOG_SECTION("cfgpack_get_fstr with NULL ctx");

    const char *out;
    uint8_t len;
    CHECK(cfgpack_get_fstr(NULL, 1, &out, &len) == CFGPACK_ERR_ARGS);
    LOG("NULL ctx -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

TEST_CASE(test_get_fstr_null_out) {
    LOG_SECTION("cfgpack_get_fstr with NULL out pointer");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[3];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[3];
    char str_pool[256];
    uint16_t str_offsets[2];

    CHECK(make_ctx(&ctx, &schema, entries, values, str_pool, sizeof(str_pool),
                   str_offsets) == CFGPACK_OK);

    uint8_t len;
    CHECK(cfgpack_get_fstr(&ctx, 3, NULL, &len) == CFGPACK_ERR_ARGS);
    LOG("NULL out -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 6. cfgpack_set_str_by_name / cfgpack_set_fstr_by_name NULL args
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_set_str_by_name_null_ctx) {
    LOG_SECTION("cfgpack_set_str_by_name with NULL ctx");

    CHECK(cfgpack_set_str_by_name(NULL, "e1", "hi") == CFGPACK_ERR_ARGS);
    LOG("NULL ctx -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

TEST_CASE(test_set_str_by_name_null_name) {
    LOG_SECTION("cfgpack_set_str_by_name with NULL name");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[3];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[3];
    char str_pool[256];
    uint16_t str_offsets[2];

    CHECK(make_ctx(&ctx, &schema, entries, values, str_pool, sizeof(str_pool),
                   str_offsets) == CFGPACK_OK);

    CHECK(cfgpack_set_str_by_name(&ctx, NULL, "hi") == CFGPACK_ERR_ARGS);
    LOG("NULL name -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

TEST_CASE(test_set_fstr_by_name_null_ctx) {
    LOG_SECTION("cfgpack_set_fstr_by_name with NULL ctx");

    CHECK(cfgpack_set_fstr_by_name(NULL, "e2", "hi") == CFGPACK_ERR_ARGS);
    LOG("NULL ctx -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

TEST_CASE(test_set_fstr_by_name_null_name) {
    LOG_SECTION("cfgpack_set_fstr_by_name with NULL name");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[3];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[3];
    char str_pool[256];
    uint16_t str_offsets[2];

    CHECK(make_ctx(&ctx, &schema, entries, values, str_pool, sizeof(str_pool),
                   str_offsets) == CFGPACK_OK);

    CHECK(cfgpack_set_fstr_by_name(&ctx, NULL, "hi") == CFGPACK_ERR_ARGS);
    LOG("NULL name -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 7. cfgpack_get_str_by_name / cfgpack_get_fstr_by_name NULL args
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_get_str_by_name_null_ctx) {
    LOG_SECTION("cfgpack_get_str_by_name with NULL ctx");

    const char *out;
    uint16_t len;
    CHECK(cfgpack_get_str_by_name(NULL, "e1", &out, &len) == CFGPACK_ERR_ARGS);
    LOG("NULL ctx -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

TEST_CASE(test_get_str_by_name_null_name) {
    LOG_SECTION("cfgpack_get_str_by_name with NULL name");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[3];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[3];
    char str_pool[256];
    uint16_t str_offsets[2];

    CHECK(make_ctx(&ctx, &schema, entries, values, str_pool, sizeof(str_pool),
                   str_offsets) == CFGPACK_OK);

    const char *out;
    uint16_t len;
    CHECK(cfgpack_get_str_by_name(&ctx, NULL, &out, &len) == CFGPACK_ERR_ARGS);
    LOG("NULL name -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

TEST_CASE(test_get_fstr_by_name_null_ctx) {
    LOG_SECTION("cfgpack_get_fstr_by_name with NULL ctx");

    const char *out;
    uint8_t len;
    CHECK(cfgpack_get_fstr_by_name(NULL, "e2", &out, &len) == CFGPACK_ERR_ARGS);
    LOG("NULL ctx -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

TEST_CASE(test_get_fstr_by_name_null_name) {
    LOG_SECTION("cfgpack_get_fstr_by_name with NULL name");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[3];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[3];
    char str_pool[256];
    uint16_t str_offsets[2];

    CHECK(make_ctx(&ctx, &schema, entries, values, str_pool, sizeof(str_pool),
                   str_offsets) == CFGPACK_OK);

    const char *out;
    uint8_t len;
    CHECK(cfgpack_get_fstr_by_name(&ctx, NULL, &out, &len) == CFGPACK_ERR_ARGS);
    LOG("NULL name -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 8. cfgpack_pageout / cfgpack_pagein NULL args
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_pageout_null_ctx) {
    LOG_SECTION("cfgpack_pageout with NULL ctx");

    uint8_t buf[64];
    size_t len;
    CHECK(cfgpack_pageout(NULL, buf, sizeof(buf), &len) == CFGPACK_ERR_ARGS);
    LOG("NULL ctx -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

TEST_CASE(test_pageout_null_out) {
    LOG_SECTION("cfgpack_pageout with NULL out buffer");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    char str_pool[1];
    uint16_t str_offsets[1];

    make_schema(&schema, entries, 1);
    CHECK(cfgpack_init(&ctx, &schema, values, 1, str_pool, sizeof(str_pool),
                       str_offsets, 0) == CFGPACK_OK);

    size_t len;
    CHECK(cfgpack_pageout(&ctx, NULL, 64, &len) == CFGPACK_ERR_ARGS);
    LOG("NULL out -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

TEST_CASE(test_pagein_null_ctx) {
    LOG_SECTION("cfgpack_pagein_remap with NULL ctx");

    uint8_t data[] = {0x80}; /* empty map */
    CHECK(cfgpack_pagein_remap(NULL, data, sizeof(data), NULL, 0) ==
          CFGPACK_ERR_ARGS);
    LOG("NULL ctx -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 9. cfgpack_set_fstr_by_name / cfgpack_get_fstr_by_name functional
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_fstr_by_name_roundtrip) {
    LOG_SECTION("set_fstr_by_name / get_fstr_by_name roundtrip");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[3];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[3];
    char str_pool[256];
    uint16_t str_offsets[2];

    CHECK(make_ctx(&ctx, &schema, entries, values, str_pool, sizeof(str_pool),
                   str_offsets) == CFGPACK_OK);

    LOG("Setting fstr entry 'e2' by name to 'test'");
    CHECK(cfgpack_set_fstr_by_name(&ctx, "e2", "test") == CFGPACK_OK);

    const char *out;
    uint8_t len;
    CHECK(cfgpack_get_fstr_by_name(&ctx, "e2", &out, &len) == CFGPACK_OK);
    CHECK(len == 4);
    CHECK(strncmp(out, "test", 4) == 0);
    LOG("Roundtrip: '%.*s' len=%u", (int)len, out, (unsigned)len);

    LOG("Overwriting with 'abcdefghijklmno' (15 chars, near max)");
    CHECK(cfgpack_set_fstr_by_name(&ctx, "e2", "abcdefghijklmno") ==
          CFGPACK_OK);
    CHECK(cfgpack_get_fstr_by_name(&ctx, "e2", &out, &len) == CFGPACK_OK);
    CHECK(len == 15);
    CHECK(strncmp(out, "abcdefghijklmno", 15) == 0);
    LOG("Roundtrip: len=%u", (unsigned)len);

    return TEST_OK;
}

TEST_CASE(test_str_by_name_roundtrip) {
    LOG_SECTION("set_str_by_name / get_str_by_name roundtrip");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[3];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[3];
    char str_pool[256];
    uint16_t str_offsets[2];

    CHECK(make_ctx(&ctx, &schema, entries, values, str_pool, sizeof(str_pool),
                   str_offsets) == CFGPACK_OK);

    LOG("Setting str entry 'e1' by name to 'hello'");
    CHECK(cfgpack_set_str_by_name(&ctx, "e1", "hello") == CFGPACK_OK);

    const char *out;
    uint16_t len;
    CHECK(cfgpack_get_str_by_name(&ctx, "e1", &out, &len) == CFGPACK_OK);
    CHECK(len == 5);
    CHECK(strncmp(out, "hello", 5) == 0);
    LOG("Roundtrip: '%.*s' len=%u", (int)len, out, (unsigned)len);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 10. Empty string (zero-length) for str and fstr
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_empty_str) {
    LOG_SECTION("set/get empty string (zero-length str)");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[3];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[3];
    char str_pool[256];
    uint16_t str_offsets[2];

    CHECK(make_ctx(&ctx, &schema, entries, values, str_pool, sizeof(str_pool),
                   str_offsets) == CFGPACK_OK);

    LOG("Setting str to empty string \"\"");
    CHECK(cfgpack_set_str(&ctx, 2, "") == CFGPACK_OK);

    const char *out;
    uint16_t len;
    CHECK(cfgpack_get_str(&ctx, 2, &out, &len) == CFGPACK_OK);
    CHECK(len == 0);
    CHECK(out[0] == '\0');
    LOG("Verified: len=0, null-terminated");

    LOG("Overwriting empty with 'hi', then back to empty");
    CHECK(cfgpack_set_str(&ctx, 2, "hi") == CFGPACK_OK);
    CHECK(cfgpack_get_str(&ctx, 2, &out, &len) == CFGPACK_OK);
    CHECK(len == 2);

    CHECK(cfgpack_set_str(&ctx, 2, "") == CFGPACK_OK);
    CHECK(cfgpack_get_str(&ctx, 2, &out, &len) == CFGPACK_OK);
    CHECK(len == 0);
    LOG("Re-emptied successfully");

    return TEST_OK;
}

TEST_CASE(test_empty_fstr) {
    LOG_SECTION("set/get empty string (zero-length fstr)");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[3];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[3];
    char str_pool[256];
    uint16_t str_offsets[2];

    CHECK(make_ctx(&ctx, &schema, entries, values, str_pool, sizeof(str_pool),
                   str_offsets) == CFGPACK_OK);

    LOG("Setting fstr to empty string \"\"");
    CHECK(cfgpack_set_fstr(&ctx, 3, "") == CFGPACK_OK);

    const char *out;
    uint8_t len;
    CHECK(cfgpack_get_fstr(&ctx, 3, &out, &len) == CFGPACK_OK);
    CHECK(len == 0);
    CHECK(out[0] == '\0');
    LOG("Verified: len=0, null-terminated");

    LOG("Overwriting empty with 'xy', then back to empty");
    CHECK(cfgpack_set_fstr(&ctx, 3, "xy") == CFGPACK_OK);
    CHECK(cfgpack_get_fstr(&ctx, 3, &out, &len) == CFGPACK_OK);
    CHECK(len == 2);

    CHECK(cfgpack_set_fstr(&ctx, 3, "") == CFGPACK_OK);
    CHECK(cfgpack_get_fstr(&ctx, 3, &out, &len) == CFGPACK_OK);
    CHECK(len == 0);
    LOG("Re-emptied successfully");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 11. Double init (calling cfgpack_init twice on same ctx)
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_double_init) {
    LOG_SECTION("Double cfgpack_init on same ctx");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    char str_pool[1];
    uint16_t str_offsets[1];

    make_schema(&schema, entries, 1);

    LOG("First init");
    CHECK(cfgpack_init(&ctx, &schema, values, 1, str_pool, sizeof(str_pool),
                       str_offsets, 0) == CFGPACK_OK);

    CHECK(cfgpack_set_u8(&ctx, 1, 42) == CFGPACK_OK);
    CHECK(cfgpack_get_size(&ctx) == 1);
    LOG("Set value, size=1");

    LOG("Second init (should reset state)");
    CHECK(cfgpack_init(&ctx, &schema, values, 1, str_pool, sizeof(str_pool),
                       str_offsets, 0) == CFGPACK_OK);

    CHECK(cfgpack_get_size(&ctx) == 0);
    LOG("After re-init: size=0 (presence bitmap cleared)");

    CHECK(cfgpack_set_u8(&ctx, 1, 99) == CFGPACK_OK);
    cfgpack_value_t v;
    CHECK(cfgpack_get(&ctx, 1, &v) == CFGPACK_OK);
    CHECK(v.v.u64 == 99);
    LOG("Context fully functional after re-init");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 12. cfgpack_print / cfgpack_print_all smoke test
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_print_basic) {
    LOG_SECTION("cfgpack_print / cfgpack_print_all smoke test");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[3];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[3];
    char str_pool[256];
    uint16_t str_offsets[2];

    CHECK(make_ctx(&ctx, &schema, entries, values, str_pool, sizeof(str_pool),
                   str_offsets) == CFGPACK_OK);

    CHECK(cfgpack_set_u8(&ctx, 1, 42) == CFGPACK_OK);
    CHECK(cfgpack_set_str(&ctx, 2, "hello") == CFGPACK_OK);
    CHECK(cfgpack_set_fstr(&ctx, 3, "world") == CFGPACK_OK);

    LOG("Print single entry (index 1):");
    CHECK(cfgpack_print(&ctx, 1) == CFGPACK_OK);

    LOG("Print all entries:");
    CHECK(cfgpack_print_all(&ctx) == CFGPACK_OK);

    /* Note: library is built without CFGPACK_HOSTED, so cfgpack_print is a
     * no-op that always returns CFGPACK_OK even for missing indices. */
    LOG("Print missing index (no-op in embedded build):");
    CHECK(cfgpack_print(&ctx, 99) == CFGPACK_OK);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 13. Parser NULL args (cfgpack_parse_schema, cfgpack_schema_measure)
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_parse_schema_null_data) {
    LOG_SECTION("cfgpack_parse_schema with NULL data");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[4];
    cfgpack_value_t values[4];
    char str_pool[256];
    uint16_t str_offsets[2];
    cfgpack_parse_error_t err;
    cfgpack_parse_opts_t opts = {&schema, entries,     4, values, str_pool,
                                 256,     str_offsets, 2, &err};

    CHECK(cfgpack_parse_schema(NULL, 10, &opts) == CFGPACK_ERR_ARGS);
    LOG("NULL data -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

TEST_CASE(test_measure_null_data) {
    LOG_SECTION("cfgpack_schema_measure with NULL data");

    cfgpack_schema_measure_t m;
    cfgpack_parse_error_t err;

    CHECK(cfgpack_schema_measure(NULL, 10, &m, &err) == CFGPACK_ERR_ARGS);
    LOG("NULL data -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

TEST_CASE(test_measure_null_out) {
    LOG_SECTION("cfgpack_schema_measure with NULL out");

    cfgpack_parse_error_t err;
    const char *data = "test 1\n";

    CHECK(cfgpack_schema_measure(data, strlen(data), NULL, &err) ==
          CFGPACK_ERR_ARGS);
    LOG("NULL out -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

TEST_CASE(test_measure_json_null_data) {
    LOG_SECTION("cfgpack_schema_measure_json with NULL data");

    cfgpack_schema_measure_t m;
    cfgpack_parse_error_t err;

    CHECK(cfgpack_schema_measure_json(NULL, 10, &m, &err) == CFGPACK_ERR_ARGS);
    LOG("NULL data -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

TEST_CASE(test_measure_json_null_out) {
    LOG_SECTION("cfgpack_schema_measure_json with NULL out");

    cfgpack_parse_error_t err;
    const char *data = "{}";

    CHECK(cfgpack_schema_measure_json(data, strlen(data), NULL, &err) ==
          CFGPACK_ERR_ARGS);
    LOG("NULL out -> CFGPACK_ERR_ARGS");

    return TEST_OK;
}

int main(void) {
    test_result_t overall = TEST_OK;

    /* cfgpack_init NULL args */
    overall |= (test_case_result("init_null_ctx", test_init_null_ctx()) !=
                TEST_OK);
    overall |= (test_case_result("init_null_schema", test_init_null_schema()) !=
                TEST_OK);

    /* cfgpack_set / cfgpack_get NULL args */
    overall |= (test_case_result("set_null_ctx", test_set_null_ctx()) !=
                TEST_OK);
    overall |= (test_case_result("set_null_value", test_set_null_value()) !=
                TEST_OK);
    overall |= (test_case_result("get_null_ctx", test_get_null_ctx()) !=
                TEST_OK);
    overall |= (test_case_result("get_null_out", test_get_null_out()) !=
                TEST_OK);

    /* cfgpack_set_by_name / cfgpack_get_by_name NULL args */
    overall |= (test_case_result("set_by_name_null_ctx",
                                 test_set_by_name_null_ctx()) != TEST_OK);
    overall |= (test_case_result("set_by_name_null_name",
                                 test_set_by_name_null_name()) != TEST_OK);
    overall |= (test_case_result("get_by_name_null_ctx",
                                 test_get_by_name_null_ctx()) != TEST_OK);
    overall |= (test_case_result("get_by_name_null_name",
                                 test_get_by_name_null_name()) != TEST_OK);

    /* cfgpack_set_str / cfgpack_get_str NULL args */
    overall |= (test_case_result("set_str_null_ctx", test_set_str_null_ctx()) !=
                TEST_OK);
    overall |= (test_case_result("set_str_null_str", test_set_str_null_str()) !=
                TEST_OK);
    overall |= (test_case_result("get_str_null_ctx", test_get_str_null_ctx()) !=
                TEST_OK);
    overall |= (test_case_result("get_str_null_out", test_get_str_null_out()) !=
                TEST_OK);

    /* cfgpack_set_fstr / cfgpack_get_fstr NULL args */
    overall |= (test_case_result("set_fstr_null_ctx",
                                 test_set_fstr_null_ctx()) != TEST_OK);
    overall |= (test_case_result("set_fstr_null_str",
                                 test_set_fstr_null_str()) != TEST_OK);
    overall |= (test_case_result("get_fstr_null_ctx",
                                 test_get_fstr_null_ctx()) != TEST_OK);
    overall |= (test_case_result("get_fstr_null_out",
                                 test_get_fstr_null_out()) != TEST_OK);

    /* str_by_name / fstr_by_name NULL args */
    overall |= (test_case_result("set_str_by_name_null_ctx",
                                 test_set_str_by_name_null_ctx()) != TEST_OK);
    overall |= (test_case_result("set_str_by_name_null_name",
                                 test_set_str_by_name_null_name()) != TEST_OK);
    overall |= (test_case_result("set_fstr_by_name_null_ctx",
                                 test_set_fstr_by_name_null_ctx()) != TEST_OK);
    overall |= (test_case_result("set_fstr_by_name_null_name",
                                 test_set_fstr_by_name_null_name()) != TEST_OK);
    overall |= (test_case_result("get_str_by_name_null_ctx",
                                 test_get_str_by_name_null_ctx()) != TEST_OK);
    overall |= (test_case_result("get_str_by_name_null_name",
                                 test_get_str_by_name_null_name()) != TEST_OK);
    overall |= (test_case_result("get_fstr_by_name_null_ctx",
                                 test_get_fstr_by_name_null_ctx()) != TEST_OK);
    overall |= (test_case_result("get_fstr_by_name_null_name",
                                 test_get_fstr_by_name_null_name()) != TEST_OK);

    /* pageout / pagein NULL args */
    overall |= (test_case_result("pageout_null_ctx", test_pageout_null_ctx()) !=
                TEST_OK);
    overall |= (test_case_result("pageout_null_out", test_pageout_null_out()) !=
                TEST_OK);
    overall |= (test_case_result("pagein_null_ctx", test_pagein_null_ctx()) !=
                TEST_OK);

    /* fstr_by_name functional roundtrip */
    overall |= (test_case_result("fstr_by_name_roundtrip",
                                 test_fstr_by_name_roundtrip()) != TEST_OK);
    overall |= (test_case_result("str_by_name_roundtrip",
                                 test_str_by_name_roundtrip()) != TEST_OK);

    /* empty string tests */
    overall |= (test_case_result("empty_str", test_empty_str()) != TEST_OK);
    overall |= (test_case_result("empty_fstr", test_empty_fstr()) != TEST_OK);

    /* double init */
    overall |= (test_case_result("double_init", test_double_init()) != TEST_OK);

    /* print smoke test */
    overall |= (test_case_result("print_basic", test_print_basic()) != TEST_OK);

    /* parser NULL args */
    overall |= (test_case_result("parse_schema_null_data",
                                 test_parse_schema_null_data()) != TEST_OK);
    overall |= (test_case_result("measure_null_data",
                                 test_measure_null_data()) != TEST_OK);
    overall |= (test_case_result("measure_null_out", test_measure_null_out()) !=
                TEST_OK);
    overall |= (test_case_result("measure_json_null_data",
                                 test_measure_json_null_data()) != TEST_OK);
    overall |= (test_case_result("measure_json_null_out",
                                 test_measure_json_null_out()) != TEST_OK);

    if (overall == TEST_OK) {
        printf(COLOR_GREEN "ALL PASS" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "SOME FAIL" COLOR_RESET "\n");
    }
    return (overall);
}
