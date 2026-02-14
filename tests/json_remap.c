/* End-to-end tests: JSON-parsed schemas + pagein_remap.
 *
 * Every test parses schemas from JSON strings (not programmatic construction),
 * then exercises pagein_remap to verify that JSON-parsed schemas produce
 * identical remap behavior to .map-parsed or programmatic schemas.
 */

#include "cfgpack/cfgpack.h"
#include "cfgpack/msgpack.h"
#include "test.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Helper: parse a JSON schema and init a context from it. */
static cfgpack_err_t json_init(const char *json,
                               cfgpack_schema_t *schema,
                               cfgpack_entry_t *entries,
                               size_t max_entries,
                               cfgpack_value_t *values,
                               char *str_pool,
                               size_t str_pool_cap,
                               uint16_t *str_offsets,
                               size_t str_offsets_count,
                               cfgpack_ctx_t *ctx) {
    cfgpack_parse_error_t err;
    cfgpack_err_t rc = cfgpack_schema_parse_json(json, strlen(json), schema,
                                                 entries, max_entries, values,
                                                 str_pool, str_pool_cap,
                                                 str_offsets, str_offsets_count,
                                                 &err);
    if (rc != CFGPACK_OK) {
        LOG("JSON parse error: %s (line %zu)", err.message, err.line);
        return rc;
    }
    return cfgpack_init(ctx, schema, values, schema->entry_count, str_pool,
                        str_pool_cap, str_offsets, str_offsets_count);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 1. Basic remap: JSON v1(u8@10,u8@11) -> v2(u8@20,u8@21), remap 10->20, 11->21
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_json_remap_basic) {
    LOG_SECTION("JSON-parsed schemas: basic index remap");

    static const char *v1_json =
        "{"
        "  \"name\": \"cfg\","
        "  \"version\": 1,"
        "  \"entries\": ["
        "    {\"index\": 10, \"name\": \"a\", \"type\": \"u8\", \"value\": 0},"
        "    {\"index\": 11, \"name\": \"b\", \"type\": \"u8\", \"value\": 0}"
        "  ]"
        "}";

    static const char *v2_json =
        "{"
        "  \"name\": \"cfg\","
        "  \"version\": 2,"
        "  \"entries\": ["
        "    {\"index\": 20, \"name\": \"a\", \"type\": \"u8\", \"value\": 0},"
        "    {\"index\": 21, \"name\": \"b\", \"type\": \"u8\", \"value\": 0}"
        "  ]"
        "}";

    cfgpack_schema_t s1, s2;
    cfgpack_entry_t e1[2], e2[2];
    cfgpack_ctx_t c1, c2;
    cfgpack_value_t v1[2], v2[2];
    char sp1[1], sp2[1];
    uint16_t so1[1], so2[1];

    LOG("Parsing v1 JSON schema");
    CHECK(json_init(v1_json, &s1, e1, 2, v1, sp1, sizeof(sp1), so1, 0, &c1) ==
          CFGPACK_OK);
    LOG("Parsing v2 JSON schema");
    CHECK(json_init(v2_json, &s2, e2, 2, v2, sp2, sizeof(sp2), so2, 0, &c2) ==
          CFGPACK_OK);

    CHECK(cfgpack_set_u8(&c1, 10, 42) == CFGPACK_OK);
    CHECK(cfgpack_set_u8(&c1, 11, 99) == CFGPACK_OK);
    LOG("Set v1 values: a@10=42, b@11=99");

    uint8_t buf[128];
    size_t len = 0;
    CHECK(cfgpack_pageout(&c1, buf, sizeof(buf), &len) == CFGPACK_OK);
    LOG("Pageout succeeded, len=%zu", len);

    cfgpack_remap_entry_t remap[] = {{10, 20}, {11, 21}};
    CHECK(cfgpack_pagein_remap(&c2, buf, len, remap, 2) == CFGPACK_OK);
    LOG("Pagein with remap succeeded");

    cfgpack_value_t v;
    CHECK(cfgpack_get(&c2, 20, &v) == CFGPACK_OK && v.v.u64 == 42);
    LOG("a@20 = %" PRIu64 " (correct)", v.v.u64);
    CHECK(cfgpack_get(&c2, 21, &v) == CFGPACK_OK && v.v.u64 == 99);
    LOG("b@21 = %" PRIu64 " (correct)", v.v.u64);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 2. Type widening: JSON v1(u8@1) -> v2(u16@1)
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_json_remap_type_widening) {
    LOG_SECTION("JSON-parsed schemas: u8 -> u16 type widening via remap");

    static const char *v1_json =
        "{"
        "  \"name\": \"widen\","
        "  \"version\": 1,"
        "  \"entries\": ["
        "    {\"index\": 1, \"name\": \"val\", \"type\": \"u8\", \"value\": 0}"
        "  ]"
        "}";

    static const char *v2_json =
        "{"
        "  \"name\": \"widen\","
        "  \"version\": 2,"
        "  \"entries\": ["
        "    {\"index\": 1, \"name\": \"val\", \"type\": \"u16\", \"value\": 0}"
        "  ]"
        "}";

    cfgpack_schema_t s1, s2;
    cfgpack_entry_t e1[1], e2[1];
    cfgpack_ctx_t c1, c2;
    cfgpack_value_t v1[1], v2[1];
    char sp1[1], sp2[1];
    uint16_t so1[1], so2[1];

    CHECK(json_init(v1_json, &s1, e1, 1, v1, sp1, sizeof(sp1), so1, 0, &c1) ==
          CFGPACK_OK);
    CHECK(json_init(v2_json, &s2, e2, 1, v2, sp2, sizeof(sp2), so2, 0, &c2) ==
          CFGPACK_OK);

    CHECK(cfgpack_set_u8(&c1, 1, 200) == CFGPACK_OK);
    LOG("Set v1 val@1 = 200 (u8)");

    uint8_t buf[64];
    size_t len = 0;
    CHECK(cfgpack_pageout(&c1, buf, sizeof(buf), &len) == CFGPACK_OK);
    CHECK(cfgpack_pagein_remap(&c2, buf, len, NULL, 0) == CFGPACK_OK);
    LOG("Pagein with widening succeeded");

    cfgpack_value_t v;
    CHECK(cfgpack_get(&c2, 1, &v) == CFGPACK_OK && v.v.u64 == 200);
    LOG("val@1 = %" PRIu64 " (u16, widened from u8, correct)", v.v.u64);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 3. String remap: JSON v1(str@5, fstr@6) -> v2(str@15, fstr@16)
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_json_remap_strings) {
    LOG_SECTION("JSON-parsed schemas: str + fstr index remap");

    static const char *v1_json =
        "{"
        "  \"name\": \"strv1\","
        "  \"version\": 1,"
        "  \"entries\": ["
        "    {\"index\": 5, \"name\": \"host\", \"type\": \"str\","
        "     \"value\": \"orig\"},"
        "    {\"index\": 6, \"name\": \"tag\", \"type\": \"fstr\","
        "     \"value\": \"v1\"}"
        "  ]"
        "}";

    static const char *v2_json =
        "{"
        "  \"name\": \"strv2\","
        "  \"version\": 2,"
        "  \"entries\": ["
        "    {\"index\": 15, \"name\": \"host\", \"type\": \"str\","
        "     \"value\": \"dflt\"},"
        "    {\"index\": 16, \"name\": \"tag\", \"type\": \"fstr\","
        "     \"value\": \"v2\"}"
        "  ]"
        "}";

    cfgpack_schema_t s1, s2;
    cfgpack_entry_t e1[2], e2[2];
    cfgpack_ctx_t c1, c2;
    cfgpack_value_t v1[2], v2[2];
    char sp1[256], sp2[256];
    uint16_t so1[2], so2[2];

    CHECK(json_init(v1_json, &s1, e1, 2, v1, sp1, sizeof(sp1), so1, 2, &c1) ==
          CFGPACK_OK);
    LOG("Parsed v1 JSON: str@5, fstr@6");
    CHECK(json_init(v2_json, &s2, e2, 2, v2, sp2, sizeof(sp2), so2, 2, &c2) ==
          CFGPACK_OK);
    LOG("Parsed v2 JSON: str@15, fstr@16");

    /* Override defaults with runtime values */
    CHECK(cfgpack_set_str(&c1, 5, "hello") == CFGPACK_OK);
    CHECK(cfgpack_set_fstr(&c1, 6, "world") == CFGPACK_OK);
    LOG("Set v1: str@5='hello', fstr@6='world'");

    uint8_t buf[256];
    size_t len = 0;
    CHECK(cfgpack_pageout(&c1, buf, sizeof(buf), &len) == CFGPACK_OK);
    LOG("Pageout succeeded, len=%zu", len);

    cfgpack_remap_entry_t remap[] = {{5, 15}, {6, 16}};
    CHECK(cfgpack_pagein_remap(&c2, buf, len, remap, 2) == CFGPACK_OK);
    LOG("Pagein with string remap succeeded");

    const char *str_out;
    uint16_t str_len;
    CHECK(cfgpack_get_str(&c2, 15, &str_out, &str_len) == CFGPACK_OK);
    CHECK(str_len == 5 && strncmp(str_out, "hello", 5) == 0);
    LOG("str@15 = '%s' (correct)", str_out);

    const char *fstr_out;
    uint8_t fstr_len;
    CHECK(cfgpack_get_fstr(&c2, 16, &fstr_out, &fstr_len) == CFGPACK_OK);
    CHECK(fstr_len == 5 && strncmp(fstr_out, "world", 5) == 0);
    LOG("fstr@16 = '%s' (correct)", fstr_out);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 4. Defaults restoration: v2 adds new entries with defaults
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_json_remap_defaults_restored) {
    LOG_SECTION("JSON-parsed schemas: new v2 entries with defaults restored");

    static const char *v1_json =
        "{"
        "  \"name\": \"def\","
        "  \"version\": 1,"
        "  \"entries\": ["
        "    {\"index\": 1, \"name\": \"a\", \"type\": \"u8\", \"value\": 0}"
        "  ]"
        "}";

    /* v2 adds entry b@2 with default=42 and c@3 with default=99 */
    static const char *v2_json =
        "{"
        "  \"name\": \"def\","
        "  \"version\": 2,"
        "  \"entries\": ["
        "    {\"index\": 1, \"name\": \"a\", \"type\": \"u8\", \"value\": 0},"
        "    {\"index\": 2, \"name\": \"b\", \"type\": \"u8\", \"value\": 42},"
        "    {\"index\": 3, \"name\": \"c\", \"type\": \"u16\", \"value\": 999}"
        "  ]"
        "}";

    cfgpack_schema_t s1, s2;
    cfgpack_entry_t e1[1], e2[3];
    cfgpack_ctx_t c1, c2;
    cfgpack_value_t v1[1], v2[3];
    char sp1[1], sp2[1];
    uint16_t so1[1], so2[1];

    CHECK(json_init(v1_json, &s1, e1, 1, v1, sp1, sizeof(sp1), so1, 0, &c1) ==
          CFGPACK_OK);
    CHECK(json_init(v2_json, &s2, e2, 3, v2, sp2, sizeof(sp2), so2, 0, &c2) ==
          CFGPACK_OK);

    /* Verify v2 defaults were parsed and entries have has_default=1 */
    LOG("v2 entry b: has_default=%d, c: has_default=%d", e2[1].has_default,
        e2[2].has_default);
    CHECK(e2[1].has_default == 1);
    CHECK(e2[2].has_default == 1);

    CHECK(cfgpack_set_u8(&c1, 1, 77) == CFGPACK_OK);
    LOG("Set v1: a@1=77");

    uint8_t buf[128];
    size_t len = 0;
    CHECK(cfgpack_pageout(&c1, buf, sizeof(buf), &len) == CFGPACK_OK);

    /* Pagein old data into v2 context */
    CHECK(cfgpack_pagein_remap(&c2, buf, len, NULL, 0) == CFGPACK_OK);
    LOG("Pagein remap succeeded (identity, no index changes)");

    cfgpack_value_t v;
    /* a@1: migrated from old data */
    CHECK(cfgpack_get(&c2, 1, &v) == CFGPACK_OK && v.v.u64 == 77);
    LOG("a@1 = %" PRIu64 " (migrated, correct)", v.v.u64);

    /* b@2: not in old data, has_default=1 -> present with default=42 */
    CHECK(cfgpack_get(&c2, 2, &v) == CFGPACK_OK);
    CHECK(v.v.u64 == 42);
    LOG("b@2 = %" PRIu64 " (default restored, correct)", v.v.u64);

    /* c@3: not in old data, has_default=1 -> present with default=999 */
    CHECK(cfgpack_get(&c2, 3, &v) == CFGPACK_OK);
    CHECK(v.v.u64 == 999);
    LOG("c@3 = %" PRIu64 " (default restored, correct)", v.v.u64);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 5. Mixed types: remap across JSON schemas with u8, i16, f32, str, fstr
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_json_remap_mixed_types) {
    LOG_SECTION("JSON-parsed schemas: mixed types remap");

    static const char *v1_json =
        "{"
        "  \"name\": \"mix\","
        "  \"version\": 1,"
        "  \"entries\": ["
        "    {\"index\": 1, \"name\": \"temp\", \"type\": \"u8\", \"value\": "
        "0},"
        "    {\"index\": 2, \"name\": \"off\", \"type\": \"i16\","
        "     \"value\": 0},"
        "    {\"index\": 3, \"name\": \"gain\", \"type\": \"f32\","
        "     \"value\": 1.0},"
        "    {\"index\": 4, \"name\": \"host\", \"type\": \"str\","
        "     \"value\": \"none\"},"
        "    {\"index\": 5, \"name\": \"tag\", \"type\": \"fstr\","
        "     \"value\": \"v1\"}"
        "  ]"
        "}";

    static const char *v2_json =
        "{"
        "  \"name\": \"mix\","
        "  \"version\": 2,"
        "  \"entries\": ["
        "    {\"index\": 10, \"name\": \"temp\", \"type\": \"u8\","
        "     \"value\": 0},"
        "    {\"index\": 11, \"name\": \"off\", \"type\": \"i16\","
        "     \"value\": 0},"
        "    {\"index\": 12, \"name\": \"gain\", \"type\": \"f32\","
        "     \"value\": 1.0},"
        "    {\"index\": 13, \"name\": \"host\", \"type\": \"str\","
        "     \"value\": \"none\"},"
        "    {\"index\": 14, \"name\": \"tag\", \"type\": \"fstr\","
        "     \"value\": \"v2\"}"
        "  ]"
        "}";

    cfgpack_schema_t s1, s2;
    cfgpack_entry_t e1[5], e2[5];
    cfgpack_ctx_t c1, c2;
    cfgpack_value_t v1[5], v2[5];
    char sp1[512], sp2[512];
    uint16_t so1[2], so2[2];

    CHECK(json_init(v1_json, &s1, e1, 5, v1, sp1, sizeof(sp1), so1, 2, &c1) ==
          CFGPACK_OK);
    CHECK(json_init(v2_json, &s2, e2, 5, v2, sp2, sizeof(sp2), so2, 2, &c2) ==
          CFGPACK_OK);
    LOG("Both schemas parsed from JSON");

    CHECK(cfgpack_set_u8(&c1, 1, 25) == CFGPACK_OK);
    CHECK(cfgpack_set_i16(&c1, 2, -100) == CFGPACK_OK);
    CHECK(cfgpack_set_f32(&c1, 3, 2.5f) == CFGPACK_OK);
    CHECK(cfgpack_set_str(&c1, 4, "sensor.local") == CFGPACK_OK);
    CHECK(cfgpack_set_fstr(&c1, 5, "abc") == CFGPACK_OK);
    LOG("Set v1 values: temp=25, off=-100, gain=2.5, host='sensor.local',"
        " tag='abc'");

    uint8_t buf[512];
    size_t len = 0;
    CHECK(cfgpack_pageout(&c1, buf, sizeof(buf), &len) == CFGPACK_OK);
    LOG("Pageout succeeded, len=%zu", len);

    cfgpack_remap_entry_t remap[] = {{1, 10},
                                     {2, 11},
                                     {3, 12},
                                     {4, 13},
                                     {5, 14}};
    CHECK(cfgpack_pagein_remap(&c2, buf, len, remap, 5) == CFGPACK_OK);
    LOG("Pagein with mixed-type remap succeeded");

    cfgpack_value_t v;
    CHECK(cfgpack_get(&c2, 10, &v) == CFGPACK_OK && v.v.u64 == 25);
    LOG("temp@10 = %" PRIu64 " (correct)", v.v.u64);

    CHECK(cfgpack_get(&c2, 11, &v) == CFGPACK_OK && v.v.i64 == -100);
    LOG("off@11 = %" PRId64 " (correct)", v.v.i64);

    CHECK(cfgpack_get(&c2, 12, &v) == CFGPACK_OK);
    CHECK(fabsf(v.v.f32 - 2.5f) < 1e-6f);
    LOG("gain@12 = %f (correct)", (double)v.v.f32);

    const char *str_out;
    uint16_t str_len;
    CHECK(cfgpack_get_str(&c2, 13, &str_out, &str_len) == CFGPACK_OK);
    CHECK(str_len == 12 && strncmp(str_out, "sensor.local", 12) == 0);
    LOG("host@13 = '%s' (correct)", str_out);

    const char *fstr_out;
    uint8_t fstr_len;
    CHECK(cfgpack_get_fstr(&c2, 14, &fstr_out, &fstr_len) == CFGPACK_OK);
    CHECK(fstr_len == 3 && strncmp(fstr_out, "abc", 3) == 0);
    LOG("tag@14 = '%s' (correct)", fstr_out);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 6. No-default entry stays absent after remap
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_json_remap_no_default_stays_absent) {
    LOG_SECTION("JSON-parsed schemas: no-default entry remains absent");

    static const char *v1_json =
        "{"
        "  \"name\": \"nd\","
        "  \"version\": 1,"
        "  \"entries\": ["
        "    {\"index\": 1, \"name\": \"a\", \"type\": \"u8\", \"value\": 0}"
        "  ]"
        "}";

    /* v2 adds b@2 with no default (null) and c@3 with default=55 */
    static const char *v2_json =
        "{"
        "  \"name\": \"nd\","
        "  \"version\": 2,"
        "  \"entries\": ["
        "    {\"index\": 1, \"name\": \"a\", \"type\": \"u8\", \"value\": 0},"
        "    {\"index\": 2, \"name\": \"b\", \"type\": \"u8\", \"value\": "
        "null},"
        "    {\"index\": 3, \"name\": \"c\", \"type\": \"u8\", \"value\": 55}"
        "  ]"
        "}";

    cfgpack_schema_t s1, s2;
    cfgpack_entry_t e1[1], e2[3];
    cfgpack_ctx_t c1, c2;
    cfgpack_value_t v1[1], v2[3];
    char sp1[1], sp2[1];
    uint16_t so1[1], so2[1];

    CHECK(json_init(v1_json, &s1, e1, 1, v1, sp1, sizeof(sp1), so1, 0, &c1) ==
          CFGPACK_OK);
    CHECK(json_init(v2_json, &s2, e2, 3, v2, sp2, sizeof(sp2), so2, 0, &c2) ==
          CFGPACK_OK);

    /* Confirm has_default flags from JSON parse */
    LOG("v2 b@2: has_default=%d, c@3: has_default=%d", e2[1].has_default,
        e2[2].has_default);
    CHECK(e2[1].has_default == 0); /* null -> no default */
    CHECK(e2[2].has_default == 1); /* 55 -> has default */

    CHECK(cfgpack_set_u8(&c1, 1, 77) == CFGPACK_OK);
    LOG("Set v1: a@1=77");

    uint8_t buf[128];
    size_t len = 0;
    CHECK(cfgpack_pageout(&c1, buf, sizeof(buf), &len) == CFGPACK_OK);
    CHECK(cfgpack_pagein_remap(&c2, buf, len, NULL, 0) == CFGPACK_OK);
    LOG("Pagein remap succeeded");

    cfgpack_value_t v;
    CHECK(cfgpack_get(&c2, 1, &v) == CFGPACK_OK && v.v.u64 == 77);
    LOG("a@1 = %" PRIu64 " (migrated, correct)", v.v.u64);

    CHECK(cfgpack_get(&c2, 2, &v) == CFGPACK_ERR_MISSING);
    LOG("b@2 correctly absent: CFGPACK_ERR_MISSING");

    CHECK(cfgpack_get(&c2, 3, &v) == CFGPACK_OK && v.v.u64 == 55);
    LOG("c@3 = %" PRIu64 " (default restored, correct)", v.v.u64);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 7. Decoded value overrides default
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_json_remap_decoded_overrides_default) {
    LOG_SECTION("JSON-parsed schemas: decoded value overrides default");

    static const char *v1_json =
        "{"
        "  \"name\": \"ovr\","
        "  \"version\": 1,"
        "  \"entries\": ["
        "    {\"index\": 1, \"name\": \"val\", \"type\": \"u8\","
        "     \"value\": 0}"
        "  ]"
        "}";

    /* v2 has same entry at same index but with a default of 99 */
    static const char *v2_json =
        "{"
        "  \"name\": \"ovr\","
        "  \"version\": 2,"
        "  \"entries\": ["
        "    {\"index\": 1, \"name\": \"val\", \"type\": \"u8\","
        "     \"value\": 99}"
        "  ]"
        "}";

    cfgpack_schema_t s1, s2;
    cfgpack_entry_t e1[1], e2[1];
    cfgpack_ctx_t c1, c2;
    cfgpack_value_t v1[1], v2[1];
    char sp1[1], sp2[1];
    uint16_t so1[1], so2[1];

    CHECK(json_init(v1_json, &s1, e1, 1, v1, sp1, sizeof(sp1), so1, 0, &c1) ==
          CFGPACK_OK);
    CHECK(json_init(v2_json, &s2, e2, 1, v2, sp2, sizeof(sp2), so2, 0, &c2) ==
          CFGPACK_OK);

    CHECK(cfgpack_set_u8(&c1, 1, 42) == CFGPACK_OK);
    LOG("Set v1: val@1=42 (v2 default is 99)");

    uint8_t buf[64];
    size_t len = 0;
    CHECK(cfgpack_pageout(&c1, buf, sizeof(buf), &len) == CFGPACK_OK);
    CHECK(cfgpack_pagein_remap(&c2, buf, len, NULL, 0) == CFGPACK_OK);
    LOG("Pagein remap succeeded");

    cfgpack_value_t v;
    CHECK(cfgpack_get(&c2, 1, &v) == CFGPACK_OK);
    CHECK(v.v.u64 == 42);
    LOG("val@1 = %" PRIu64 " (decoded value wins over default 99, correct)",
        v.v.u64);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 8. Remap + widening + defaults combined (JSON end-to-end)
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_json_remap_widening_with_defaults) {
    LOG_SECTION("JSON schemas: remap + type widening + defaults combined");

    static const char *v1_json =
        "{"
        "  \"name\": \"combo\","
        "  \"version\": 1,"
        "  \"entries\": ["
        "    {\"index\": 10, \"name\": \"a\", \"type\": \"u8\","
        "     \"value\": 0},"
        "    {\"index\": 11, \"name\": \"b\", \"type\": \"u8\","
        "     \"value\": 0}"
        "  ]"
        "}";

    /* v2: a widened u8->u16 and remapped 10->20, b remapped 11->21,
     * new entry c@22 with default=500 */
    static const char *v2_json =
        "{"
        "  \"name\": \"combo\","
        "  \"version\": 2,"
        "  \"entries\": ["
        "    {\"index\": 20, \"name\": \"a\", \"type\": \"u16\","
        "     \"value\": 0},"
        "    {\"index\": 21, \"name\": \"b\", \"type\": \"u8\","
        "     \"value\": 0},"
        "    {\"index\": 22, \"name\": \"c\", \"type\": \"u16\","
        "     \"value\": 500}"
        "  ]"
        "}";

    cfgpack_schema_t s1, s2;
    cfgpack_entry_t e1[2], e2[3];
    cfgpack_ctx_t c1, c2;
    cfgpack_value_t v1[2], v2[3];
    char sp1[1], sp2[1];
    uint16_t so1[1], so2[1];

    CHECK(json_init(v1_json, &s1, e1, 2, v1, sp1, sizeof(sp1), so1, 0, &c1) ==
          CFGPACK_OK);
    CHECK(json_init(v2_json, &s2, e2, 3, v2, sp2, sizeof(sp2), so2, 0, &c2) ==
          CFGPACK_OK);

    CHECK(cfgpack_set_u8(&c1, 10, 200) == CFGPACK_OK);
    CHECK(cfgpack_set_u8(&c1, 11, 150) == CFGPACK_OK);
    LOG("Set v1: a@10=200, b@11=150");

    uint8_t buf[128];
    size_t len = 0;
    CHECK(cfgpack_pageout(&c1, buf, sizeof(buf), &len) == CFGPACK_OK);
    LOG("Pageout succeeded, len=%zu", len);

    cfgpack_remap_entry_t remap[] = {{10, 20}, {11, 21}};
    CHECK(cfgpack_pagein_remap(&c2, buf, len, remap, 2) == CFGPACK_OK);
    LOG("Pagein with remap + widening succeeded");

    cfgpack_value_t v;
    CHECK(cfgpack_get(&c2, 20, &v) == CFGPACK_OK && v.v.u64 == 200);
    LOG("a@20 = %" PRIu64 " (widened u8->u16, correct)", v.v.u64);

    CHECK(cfgpack_get(&c2, 21, &v) == CFGPACK_OK && v.v.u64 == 150);
    LOG("b@21 = %" PRIu64 " (same type, correct)", v.v.u64);

    CHECK(cfgpack_get(&c2, 22, &v) == CFGPACK_OK && v.v.u64 == 500);
    LOG("c@22 = %" PRIu64 " (default restored, correct)", v.v.u64);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 9. fstr -> str widening across JSON-parsed schemas
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_json_remap_fstr_to_str_widening) {
    LOG_SECTION("JSON-parsed schemas: fstr -> str widening via remap");

    static const char *v1_json =
        "{"
        "  \"name\": \"fsw\","
        "  \"version\": 1,"
        "  \"entries\": ["
        "    {\"index\": 1, \"name\": \"nm\", \"type\": \"fstr\","
        "     \"value\": \"init\"}"
        "  ]"
        "}";

    /* v2 widens fstr -> str at same index */
    static const char *v2_json =
        "{"
        "  \"name\": \"fsw\","
        "  \"version\": 2,"
        "  \"entries\": ["
        "    {\"index\": 1, \"name\": \"nm\", \"type\": \"str\","
        "     \"value\": \"dflt\"}"
        "  ]"
        "}";

    cfgpack_schema_t s1, s2;
    cfgpack_entry_t e1[1], e2[1];
    cfgpack_ctx_t c1, c2;
    cfgpack_value_t v1[1], v2[1];
    char sp1[64], sp2[128];
    uint16_t so1[1], so2[1];

    CHECK(json_init(v1_json, &s1, e1, 1, v1, sp1, sizeof(sp1), so1, 1, &c1) ==
          CFGPACK_OK);
    CHECK(json_init(v2_json, &s2, e2, 1, v2, sp2, sizeof(sp2), so2, 1, &c2) ==
          CFGPACK_OK);

    CHECK(cfgpack_set_fstr(&c1, 1, "short") == CFGPACK_OK);
    LOG("Set v1: fstr@1='short'");

    uint8_t buf[128];
    size_t len = 0;
    CHECK(cfgpack_pageout(&c1, buf, sizeof(buf), &len) == CFGPACK_OK);
    CHECK(cfgpack_pagein_remap(&c2, buf, len, NULL, 0) == CFGPACK_OK);
    LOG("Pagein with fstr->str widening succeeded");

    const char *str_out;
    uint16_t str_len;
    CHECK(cfgpack_get_str(&c2, 1, &str_out, &str_len) == CFGPACK_OK);
    CHECK(str_len == 5 && strncmp(str_out, "short", 5) == 0);
    LOG("str@1 = '%s' len=%u (widened from fstr, correct)", str_out, str_len);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 10. String defaults restored after remap (JSON end-to-end)
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_json_remap_string_defaults_restored) {
    LOG_SECTION("JSON schemas: new str/fstr entries with defaults after remap");

    static const char *v1_json =
        "{"
        "  \"name\": \"sd\","
        "  \"version\": 1,"
        "  \"entries\": ["
        "    {\"index\": 1, \"name\": \"val\", \"type\": \"u8\", \"value\": 0}"
        "  ]"
        "}";

    /* v2 adds str@2 with default "hello" and fstr@3 with default "fw" */
    static const char *v2_json =
        "{"
        "  \"name\": \"sd\","
        "  \"version\": 2,"
        "  \"entries\": ["
        "    {\"index\": 1, \"name\": \"val\", \"type\": \"u8\", \"value\": 0},"
        "    {\"index\": 2, \"name\": \"host\", \"type\": \"str\","
        "     \"value\": \"hello\"},"
        "    {\"index\": 3, \"name\": \"fw\", \"type\": \"fstr\","
        "     \"value\": \"1.0\"}"
        "  ]"
        "}";

    cfgpack_schema_t s1, s2;
    cfgpack_entry_t e1[1], e2[3];
    cfgpack_ctx_t c1, c2;
    cfgpack_value_t v1[1], v2[3];
    char sp1[1], sp2[256];
    uint16_t so1[1], so2[2];

    CHECK(json_init(v1_json, &s1, e1, 1, v1, sp1, sizeof(sp1), so1, 0, &c1) ==
          CFGPACK_OK);
    CHECK(json_init(v2_json, &s2, e2, 3, v2, sp2, sizeof(sp2), so2, 2, &c2) ==
          CFGPACK_OK);

    CHECK(cfgpack_set_u8(&c1, 1, 77) == CFGPACK_OK);
    LOG("Set v1: val@1=77");

    uint8_t buf[128];
    size_t len = 0;
    CHECK(cfgpack_pageout(&c1, buf, sizeof(buf), &len) == CFGPACK_OK);
    CHECK(cfgpack_pagein_remap(&c2, buf, len, NULL, 0) == CFGPACK_OK);
    LOG("Pagein remap succeeded");

    cfgpack_value_t v;
    CHECK(cfgpack_get(&c2, 1, &v) == CFGPACK_OK && v.v.u64 == 77);
    LOG("val@1 = %" PRIu64 " (migrated, correct)", v.v.u64);

    /* str@2 default "hello" should be present */
    const char *str_out;
    uint16_t str_len;
    CHECK(cfgpack_get_str(&c2, 2, &str_out, &str_len) == CFGPACK_OK);
    CHECK(str_len == 5 && strncmp(str_out, "hello", 5) == 0);
    LOG("host@2 = '%s' (default restored, correct)", str_out);

    /* fstr@3 default "1.0" should be present */
    const char *fstr_out;
    uint8_t fstr_len;
    CHECK(cfgpack_get_fstr(&c2, 3, &fstr_out, &fstr_len) == CFGPACK_OK);
    CHECK(fstr_len == 3 && strncmp(fstr_out, "1.0", 3) == 0);
    LOG("fw@3 = '%s' (default restored, correct)", fstr_out);

    return TEST_OK;
}

