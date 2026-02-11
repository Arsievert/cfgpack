#include "cfgpack/cfgpack.h"
#include "test.h"

#include <string.h>

TEST_CASE(test_basic_case) {
    LOG_SECTION("Basic set/get/pageout/pagein test");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_fat_value_t defaults[2];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[2];
    cfgpack_value_t out;
    uint8_t buf[256];
    size_t out_len = 0;
    cfgpack_err_t rc;

    /* String pool for 1 str entry (max 64+1 bytes) */
    char str_pool[128];
    uint16_t str_offsets[1]; /* 1 string entry (index 2 is str) */

    LOG("Creating schema with 2 entries: 'a' (u8) and 'b' (str)");
    schema.map_name[0] = '\0';
    schema.version = 1;
    schema.entry_count = 2;
    schema.entries = entries;

    entries[0].index = 1;
    snprintf(entries[0].name, sizeof(entries[0].name), "%s", "a");
    entries[0].type = CFGPACK_TYPE_U8;
    entries[0].has_default = 0;
    entries[1].index = 2;
    snprintf(entries[1].name, sizeof(entries[1].name), "%s", "b");
    entries[1].type = CFGPACK_TYPE_STR;
    entries[1].has_default = 0;

    LOG("Initializing context with 2-entry schema");
    rc = cfgpack_init(&ctx, &schema, values, 2, defaults, str_pool, sizeof(str_pool), str_offsets, 1);
    CHECK(rc == CFGPACK_OK);
    LOG("cfgpack_init() returned OK");

    LOG("Setting entry 'a' (index 1) to u8 value 5");
    rc = cfgpack_set_u8(&ctx, 1, 5);
    CHECK(rc == CFGPACK_OK);

    LOG("Setting entry 'b' (index 2) to str value \"foo\"");
    rc = cfgpack_set_str(&ctx, 2, "foo");
    CHECK(rc == CFGPACK_OK);

    LOG("Getting entry at index 1 (expect u8=5)");
    rc = cfgpack_get(&ctx, 1, &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.u64 == 5);
    LOG_VALUE("Retrieved value", out);

    LOG("Getting entry at index 2 (expect str len=3)");
    const char *str_out;
    uint16_t str_len;
    rc = cfgpack_get_str(&ctx, 2, &str_out, &str_len);
    CHECK(rc == CFGPACK_OK);
    CHECK(str_len == 3);
    CHECK(strcmp(str_out, "foo") == 0);
    LOG("Retrieved str: \"%s\" (len=%u)", str_out, str_len);

    LOG("Testing name-based access: get_by_name('a')");
    rc = cfgpack_get_by_name(&ctx, "a", &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.u64 == 5);
    LOG("get_by_name('a') returned u64=%llu", (unsigned long long)out.v.u64);

    LOG("Testing name-based access: get_by_name('b') via get_str_by_name");
    rc = cfgpack_get_str_by_name(&ctx, "b", &str_out, &str_len);
    CHECK(rc == CFGPACK_OK);
    CHECK(str_len == 3);
    LOG("get_str_by_name('b') returned str len=%u", str_len);

    LOG("Testing name-based set: set_by_name('a', 9)");
    cfgpack_value_t v1;
    v1.type = CFGPACK_TYPE_U8;
    v1.v.u64 = 9;
    rc = cfgpack_set_by_name(&ctx, "a", &v1);
    CHECK(rc == CFGPACK_OK);
    rc = cfgpack_get(&ctx, 1, &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.u64 == 9);
    LOG("Value updated successfully to %llu", (unsigned long long)out.v.u64);

    LOG("Calling pageout() to serialize context to buffer");
    rc = cfgpack_pageout(&ctx, buf, sizeof(buf), &out_len);
    CHECK(rc == CFGPACK_OK);
    LOG("Serialized %zu bytes", out_len);
    LOG_HEX("Serialized data", buf, out_len);

    LOG("Clearing values and present bits, then calling pagein_buf()");
    memset(values, 0, sizeof(values));
    memset(ctx.present, 0, sizeof(ctx.present));
    rc = cfgpack_pagein_buf(&ctx, buf, out_len);
    CHECK(rc == CFGPACK_OK);
    LOG("pagein_buf() succeeded");

    LOG("Verifying restored values");
    rc = cfgpack_get(&ctx, 1, &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.u64 == 9);
    LOG_VALUE("Restored index 1", out);

    rc = cfgpack_get_by_name(&ctx, "a", &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.u64 == 9);
    LOG("get_by_name('a') after restore: %llu", (unsigned long long)out.v.u64);

    cfgpack_free(&ctx);
    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_pageout_small_buffer) {
    LOG_SECTION("Test pageout with buffer too small");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_fat_value_t defaults[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    uint8_t buf[4]; /* intentionally tiny */
    size_t out_len = 0;
    cfgpack_err_t rc;

    /* No string entries, so minimal pool */
    char str_pool[1];
    uint16_t str_offsets[1];

    LOG("Creating schema with 1 entry: 'a' (u8)");
    schema.map_name[0] = '\0';
    schema.version = 1;
    schema.entry_count = 1;
    schema.entries = entries;

    entries[0].index = 1;
    snprintf(entries[0].name, sizeof(entries[0].name), "%s", "a");
    entries[0].type = CFGPACK_TYPE_U8;
    entries[0].has_default = 0;

    LOG("Initializing context");
    rc = cfgpack_init(&ctx, &schema, values, 1, defaults, str_pool, sizeof(str_pool), str_offsets, 0);
    CHECK(rc == CFGPACK_OK);

    LOG("Setting entry 'a' to value 5");
    rc = cfgpack_set_u8(&ctx, 1, 5);
    CHECK(rc == CFGPACK_OK);

    LOG("Calling pageout with 4-byte buffer (too small)");
    rc = cfgpack_pageout(&ctx, buf, sizeof(buf), &out_len);
    CHECK(rc == CFGPACK_ERR_ENCODE);
    LOG("pageout() correctly returned CFGPACK_ERR_ENCODE");

    cfgpack_free(&ctx);
    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_defaults_applied_at_init) {
    LOG_SECTION("Test default values applied at init");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_fat_value_t defaults[2];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[2];
    cfgpack_value_t out;
    cfgpack_err_t rc;

    /* No string entries */
    char str_pool[1];
    uint16_t str_offsets[1];

    LOG("Creating schema with 2 entries:");
    LOG("  Entry 0 (index 1): 'a' (u8) with default=42");
    LOG("  Entry 1 (index 2): 'b' (u8) without default (NIL)");

    schema.map_name[0] = '\0';
    schema.version = 1;
    schema.entry_count = 2;
    schema.entries = entries;

    /* Entry 0: has default value 42 */
    entries[0].index = 1;
    snprintf(entries[0].name, sizeof(entries[0].name), "%s", "a");
    entries[0].type = CFGPACK_TYPE_U8;
    entries[0].has_default = 1;
    defaults[0].type = CFGPACK_TYPE_U8;
    defaults[0].v.u64 = 42;

    /* Entry 1: no default (NIL) */
    entries[1].index = 2;
    snprintf(entries[1].name, sizeof(entries[1].name), "%s", "b");
    entries[1].type = CFGPACK_TYPE_U8;
    entries[1].has_default = 0;

    LOG("Initializing context");
    rc = cfgpack_init(&ctx, &schema, values, 2, defaults, str_pool, sizeof(str_pool), str_offsets, 0);
    CHECK(rc == CFGPACK_OK);

    LOG("Getting entry with default (index 1)");
    rc = cfgpack_get(&ctx, 1, &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.u64 == 42);
    LOG_VALUE("Entry with default", out);

    LOG("Getting entry without default (index 2) - expect CFGPACK_ERR_MISSING");
    rc = cfgpack_get(&ctx, 2, &out);
    CHECK(rc == CFGPACK_ERR_MISSING);
    LOG("Correctly returned CFGPACK_ERR_MISSING for missing entry");

    LOG("Overriding default: setting index 1 to 99");
    cfgpack_value_t v;
    v.type = CFGPACK_TYPE_U8;
    v.v.u64 = 99;
    rc = cfgpack_set(&ctx, 1, &v);
    CHECK(rc == CFGPACK_OK);

    rc = cfgpack_get(&ctx, 1, &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.u64 == 99);
    LOG_VALUE("After override", out);

    cfgpack_free(&ctx);
    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_reset_to_defaults) {
    LOG_SECTION("Test reset_to_defaults functionality");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[3];
    cfgpack_fat_value_t defaults[3];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[3];
    cfgpack_value_t out;
    cfgpack_err_t rc;

    /* 1 fstr entry */
    char str_pool[32];
    uint16_t str_offsets[1];

    LOG("Creating schema with 3 entries:");
    LOG("  Entry 0 (index 1): 'a' (u8) with default=10");
    LOG("  Entry 1 (index 2): 'msg' (fstr) with default=\"hi\"");
    LOG("  Entry 2 (index 3): 'b' (u16) without default (NIL)");

    schema.map_name[0] = '\0';
    schema.version = 1;
    schema.entry_count = 3;
    schema.entries = entries;

    /* Entry 0: has default value 10 */
    entries[0].index = 1;
    snprintf(entries[0].name, sizeof(entries[0].name), "%s", "a");
    entries[0].type = CFGPACK_TYPE_U8;
    entries[0].has_default = 1;
    defaults[0].type = CFGPACK_TYPE_U8;
    defaults[0].v.u64 = 10;

    /* Entry 1: has default string "hi" */
    entries[1].index = 2;
    snprintf(entries[1].name, sizeof(entries[1].name), "%s", "msg");
    entries[1].type = CFGPACK_TYPE_FSTR;
    entries[1].has_default = 1;
    defaults[1].type = CFGPACK_TYPE_FSTR;
    defaults[1].v.fstr.len = 2;
    snprintf(defaults[1].v.fstr.data, sizeof(defaults[1].v.fstr.data), "%s", "hi");

    /* Entry 2: no default (NIL) */
    entries[2].index = 3;
    snprintf(entries[2].name, sizeof(entries[2].name), "%s", "b");
    entries[2].type = CFGPACK_TYPE_U16;
    entries[2].has_default = 0;

    LOG("Initializing context");
    rc = cfgpack_init(&ctx, &schema, values, 3, defaults, str_pool, sizeof(str_pool), str_offsets, 1);
    CHECK(rc == CFGPACK_OK);

    LOG("Verifying defaults are applied at init");
    rc = cfgpack_get(&ctx, 1, &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.u64 == 10);
    LOG_VALUE("Index 1 (default)", out);

    const char *fstr_out;
    uint8_t fstr_len;
    rc = cfgpack_get_fstr(&ctx, 2, &fstr_out, &fstr_len);
    CHECK(rc == CFGPACK_OK);
    CHECK(fstr_len == 2);
    CHECK(strcmp(fstr_out, "hi") == 0);
    LOG("Index 2 (default fstr): \"%s\" len=%u", fstr_out, fstr_len);

    rc = cfgpack_get(&ctx, 3, &out);
    CHECK(rc == CFGPACK_ERR_MISSING);
    LOG("Index 3 correctly missing (no default)");

    LOG("Modifying all values:");
    cfgpack_value_t v;
    v.type = CFGPACK_TYPE_U8;
    v.v.u64 = 99;
    rc = cfgpack_set(&ctx, 1, &v);
    CHECK(rc == CFGPACK_OK);
    LOG("  Set index 1 to 99");

    rc = cfgpack_set_fstr(&ctx, 2, "hello");
    CHECK(rc == CFGPACK_OK);
    LOG("  Set index 2 to \"hello\"");

    v.type = CFGPACK_TYPE_U16;
    v.v.u64 = 1000;
    rc = cfgpack_set(&ctx, 3, &v);
    CHECK(rc == CFGPACK_OK);
    LOG("  Set index 3 to 1000");

    LOG("Verifying modified values:");
    rc = cfgpack_get(&ctx, 1, &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.u64 == 99);
    LOG_VALUE("Index 1 (modified)", out);

    rc = cfgpack_get_fstr(&ctx, 2, &fstr_out, &fstr_len);
    CHECK(rc == CFGPACK_OK);
    CHECK(strcmp(fstr_out, "hello") == 0);
    LOG("Index 2 (modified fstr): \"%s\"", fstr_out);

    rc = cfgpack_get(&ctx, 3, &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.u64 == 1000);
    LOG_VALUE("Index 3 (modified)", out);

    LOG("Calling cfgpack_reset_to_defaults()");
    cfgpack_reset_to_defaults(&ctx);

    LOG("Verifying defaults are restored:");
    rc = cfgpack_get(&ctx, 1, &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.u64 == 10);
    LOG_VALUE("Index 1 (restored)", out);

    rc = cfgpack_get_fstr(&ctx, 2, &fstr_out, &fstr_len);
    CHECK(rc == CFGPACK_OK);
    CHECK(fstr_len == 2);
    CHECK(strcmp(fstr_out, "hi") == 0);
    LOG("Index 2 (restored fstr): \"%s\" len=%u", fstr_out, fstr_len);

    rc = cfgpack_get(&ctx, 3, &out);
    CHECK(rc == CFGPACK_ERR_MISSING);
    LOG("Index 3 correctly missing again (no default)");

    cfgpack_free(&ctx);
    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_typed_convenience_functions) {
    LOG_SECTION("Test typed convenience functions (set_u8, get_u8, etc.)");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[6];
    cfgpack_fat_value_t defaults[6];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[6];
    cfgpack_err_t rc;

    /* 2 string entries: index 4 (str) and index 5 (fstr) */
    char str_pool[128];
    uint16_t str_offsets[2];

    LOG("Creating schema with 6 entries of various types:");
    LOG("  Index 1: u8v (u8)");
    LOG("  Index 2: i32v (i32)");
    LOG("  Index 3: f32v (f32)");
    LOG("  Index 4: strv (str)");
    LOG("  Index 5: fstr (fstr)");
    LOG("  Index 6: u64v (u64)");

    schema.map_name[0] = '\0';
    schema.version = 1;
    schema.entry_count = 6;
    schema.entries = entries;

    /* Set up schema with various types */
    entries[0].index = 1;
    snprintf(entries[0].name, sizeof(entries[0].name), "%s", "u8v");
    entries[0].type = CFGPACK_TYPE_U8;
    entries[0].has_default = 0;
    entries[1].index = 2;
    snprintf(entries[1].name, sizeof(entries[1].name), "%s", "i32v");
    entries[1].type = CFGPACK_TYPE_I32;
    entries[1].has_default = 0;
    entries[2].index = 3;
    snprintf(entries[2].name, sizeof(entries[2].name), "%s", "f32v");
    entries[2].type = CFGPACK_TYPE_F32;
    entries[2].has_default = 0;
    entries[3].index = 4;
    snprintf(entries[3].name, sizeof(entries[3].name), "%s", "strv");
    entries[3].type = CFGPACK_TYPE_STR;
    entries[3].has_default = 0;
    entries[4].index = 5;
    snprintf(entries[4].name, sizeof(entries[4].name), "%s", "fstr");
    entries[4].type = CFGPACK_TYPE_FSTR;
    entries[4].has_default = 0;
    entries[5].index = 6;
    snprintf(entries[5].name, sizeof(entries[5].name), "%s", "u64v");
    entries[5].type = CFGPACK_TYPE_U64;
    entries[5].has_default = 0;

    LOG("Initializing context");
    rc = cfgpack_init(&ctx, &schema, values, 6, defaults, str_pool, sizeof(str_pool), str_offsets, 2);
    CHECK(rc == CFGPACK_OK);

    LOG("Testing typed setters by index:");

    LOG("  cfgpack_set_u8(ctx, 1, 42)");
    rc = cfgpack_set_u8(&ctx, 1, 42);
    CHECK(rc == CFGPACK_OK);

    LOG("  cfgpack_set_i32(ctx, 2, -12345)");
    rc = cfgpack_set_i32(&ctx, 2, -12345);
    CHECK(rc == CFGPACK_OK);

    LOG("  cfgpack_set_f32(ctx, 3, 3.14f)");
    rc = cfgpack_set_f32(&ctx, 3, 3.14f);
    CHECK(rc == CFGPACK_OK);

    LOG("  cfgpack_set_str(ctx, 4, \"hello world\")");
    rc = cfgpack_set_str(&ctx, 4, "hello world");
    CHECK(rc == CFGPACK_OK);

    LOG("  cfgpack_set_fstr(ctx, 5, \"fixed\")");
    rc = cfgpack_set_fstr(&ctx, 5, "fixed");
    CHECK(rc == CFGPACK_OK);

    LOG("  cfgpack_set_u64(ctx, 6, 0xDEADBEEFCAFE)");
    rc = cfgpack_set_u64(&ctx, 6, 0xDEADBEEFCAFEULL);
    CHECK(rc == CFGPACK_OK);

    LOG("Testing typed getters by index:");

    uint8_t u8_out;
    rc = cfgpack_get_u8(&ctx, 1, &u8_out);
    CHECK(rc == CFGPACK_OK);
    CHECK(u8_out == 42);
    LOG("  cfgpack_get_u8(ctx, 1) = %u", u8_out);

    int32_t i32_out;
    rc = cfgpack_get_i32(&ctx, 2, &i32_out);
    CHECK(rc == CFGPACK_OK);
    CHECK(i32_out == -12345);
    LOG("  cfgpack_get_i32(ctx, 2) = %d", i32_out);

    float f32_out;
    rc = cfgpack_get_f32(&ctx, 3, &f32_out);
    CHECK(rc == CFGPACK_OK);
    CHECK(f32_out > 3.13f && f32_out < 3.15f);
    LOG("  cfgpack_get_f32(ctx, 3) = %f", f32_out);

    const char *str_out;
    uint16_t str_len;
    rc = cfgpack_get_str(&ctx, 4, &str_out, &str_len);
    CHECK(rc == CFGPACK_OK);
    CHECK(str_len == 11);
    CHECK(strcmp(str_out, "hello world") == 0);
    LOG("  cfgpack_get_str(ctx, 4) = \"%s\" (len=%u)", str_out, str_len);

    const char *fstr_out;
    uint8_t fstr_len;
    rc = cfgpack_get_fstr(&ctx, 5, &fstr_out, &fstr_len);
    CHECK(rc == CFGPACK_OK);
    CHECK(fstr_len == 5);
    CHECK(strcmp(fstr_out, "fixed") == 0);
    LOG("  cfgpack_get_fstr(ctx, 5) = \"%s\" (len=%u)", fstr_out, fstr_len);

    uint64_t u64_out;
    rc = cfgpack_get_u64(&ctx, 6, &u64_out);
    CHECK(rc == CFGPACK_OK);
    CHECK(u64_out == 0xDEADBEEFCAFEULL);
    LOG("  cfgpack_get_u64(ctx, 6) = 0x%llx", (unsigned long long)u64_out);

    LOG("Testing typed setters by name:");

    LOG("  cfgpack_set_u8_by_name(ctx, \"u8v\", 99)");
    rc = cfgpack_set_u8_by_name(&ctx, "u8v", 99);
    CHECK(rc == CFGPACK_OK);

    LOG("  cfgpack_set_str_by_name(ctx, \"strv\", \"updated\")");
    rc = cfgpack_set_str_by_name(&ctx, "strv", "updated");
    CHECK(rc == CFGPACK_OK);

    LOG("Testing typed getters by name:");

    rc = cfgpack_get_u8_by_name(&ctx, "u8v", &u8_out);
    CHECK(rc == CFGPACK_OK);
    CHECK(u8_out == 99);
    LOG("  cfgpack_get_u8_by_name(ctx, \"u8v\") = %u", u8_out);

    rc = cfgpack_get_str_by_name(&ctx, "strv", &str_out, &str_len);
    CHECK(rc == CFGPACK_OK);
    CHECK(strcmp(str_out, "updated") == 0);
    LOG("  cfgpack_get_str_by_name(ctx, \"strv\") = \"%s\"", str_out);

    LOG("Testing type mismatch errors:");

    LOG("  cfgpack_get_u8(ctx, 2) - index 2 is i32, not u8");
    rc = cfgpack_get_u8(&ctx, 2, &u8_out);
    CHECK(rc == CFGPACK_ERR_TYPE_MISMATCH);
    LOG("  Correctly returned CFGPACK_ERR_TYPE_MISMATCH");

    LOG("  cfgpack_set_u8(ctx, 2, 5) - index 2 is i32, not u8");
    rc = cfgpack_set_u8(&ctx, 2, 5);
    CHECK(rc == CFGPACK_ERR_TYPE_MISMATCH);
    LOG("  Correctly returned CFGPACK_ERR_TYPE_MISMATCH");

    LOG("Testing string too long error:");
    char long_str[70];
    memset(long_str, 'x', 65);
    long_str[65] = '\0';
    LOG("  cfgpack_set_str(ctx, 4, <65-char string>) - max is 64");
    rc = cfgpack_set_str(&ctx, 4, long_str);
    CHECK(rc == CFGPACK_ERR_STR_TOO_LONG);
    LOG("  Correctly returned CFGPACK_ERR_STR_TOO_LONG");

    cfgpack_free(&ctx);
    LOG("Test completed successfully");
    return (TEST_OK);
}

int main(void) {
    test_result_t overall = TEST_OK;

    overall |= (test_case_result("basic", test_basic_case()) != TEST_OK);
    overall |= (test_case_result("pageout_small_buffer", test_pageout_small_buffer()) != TEST_OK);
    overall |= (test_case_result("defaults_applied_at_init", test_defaults_applied_at_init()) != TEST_OK);
    overall |= (test_case_result("reset_to_defaults", test_reset_to_defaults()) != TEST_OK);
    overall |= (test_case_result("typed_convenience_functions", test_typed_convenience_functions()) != TEST_OK);

    if (overall == TEST_OK) {
        printf(COLOR_GREEN "ALL PASS" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "SOME FAIL" COLOR_RESET "\n");
    }
    return (overall);
}
