#include "cfgpack/cfgpack.h"
#include "test.h"

#include <string.h>

TEST_CASE(test_basic_case) {
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_value_t defaults[2];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[2];
    uint8_t present[(2+7)/8];
    cfgpack_value_t v1;
    cfgpack_value_t v2;
    cfgpack_value_t out;
    uint8_t buf[256];
    size_t out_len = 0;
    cfgpack_err_t rc;

    schema.map_name[0] = '\0';
    schema.version = 1;
    schema.entry_count = 2;
    schema.entries = entries;

    entries[0].index = 1; snprintf(entries[0].name, sizeof(entries[0].name), "%s", "a"); entries[0].type = CFGPACK_TYPE_U8; entries[0].has_default = 0;
    entries[1].index = 2; snprintf(entries[1].name, sizeof(entries[1].name), "%s", "b"); entries[1].type = CFGPACK_TYPE_STR; entries[1].has_default = 0;

    rc = cfgpack_init(&ctx, &schema, values, 2, defaults, present, sizeof(present));
    CHECK(rc == CFGPACK_OK);

    v1.type = CFGPACK_TYPE_U8;
    v1.v.u64 = 5;
    rc = cfgpack_set(&ctx, 1, &v1);
    CHECK(rc == CFGPACK_OK);

    v2.type = CFGPACK_TYPE_STR;
    v2.v.str.len = 3;
    v2.v.str.data[0] = 'f';
    v2.v.str.data[1] = 'o';
    v2.v.str.data[2] = 'o';
    v2.v.str.data[3] = '\0';
    rc = cfgpack_set(&ctx, 2, &v2);
    CHECK(rc == CFGPACK_OK);

    rc = cfgpack_get(&ctx, 1, &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.u64 == 5);

    rc = cfgpack_get(&ctx, 2, &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.str.len == 3);

    /* name-based access */
    rc = cfgpack_get_by_name(&ctx, "a", &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.u64 == 5);

    rc = cfgpack_get_by_name(&ctx, "b", &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.str.len == 3);

    /* name-based set */
    v1.v.u64 = 9;
    rc = cfgpack_set_by_name(&ctx, "a", &v1);
    CHECK(rc == CFGPACK_OK);
    rc = cfgpack_get(&ctx, 1, &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.u64 == 9);

    rc = cfgpack_pageout(&ctx, buf, sizeof(buf), &out_len);
    CHECK(rc == CFGPACK_OK);

    /* clear and read back */
    memset(values, 0, sizeof(values));
    memset(present, 0, sizeof(present));
    rc = cfgpack_pagein_buf(&ctx, buf, out_len);
    CHECK(rc == CFGPACK_OK);
    rc = cfgpack_get(&ctx, 1, &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.u64 == 9);

    rc = cfgpack_get_by_name(&ctx, "a", &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.u64 == 9);

    cfgpack_free(&ctx);
    return (TEST_OK);
}