int main(void) {
    test_result_t overall = TEST_OK;

    overall |= (test_case_result("json_remap_basic", test_json_remap_basic()) !=
                TEST_OK);
    overall |= (test_case_result("json_remap_type_widening",
                                 test_json_remap_type_widening()) != TEST_OK);
    overall |= (test_case_result("json_remap_strings",
                                 test_json_remap_strings()) != TEST_OK);
    overall |= (test_case_result("json_remap_defaults_restored",
                                 test_json_remap_defaults_restored()) !=
                TEST_OK);
    overall |= (test_case_result("json_remap_mixed_types",
                                 test_json_remap_mixed_types()) != TEST_OK);
    overall |= (test_case_result("json_remap_no_default_stays_absent",
                                 test_json_remap_no_default_stays_absent()) !=
                TEST_OK);
    overall |= (test_case_result("json_remap_decoded_overrides_default",
                                 test_json_remap_decoded_overrides_default()) !=
                TEST_OK);
    overall |= (test_case_result("json_remap_widening_with_defaults",
                                 test_json_remap_widening_with_defaults()) !=
                TEST_OK);
    overall |= (test_case_result("json_remap_fstr_to_str_widening",
                                 test_json_remap_fstr_to_str_widening()) !=
                TEST_OK);
    overall |= (test_case_result("json_remap_string_defaults_restored",
                                 test_json_remap_string_defaults_restored()) !=
                TEST_OK);

    if (overall == TEST_OK) {
        printf(COLOR_GREEN "ALL PASS" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "SOME FAIL" COLOR_RESET "\n");
    }
    return (overall);
}
