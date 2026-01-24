#include "cfgpack/cfgpack.h"
#include "test.h"

#include <string.h>

TEST_CASE(test_basic_case) {
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[2];
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

    entries[0].index = 1; snprintf(entries[0].name, sizeof(entries[0].name), "%s", "a"); entries[0].type = CFGPACK_TYPE_U8;
    entries[1].index = 2; snprintf(entries[1].name, sizeof(entries[1].name), "%s", "b"); entries[1].type = CFGPACK_TYPE_STR;

    rc = cfgpack_init(&ctx, &schema, values, 2, present, sizeof(present));
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

    entries[0].index = 1; snprintf(entries[0].name, sizeof(entries[0].name), "%s", "a"); entries[0].type = CFGPACK_TYPE_U8;

    rc = cfgpack_init(&ctx, &schema, values, 1, present, sizeof(present));
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

int main(void) {
    test_result_t overall = TEST_OK;

    overall |= (test_case_result("basic", test_basic_case()) != TEST_OK);
    overall |= (test_case_result("pageout_small_buffer", test_pageout_small_buffer()) != TEST_OK);

    if (overall == TEST_OK) {
        printf("ALL PASS\n");
    } else {
        printf("SOME FAIL\n");
    }
    return (overall);
}
