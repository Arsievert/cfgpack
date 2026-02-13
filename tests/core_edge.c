/* Edge-case tests for core get/set, string pool, and utility functions. */

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

/* ═══════════════════════════════════════════════════════════════════════════
 * 1. cfgpack_get with index not in schema -> ERR_MISSING
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_get_invalid_index) {
    LOG_SECTION("cfgpack_get with index not in schema");

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
    LOG("Context initialized with 1 entry at index 1");

    LOG("Attempting cfgpack_get with index 99 (not in schema)");
    CHECK(cfgpack_get(&ctx, 99, &v) == CFGPACK_ERR_MISSING);
    LOG("Correctly returned: CFGPACK_ERR_MISSING");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 2. cfgpack_set with index not in schema -> ERR_MISSING
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_set_invalid_index) {
    LOG_SECTION("cfgpack_set with index not in schema");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    char str_pool[1];
    uint16_t str_offsets[1];

    make_schema(&schema, entries, 1);

    CHECK(cfgpack_init(&ctx, &schema, values, 1, str_pool, sizeof(str_pool),
                       str_offsets, 0) == CFGPACK_OK);
    LOG("Context initialized with 1 entry at index 1");

    LOG("Attempting cfgpack_set with index 99 (not in schema)");
    cfgpack_value_t v = {.type = CFGPACK_TYPE_U8, .v.u64 = 42};
    CHECK(cfgpack_set(&ctx, 99, &v) == CFGPACK_ERR_MISSING);
    LOG("Correctly returned: CFGPACK_ERR_MISSING");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 3. cfgpack_get_by_name with unknown name -> ERR_MISSING
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_get_by_name_not_found) {
    LOG_SECTION("cfgpack_get_by_name with unknown name");

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
    LOG("Context initialized with 1 entry named 'e0'");

    LOG("Attempting cfgpack_get_by_name with 'nope'");
    CHECK(cfgpack_get_by_name(&ctx, "nope", &v) == CFGPACK_ERR_MISSING);
    LOG("Correctly returned: CFGPACK_ERR_MISSING");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 4. cfgpack_set_by_name with unknown name -> ERR_MISSING
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_set_by_name_not_found) {
    LOG_SECTION("cfgpack_set_by_name with unknown name");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    char str_pool[1];
    uint16_t str_offsets[1];

    make_schema(&schema, entries, 1);

    CHECK(cfgpack_init(&ctx, &schema, values, 1, str_pool, sizeof(str_pool),
                       str_offsets, 0) == CFGPACK_OK);
    LOG("Context initialized with 1 entry named 'e0'");

    LOG("Attempting cfgpack_set_by_name with 'nope'");
    cfgpack_value_t v = {.type = CFGPACK_TYPE_U8, .v.u64 = 42};
    CHECK(cfgpack_set_by_name(&ctx, "nope", &v) == CFGPACK_ERR_MISSING);
    LOG("Correctly returned: CFGPACK_ERR_MISSING");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 5. cfgpack_set with reserved index 0 -> ERR_RESERVED_INDEX
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_set_reserved_index_zero) {
    LOG_SECTION("cfgpack_set with index 0 (reserved)");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    char str_pool[1];
    uint16_t str_offsets[1];

    make_schema(&schema, entries, 1);

    CHECK(cfgpack_init(&ctx, &schema, values, 1, str_pool, sizeof(str_pool),
                       str_offsets, 0) == CFGPACK_OK);
    LOG("Context initialized");

    LOG("Attempting cfgpack_set with index 0");
    cfgpack_value_t v = {.type = CFGPACK_TYPE_U8, .v.u64 = 1};
    CHECK(cfgpack_set(&ctx, 0, &v) == CFGPACK_ERR_RESERVED_INDEX);
    LOG("Correctly returned: CFGPACK_ERR_RESERVED_INDEX");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 6. cfgpack_get with reserved index 0 -> ERR_RESERVED_INDEX
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_get_reserved_index_zero) {
    LOG_SECTION("cfgpack_get with index 0 (reserved)");

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
    LOG("Context initialized");

    LOG("Attempting cfgpack_get with index 0");
    CHECK(cfgpack_get(&ctx, 0, &v) == CFGPACK_ERR_RESERVED_INDEX);
    LOG("Correctly returned: CFGPACK_ERR_RESERVED_INDEX");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 7. String accessors with reserved index 0
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_str_reserved_index_zero) {
    LOG_SECTION("String accessors with index 0 (reserved)");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[2];
    char str_pool[256];
    uint16_t str_offsets[2];

    make_schema(&schema, entries, 2);
    entries[0].type = CFGPACK_TYPE_STR;
    entries[1].type = CFGPACK_TYPE_FSTR;

    CHECK(cfgpack_init(&ctx, &schema, values, 2, str_pool, sizeof(str_pool),
                       str_offsets, 2) == CFGPACK_OK);
    LOG("Context initialized with str at index 1, fstr at index 2");

    LOG("Testing cfgpack_set_str with index 0");
    CHECK(cfgpack_set_str(&ctx, 0, "test") == CFGPACK_ERR_RESERVED_INDEX);
    LOG("Correctly returned: CFGPACK_ERR_RESERVED_INDEX");

    LOG("Testing cfgpack_get_str with index 0");
    const char *str_out;
    uint16_t str_len;
    CHECK(cfgpack_get_str(&ctx, 0, &str_out, &str_len) ==
          CFGPACK_ERR_RESERVED_INDEX);
    LOG("Correctly returned: CFGPACK_ERR_RESERVED_INDEX");

    LOG("Testing cfgpack_set_fstr with index 0");
    CHECK(cfgpack_set_fstr(&ctx, 0, "test") == CFGPACK_ERR_RESERVED_INDEX);
    LOG("Correctly returned: CFGPACK_ERR_RESERVED_INDEX");

    LOG("Testing cfgpack_get_fstr with index 0");
    const char *fstr_out;
    uint8_t fstr_len;
    CHECK(cfgpack_get_fstr(&ctx, 0, &fstr_out, &fstr_len) ==
          CFGPACK_ERR_RESERVED_INDEX);
    LOG("Correctly returned: CFGPACK_ERR_RESERVED_INDEX");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 8. String overwrite (shorter then longer)
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_string_overwrite) {
    LOG_SECTION("Overwrite str value with different lengths");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    char str_pool[128];
    uint16_t str_offsets[1];

    make_schema(&schema, entries, 1);
    entries[0].type = CFGPACK_TYPE_STR;

    CHECK(cfgpack_init(&ctx, &schema, values, 1, str_pool, sizeof(str_pool),
                       str_offsets, 1) == CFGPACK_OK);
    LOG("Context initialized with 1 str entry");

    LOG("Setting str to 'hello' (5 chars)");
    CHECK(cfgpack_set_str(&ctx, 1, "hello") == CFGPACK_OK);
    const char *out;
    uint16_t len;
    CHECK(cfgpack_get_str(&ctx, 1, &out, &len) == CFGPACK_OK);
    CHECK(len == 5);
    CHECK(strncmp(out, "hello", 5) == 0);
    LOG("Verified: '%s' len=%u", out, len);

    LOG("Overwriting str with 'hi' (2 chars, shorter)");
    CHECK(cfgpack_set_str(&ctx, 1, "hi") == CFGPACK_OK);
    CHECK(cfgpack_get_str(&ctx, 1, &out, &len) == CFGPACK_OK);
    CHECK(len == 2);
    CHECK(strncmp(out, "hi", 2) == 0);
    LOG("Verified: '%s' len=%u", out, len);

    LOG("Overwriting str with 'longer_string' (13 chars, longer)");
    CHECK(cfgpack_set_str(&ctx, 1, "longer_string") == CFGPACK_OK);
    CHECK(cfgpack_get_str(&ctx, 1, &out, &len) == CFGPACK_OK);
    CHECK(len == 13);
    CHECK(strncmp(out, "longer_string", 13) == 0);
    LOG("Verified: '%s' len=%u", out, len);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 9. fstr overwrite
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_fstr_overwrite) {
    LOG_SECTION("Overwrite fstr value and verify");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    char str_pool[32];
    uint16_t str_offsets[1];

    make_schema(&schema, entries, 1);
    entries[0].type = CFGPACK_TYPE_FSTR;

    CHECK(cfgpack_init(&ctx, &schema, values, 1, str_pool, sizeof(str_pool),
                       str_offsets, 1) == CFGPACK_OK);
    LOG("Context initialized with 1 fstr entry");

    LOG("Setting fstr to 'abc' (3 chars)");
    CHECK(cfgpack_set_fstr(&ctx, 1, "abc") == CFGPACK_OK);
    const char *out;
    uint8_t len;
    CHECK(cfgpack_get_fstr(&ctx, 1, &out, &len) == CFGPACK_OK);
    CHECK(len == 3);
    CHECK(strncmp(out, "abc", 3) == 0);
    LOG("Verified: '%s' len=%u", out, (unsigned)len);

    LOG("Overwriting fstr with 'x' (1 char)");
    CHECK(cfgpack_set_fstr(&ctx, 1, "x") == CFGPACK_OK);
    CHECK(cfgpack_get_fstr(&ctx, 1, &out, &len) == CFGPACK_OK);
    CHECK(len == 1);
    CHECK(out[0] == 'x');
    LOG("Verified: '%s' len=%u", out, (unsigned)len);

    LOG("Overwriting fstr with 'sixteen_chars!!' (15 chars, near max)");
    CHECK(cfgpack_set_fstr(&ctx, 1, "sixteen_chars!!") == CFGPACK_OK);
    CHECK(cfgpack_get_fstr(&ctx, 1, &out, &len) == CFGPACK_OK);
    CHECK(len == 15);
    CHECK(strncmp(out, "sixteen_chars!!", 15) == 0);
    LOG("Verified: '%s' len=%u", out, (unsigned)len);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 10. cfgpack_get_version returns schema version
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_get_version) {
    LOG_SECTION("cfgpack_get_version returns schema version");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    char str_pool[1];
    uint16_t str_offsets[1];

    make_schema(&schema, entries, 1);
    schema.version = 42;

    CHECK(cfgpack_init(&ctx, &schema, values, 1, str_pool, sizeof(str_pool),
                       str_offsets, 0) == CFGPACK_OK);
    LOG("Context initialized with schema version 42");

    CHECK(cfgpack_get_version(&ctx) == 42);
    LOG("cfgpack_get_version returned 42 (correct)");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 11. cfgpack_get_size reflects presence count
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_get_size) {
    LOG_SECTION("cfgpack_get_size reflects presence count");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[3];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[3];
    char str_pool[1];
    uint16_t str_offsets[1];

    make_schema(&schema, entries, 3);

    CHECK(cfgpack_init(&ctx, &schema, values, 3, str_pool, sizeof(str_pool),
                       str_offsets, 0) == CFGPACK_OK);
    LOG("Context initialized with 3 entries");

    CHECK(cfgpack_get_size(&ctx) == 0);
    LOG("Initial size = 0 (correct)");

    CHECK(cfgpack_set_u8(&ctx, 1, 10) == CFGPACK_OK);
    CHECK(cfgpack_get_size(&ctx) == 1);
    LOG("After setting index 1: size = 1 (correct)");

    CHECK(cfgpack_set_u8(&ctx, 2, 20) == CFGPACK_OK);
    CHECK(cfgpack_get_size(&ctx) == 2);
    LOG("After setting index 2: size = 2 (correct)");

    CHECK(cfgpack_set_u8(&ctx, 3, 30) == CFGPACK_OK);
    CHECK(cfgpack_get_size(&ctx) == 3);
    LOG("After setting index 3: size = 3 (correct)");

    /* Pagein a minimal map with only 1 entry -> resets to 1 */
    uint8_t tiny[] = {0x81, 0x01, 0x05};
    CHECK(cfgpack_pagein_buf(&ctx, tiny, sizeof(tiny)) == CFGPACK_OK);
    CHECK(cfgpack_get_size(&ctx) == 1);
    LOG("After pagein with 1 entry: size = 1 (correct, reset)");

    return TEST_OK;
}