TEST_CASE(test_pageout_small_buffer) {
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_value_t defaults[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    uint8_t present[(1+7)/8];
    cfgpack_value_t v;
    uint8_t buf[4]; /* intentionally tiny */
    size_t out_len = 0;
    cfgpack_err_t rc;

    schema.map_name[0] = '\0';
    schema.version = 1;
    schema.entry_count = 1;
    schema.entries = entries;

    entries[0].index = 1; snprintf(entries[0].name, sizeof(entries[0].name), "%s", "a"); entries[0].type = CFGPACK_TYPE_U8; entries[0].has_default = 0;

    rc = cfgpack_init(&ctx, &schema, values, 1, defaults, present, sizeof(present));
    CHECK(rc == CFGPACK_OK);

    v.type = CFGPACK_TYPE_U8;
    v.v.u64 = 5;
    rc = cfgpack_set(&ctx, 1, &v);
    CHECK(rc == CFGPACK_OK);

    rc = cfgpack_pageout(&ctx, buf, sizeof(buf), &out_len);
    CHECK(rc == CFGPACK_ERR_ENCODE);

    cfgpack_free(&ctx);
    return (TEST_OK);
}

TEST_CASE(test_defaults_applied_at_init) {
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
    cfgpack_value_t defaults[2];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[2];
    uint8_t present[(2+7)/8];
    cfgpack_value_t out;
    cfgpack_err_t rc;

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

    rc = cfgpack_init(&ctx, &schema, values, 2, defaults, present, sizeof(present));
    CHECK(rc == CFGPACK_OK);

    /* Entry with default should be present and have correct value */
    rc = cfgpack_get(&ctx, 1, &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.u64 == 42);

    /* Entry without default should be missing */
    rc = cfgpack_get(&ctx, 2, &out);
    CHECK(rc == CFGPACK_ERR_MISSING);

    /* Override the default */
    cfgpack_value_t v;
    v.type = CFGPACK_TYPE_U8;
    v.v.u64 = 99;
    rc = cfgpack_set(&ctx, 1, &v);
    CHECK(rc == CFGPACK_OK);

    rc = cfgpack_get(&ctx, 1, &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.u64 == 99);

    cfgpack_free(&ctx);
    return (TEST_OK);
}

TEST_CASE(test_reset_to_defaults) {
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[3];
    cfgpack_value_t defaults[3];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[3];
    uint8_t present[(3+7)/8];
    cfgpack_value_t out;
    cfgpack_err_t rc;

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

    rc = cfgpack_init(&ctx, &schema, values, 3, defaults, present, sizeof(present));
    CHECK(rc == CFGPACK_OK);

    /* Verify defaults are applied */
    rc = cfgpack_get(&ctx, 1, &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.u64 == 10);

    rc = cfgpack_get(&ctx, 2, &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.fstr.len == 2);
    CHECK(strcmp(out.v.fstr.data, "hi") == 0);

    rc = cfgpack_get(&ctx, 3, &out);
    CHECK(rc == CFGPACK_ERR_MISSING);

    /* Modify values */
    cfgpack_value_t v;
    v.type = CFGPACK_TYPE_U8;
    v.v.u64 = 99;
    rc = cfgpack_set(&ctx, 1, &v);
    CHECK(rc == CFGPACK_OK);

    v.type = CFGPACK_TYPE_FSTR;
    v.v.fstr.len = 5;
    snprintf(v.v.fstr.data, sizeof(v.v.fstr.data), "%s", "hello");
    rc = cfgpack_set(&ctx, 2, &v);
    CHECK(rc == CFGPACK_OK);

    v.type = CFGPACK_TYPE_U16;
    v.v.u64 = 1000;
    rc = cfgpack_set(&ctx, 3, &v);
    CHECK(rc == CFGPACK_OK);

    /* Verify modified values */
    rc = cfgpack_get(&ctx, 1, &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.u64 == 99);

    rc = cfgpack_get(&ctx, 2, &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(strcmp(out.v.fstr.data, "hello") == 0);

    rc = cfgpack_get(&ctx, 3, &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.u64 == 1000);

    /* Reset to defaults */
    cfgpack_reset_to_defaults(&ctx);

    /* Verify defaults are restored */
    rc = cfgpack_get(&ctx, 1, &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.u64 == 10);

    rc = cfgpack_get(&ctx, 2, &out);
    CHECK(rc == CFGPACK_OK);
    CHECK(out.v.fstr.len == 2);
    CHECK(strcmp(out.v.fstr.data, "hi") == 0);

    /* Entry without default should be missing again */
    rc = cfgpack_get(&ctx, 3, &out);
    CHECK(rc == CFGPACK_ERR_MISSING);

    cfgpack_free(&ctx);
    return (TEST_OK);
}

TEST_CASE(test_typed_convenience_functions) {
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[6];
    cfgpack_value_t defaults[6];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[6];
    uint8_t present[(6+7)/8];
    cfgpack_err_t rc;

    schema.map_name[0] = '\0';
    schema.version = 1;
    schema.entry_count = 6;
    schema.entries = entries;

    /* Set up schema with various types */
    entries[0].index = 1; snprintf(entries[0].name, sizeof(entries[0].name), "%s", "u8v"); entries[0].type = CFGPACK_TYPE_U8; entries[0].has_default = 0;
    entries[1].index = 2; snprintf(entries[1].name, sizeof(entries[1].name), "%s", "i32v"); entries[1].type = CFGPACK_TYPE_I32; entries[1].has_default = 0;
    entries[2].index = 3; snprintf(entries[2].name, sizeof(entries[2].name), "%s", "f32v"); entries[2].type = CFGPACK_TYPE_F32; entries[2].has_default = 0;
    entries[3].index = 4; snprintf(entries[3].name, sizeof(entries[3].name), "%s", "strv"); entries[3].type = CFGPACK_TYPE_STR; entries[3].has_default = 0;
    entries[4].index = 5; snprintf(entries[4].name, sizeof(entries[4].name), "%s", "fstr"); entries[4].type = CFGPACK_TYPE_FSTR; entries[4].has_default = 0;
    entries[5].index = 6; snprintf(entries[5].name, sizeof(entries[5].name), "%s", "u64v"); entries[5].type = CFGPACK_TYPE_U64; entries[5].has_default = 0;

    rc = cfgpack_init(&ctx, &schema, values, 6, defaults, present, sizeof(present));
    CHECK(rc == CFGPACK_OK);

    /* Test typed setters by index */
    rc = cfgpack_set_u8(&ctx, 1, 42);
    CHECK(rc == CFGPACK_OK);

    rc = cfgpack_set_i32(&ctx, 2, -12345);
    CHECK(rc == CFGPACK_OK);

    rc = cfgpack_set_f32(&ctx, 3, 3.14f);
    CHECK(rc == CFGPACK_OK);

    rc = cfgpack_set_str(&ctx, 4, "hello world");
    CHECK(rc == CFGPACK_OK);

    rc = cfgpack_set_fstr(&ctx, 5, "fixed");
    CHECK(rc == CFGPACK_OK);

    rc = cfgpack_set_u64(&ctx, 6, 0xDEADBEEFCAFEULL);
    CHECK(rc == CFGPACK_OK);

    /* Test typed getters by index */
    uint8_t u8_out;
    rc = cfgpack_get_u8(&ctx, 1, &u8_out);
    CHECK(rc == CFGPACK_OK);
    CHECK(u8_out == 42);

    int32_t i32_out;
    rc = cfgpack_get_i32(&ctx, 2, &i32_out);
    CHECK(rc == CFGPACK_OK);
    CHECK(i32_out == -12345);

    float f32_out;
    rc = cfgpack_get_f32(&ctx, 3, &f32_out);
    CHECK(rc == CFGPACK_OK);
    CHECK(f32_out > 3.13f && f32_out < 3.15f);

    const char *str_out;
    uint16_t str_len;
    rc = cfgpack_get_str(&ctx, 4, &str_out, &str_len);
    CHECK(rc == CFGPACK_OK);
    CHECK(str_len == 11);
    CHECK(strcmp(str_out, "hello world") == 0);

    const char *fstr_out;
    uint8_t fstr_len;
    rc = cfgpack_get_fstr(&ctx, 5, &fstr_out, &fstr_len);
    CHECK(rc == CFGPACK_OK);
    CHECK(fstr_len == 5);
    CHECK(strcmp(fstr_out, "fixed") == 0);

    uint64_t u64_out;
    rc = cfgpack_get_u64(&ctx, 6, &u64_out);
    CHECK(rc == CFGPACK_OK);
    CHECK(u64_out == 0xDEADBEEFCAFEULL);

    /* Test typed setters by name */
    rc = cfgpack_set_u8_by_name(&ctx, "u8v", 99);
    CHECK(rc == CFGPACK_OK);

    rc = cfgpack_set_str_by_name(&ctx, "strv", "updated");
    CHECK(rc == CFGPACK_OK);

    /* Test typed getters by name */
    rc = cfgpack_get_u8_by_name(&ctx, "u8v", &u8_out);
    CHECK(rc == CFGPACK_OK);
    CHECK(u8_out == 99);

    rc = cfgpack_get_str_by_name(&ctx, "strv", &str_out, &str_len);
    CHECK(rc == CFGPACK_OK);
    CHECK(strcmp(str_out, "updated") == 0);

    /* Test type mismatch errors */
    rc = cfgpack_get_u8(&ctx, 2, &u8_out);  /* index 2 is i32, not u8 */
    CHECK(rc == CFGPACK_ERR_TYPE_MISMATCH);

    rc = cfgpack_set_u8(&ctx, 2, 5);  /* index 2 is i32, not u8 */
    CHECK(rc == CFGPACK_ERR_TYPE_MISMATCH);

    /* Test string too long */
    char long_str[70];
    memset(long_str, 'x', 65);
    long_str[65] = '\0';
    rc = cfgpack_set_str(&ctx, 4, long_str);
    CHECK(rc == CFGPACK_ERR_STR_TOO_LONG);

    cfgpack_free(&ctx);
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