int main(void) {
    test_result_t overall = TEST_OK;

    overall |= (test_case_result("get_invalid_index",
                                 test_get_invalid_index()) != TEST_OK);
    overall |= (test_case_result("set_invalid_index",
                                 test_set_invalid_index()) != TEST_OK);
    overall |= (test_case_result("get_by_name_not_found",
                                 test_get_by_name_not_found()) != TEST_OK);
    overall |= (test_case_result("set_by_name_not_found",
                                 test_set_by_name_not_found()) != TEST_OK);
    overall |= (test_case_result("set_reserved_index_zero",
                                 test_set_reserved_index_zero()) != TEST_OK);
    overall |= (test_case_result("get_reserved_index_zero",
                                 test_get_reserved_index_zero()) != TEST_OK);
    overall |= (test_case_result("str_reserved_index_zero",
                                 test_str_reserved_index_zero()) != TEST_OK);
    overall |= (test_case_result("string_overwrite", test_string_overwrite()) !=
                TEST_OK);
    overall |= (test_case_result("fstr_overwrite", test_fstr_overwrite()) !=
                TEST_OK);
    overall |= (test_case_result("get_version", test_get_version()) != TEST_OK);
    overall |= (test_case_result("get_size", test_get_size()) != TEST_OK);

    if (overall == TEST_OK) {
        printf(COLOR_GREEN "ALL PASS" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "SOME FAIL" COLOR_RESET "\n");
    }
    return (overall);
}
