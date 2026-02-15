/* MessagePack schema format: measure, parse, write, roundtrip, remap, and error
 * tests. Uses JSON parse -> write_msgpack to generate msgpack test data for
 * happy-path tests. Error-path tests hand-craft minimal msgpack byte arrays.
 */

#include "cfgpack/cfgpack.h"
#include "cfgpack/msgpack.h"
#include "test.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* Helper: parse JSON, init ctx, write msgpack binary. */
static cfgpack_err_t json_to_msgpack(const char *json,
                                     uint8_t *mp_out,
                                     size_t mp_cap,
                                     size_t *mp_len) {
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[32];
    cfgpack_value_t values[32];
    char str_pool[512];
    uint16_t str_offsets[16];
    cfgpack_parse_error_t perr;

    cfgpack_schema_measure_t m;
    cfgpack_err_t rc = cfgpack_schema_measure_json(json, strlen(json), &m,
                                                   &perr);
    if (rc != CFGPACK_OK) {
        return rc;
    }

    cfgpack_parse_opts_t opts = {&schema,       entries,
                                 m.entry_count, values,
                                 str_pool,      sizeof(str_pool),
                                 str_offsets,   m.str_count + m.fstr_count,
                                 &perr};
    rc = cfgpack_schema_parse_json(json, strlen(json), &opts);
    if (rc != CFGPACK_OK) {
        return rc;
    }

    cfgpack_ctx_t ctx;
    rc = cfgpack_init(&ctx, &schema, values, schema.entry_count, str_pool,
                      sizeof(str_pool), str_offsets,
                      m.str_count + m.fstr_count);
    if (rc != CFGPACK_OK) {
        return rc;
    }

    return cfgpack_schema_write_msgpack(&ctx, mp_out, mp_cap, mp_len, &perr);
}

/* Helper: parse msgpack and init a context from it. */
static cfgpack_err_t msgpack_init(const uint8_t *data,
                                  size_t data_len,
                                  const cfgpack_parse_opts_t *opts,
                                  cfgpack_ctx_t *ctx) {
    cfgpack_err_t rc = cfgpack_schema_parse_msgpack(data, data_len, opts);
    if (rc != CFGPACK_OK) {
        if (opts->err) {
            LOG("msgpack parse error: %s", opts->err->message);
        }
        return rc;
    }
    return cfgpack_init(ctx, opts->out_schema, opts->values,
                        opts->out_schema->entry_count, opts->str_pool,
                        opts->str_pool_cap, opts->str_offsets,
                        opts->str_offsets_count);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 1. Roundtrip: JSON parse -> write_msgpack -> parse_msgpack -> compare
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_roundtrip) {
    LOG_SECTION("JSON -> msgpack -> parse_msgpack roundtrip");

    static const char *json = "{"
                              "  \"name\": \"demo\","
                              "  \"version\": 3,"
                              "  \"entries\": ["
                              "    {\"index\": 5, \"name\": \"temp\", "
                              "\"type\": \"u16\", \"value\": 100},"
                              "    {\"index\": 2, \"name\": \"flag\", "
                              "\"type\": \"u8\", \"value\": 1},"
                              "    {\"index\": 10, \"name\": \"gain\", "
                              "\"type\": \"f32\", \"value\": 1.5}"
                              "  ]"
                              "}";

    uint8_t mp[512];
    size_t mp_len = 0;
    CHECK(json_to_msgpack(json, mp, sizeof(mp), &mp_len) == CFGPACK_OK);
    LOG("Wrote %zu bytes of msgpack", mp_len);

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t values[8];
    char str_pool[1];
    uint16_t str_offsets[1];
    cfgpack_parse_error_t perr;
    cfgpack_parse_opts_t opts = {&schema,     entries,  8,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 0,        &perr};
    cfgpack_ctx_t ctx;

    CHECK(msgpack_init(mp, mp_len, &opts, &ctx) == CFGPACK_OK);

    CHECK(strcmp(schema.map_name, "demo") == 0);
    LOG("name: %s", schema.map_name);
    CHECK(schema.version == 3);
    LOG("version: %u", schema.version);
    CHECK(schema.entry_count == 3);
    LOG("entry_count: %zu", schema.entry_count);

    /* Entries should be sorted by index: 2, 5, 10 */
    CHECK(entries[0].index == 2);
    CHECK(strcmp(entries[0].name, "flag") == 0);
    CHECK(entries[0].type == CFGPACK_TYPE_U8);

    CHECK(entries[1].index == 5);
    CHECK(strcmp(entries[1].name, "temp") == 0);
    CHECK(entries[1].type == CFGPACK_TYPE_U16);

    CHECK(entries[2].index == 10);
    CHECK(strcmp(entries[2].name, "gain") == 0);
    CHECK(entries[2].type == CFGPACK_TYPE_F32);

    /* Check default values */
    cfgpack_value_t v;
    CHECK(cfgpack_get(&ctx, 2, &v) == CFGPACK_OK && v.v.u64 == 1);
    LOG("flag@2 = %" PRIu64, v.v.u64);
    CHECK(cfgpack_get(&ctx, 5, &v) == CFGPACK_OK && v.v.u64 == 100);
    LOG("temp@5 = %" PRIu64, v.v.u64);
    CHECK(cfgpack_get(&ctx, 10, &v) == CFGPACK_OK &&
          fabsf(v.v.f32 - 1.5f) < 0.001f);
    LOG("gain@10 = %f", (double)v.v.f32);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 2. Measure accuracy: measure agrees with actual parse results
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_measure_accuracy) {
    LOG_SECTION("measure_msgpack agrees with actual parse");

    static const char *json =
        "{"
        "  \"name\": \"meas\","
        "  \"version\": 1,"
        "  \"entries\": ["
        "    {\"index\": 1, \"name\": \"a\", \"type\": \"u8\", \"value\": 0},"
        "    {\"index\": 2, \"name\": \"s\", \"type\": \"str\", \"value\": "
        "\"hi\"},"
        "    {\"index\": 3, \"name\": \"f\", \"type\": \"fstr\", \"value\": "
        "\"yo\"}"
        "  ]"
        "}";

    uint8_t mp[512];
    size_t mp_len = 0;
    CHECK(json_to_msgpack(json, mp, sizeof(mp), &mp_len) == CFGPACK_OK);

    cfgpack_schema_measure_t m;
    cfgpack_parse_error_t perr;
    CHECK(cfgpack_schema_measure_msgpack(mp, mp_len, &m, &perr) == CFGPACK_OK);

    LOG("entry_count=%zu str_count=%zu fstr_count=%zu pool_size=%zu",
        m.entry_count, m.str_count, m.fstr_count, m.str_pool_size);

    CHECK(m.entry_count == 3);
    CHECK(m.str_count == 1);
    CHECK(m.fstr_count == 1);
    CHECK(m.str_pool_size == (CFGPACK_STR_MAX + 1) + (CFGPACK_FSTR_MAX + 1));

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 3. All types: schema with all 12 types parsed correctly
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_all_types) {
    LOG_SECTION("All 12 types roundtrip through msgpack");

    static const char *json = "{"
                              "  \"name\": \"all\","
                              "  \"version\": 1,"
                              "  \"entries\": ["
                              "    {\"index\": 1,  \"name\": \"a\", \"type\": "
                              "\"u8\",   \"value\": 255},"
                              "    {\"index\": 2,  \"name\": \"b\", \"type\": "
                              "\"u16\",  \"value\": 1000},"
                              "    {\"index\": 3,  \"name\": \"c\", \"type\": "
                              "\"u32\",  \"value\": 70000},"
                              "    {\"index\": 4,  \"name\": \"d\", \"type\": "
                              "\"u64\",  \"value\": 100000},"
                              "    {\"index\": 5,  \"name\": \"e\", \"type\": "
                              "\"i8\",   \"value\": -1},"
                              "    {\"index\": 6,  \"name\": \"f\", \"type\": "
                              "\"i16\",  \"value\": -200},"
                              "    {\"index\": 7,  \"name\": \"g\", \"type\": "
                              "\"i32\",  \"value\": -50000},"
                              "    {\"index\": 8,  \"name\": \"h\", \"type\": "
                              "\"i64\",  \"value\": -99999},"
                              "    {\"index\": 9,  \"name\": \"i\", \"type\": "
                              "\"f32\",  \"value\": 3.14},"
                              "    {\"index\": 10, \"name\": \"j\", \"type\": "
                              "\"f64\",  \"value\": 2.718},"
                              "    {\"index\": 11, \"name\": \"k\", \"type\": "
                              "\"str\",  \"value\": \"hello\"},"
                              "    {\"index\": 12, \"name\": \"l\", \"type\": "
                              "\"fstr\", \"value\": \"xy\"}"
                              "  ]"
                              "}";

    uint8_t mp[1024];
    size_t mp_len = 0;
    CHECK(json_to_msgpack(json, mp, sizeof(mp), &mp_len) == CFGPACK_OK);
    LOG("Wrote %zu bytes of msgpack for 12-type schema", mp_len);

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[16];
    cfgpack_value_t values[16];
    size_t pool_size = (CFGPACK_STR_MAX + 1) + (CFGPACK_FSTR_MAX + 1);
    char str_pool[CFGPACK_STR_MAX + 1 + CFGPACK_FSTR_MAX + 1];
    uint16_t str_offsets[2];
    cfgpack_parse_error_t perr;
    cfgpack_parse_opts_t opts = {&schema,   entries,     16, values, str_pool,
                                 pool_size, str_offsets, 2,  &perr};
    cfgpack_ctx_t ctx;

    CHECK(msgpack_init(mp, mp_len, &opts, &ctx) == CFGPACK_OK);
    CHECK(schema.entry_count == 12);

    /* Verify types are correct */
    CHECK(entries[0].type == CFGPACK_TYPE_U8);
    CHECK(entries[1].type == CFGPACK_TYPE_U16);
    CHECK(entries[2].type == CFGPACK_TYPE_U32);
    CHECK(entries[3].type == CFGPACK_TYPE_U64);
    CHECK(entries[4].type == CFGPACK_TYPE_I8);
    CHECK(entries[5].type == CFGPACK_TYPE_I16);
    CHECK(entries[6].type == CFGPACK_TYPE_I32);
    CHECK(entries[7].type == CFGPACK_TYPE_I64);
    CHECK(entries[8].type == CFGPACK_TYPE_F32);
    CHECK(entries[9].type == CFGPACK_TYPE_F64);
    CHECK(entries[10].type == CFGPACK_TYPE_STR);
    CHECK(entries[11].type == CFGPACK_TYPE_FSTR);

    /* Spot-check some values */
    cfgpack_value_t v;
    CHECK(cfgpack_get(&ctx, 1, &v) == CFGPACK_OK && v.v.u64 == 255);
    CHECK(cfgpack_get(&ctx, 5, &v) == CFGPACK_OK && v.v.i64 == -1);
    CHECK(cfgpack_get(&ctx, 6, &v) == CFGPACK_OK && v.v.i64 == -200);
    CHECK(cfgpack_get(&ctx, 9, &v) == CFGPACK_OK &&
          fabsf(v.v.f32 - 3.14f) < 0.01f);
    LOG("All 12 types verified");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 4. String defaults: str and fstr defaults survive roundtrip
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_string_defaults) {
    LOG_SECTION("String defaults survive msgpack roundtrip");

    static const char *json = "{"
                              "  \"name\": \"strs\","
                              "  \"version\": 1,"
                              "  \"entries\": ["
                              "    {\"index\": 1, \"name\": \"s\", \"type\": "
                              "\"str\", \"value\": \"sensor-A\"},"
                              "    {\"index\": 2, \"name\": \"f\", \"type\": "
                              "\"fstr\", \"value\": \"XY\"}"
                              "  ]"
                              "}";

    uint8_t mp[512];
    size_t mp_len = 0;
    CHECK(json_to_msgpack(json, mp, sizeof(mp), &mp_len) == CFGPACK_OK);

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[4];
    cfgpack_value_t values[4];
    size_t pool_size = (CFGPACK_STR_MAX + 1) + (CFGPACK_FSTR_MAX + 1);
    char str_pool[CFGPACK_STR_MAX + 1 + CFGPACK_FSTR_MAX + 1];
    uint16_t str_offsets[2];
    cfgpack_parse_error_t perr;
    cfgpack_parse_opts_t opts = {&schema,   entries,     4, values, str_pool,
                                 pool_size, str_offsets, 2, &perr};
    cfgpack_ctx_t ctx;

    CHECK(msgpack_init(mp, mp_len, &opts, &ctx) == CFGPACK_OK);

    /* Read str default */
    const char *str_out;
    uint16_t slen;
    CHECK(cfgpack_get_str(&ctx, 1, &str_out, &slen) == CFGPACK_OK);
    CHECK(strcmp(str_out, "sensor-A") == 0);
    LOG("str@1 = \"%s\" (len=%u)", str_out, slen);

    /* Read fstr default */
    const char *fstr_out;
    uint8_t flen;
    CHECK(cfgpack_get_fstr(&ctx, 2, &fstr_out, &flen) == CFGPACK_OK);
    CHECK(strcmp(fstr_out, "XY") == 0);
    LOG("fstr@2 = \"%s\" (len=%u)", fstr_out, flen);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 5. Nil defaults: entries with nil default have has_default=0
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_nil_defaults) {
    LOG_SECTION("Nil defaults -> has_default=0");

    static const char *json =
        "{"
        "  \"name\": \"nil\","
        "  \"version\": 1,"
        "  \"entries\": ["
        "    {\"index\": 1, \"name\": \"a\", \"type\": \"u8\", \"value\": 42},"
        "    {\"index\": 2, \"name\": \"b\", \"type\": \"u8\", \"value\": null}"
        "  ]"
        "}";

    uint8_t mp[512];
    size_t mp_len = 0;
    CHECK(json_to_msgpack(json, mp, sizeof(mp), &mp_len) == CFGPACK_OK);

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[4];
    cfgpack_value_t values[4];
    char str_pool[1];
    uint16_t str_offsets[1];
    cfgpack_parse_error_t perr;

    cfgpack_parse_opts_t opts = {&schema, entries,     4, values, str_pool,
                                 0,       str_offsets, 0, &perr};
    CHECK(cfgpack_schema_parse_msgpack(mp, mp_len, &opts) == CFGPACK_OK);

    CHECK(entries[0].index == 1);
    CHECK(entries[0].has_default == 1);
    LOG("a@1 has_default=%d", entries[0].has_default);

    CHECK(entries[1].index == 2);
    CHECK(entries[1].has_default == 0);
    LOG("b@2 has_default=%d", entries[1].has_default);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 6. Remap basic: parse v1/v2 from msgpack, pageout v1, pagein_remap v2
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_remap_basic) {
    LOG_SECTION("msgpack-parsed schemas: basic remap");

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

    uint8_t mp1[512], mp2[512];
    size_t mp1_len = 0, mp2_len = 0;
    CHECK(json_to_msgpack(v1_json, mp1, sizeof(mp1), &mp1_len) == CFGPACK_OK);
    CHECK(json_to_msgpack(v2_json, mp2, sizeof(mp2), &mp2_len) == CFGPACK_OK);

    cfgpack_schema_t s1, s2;
    cfgpack_entry_t e1[4], e2[4];
    cfgpack_value_t val1[4], val2[4];
    char sp1[1], sp2[1];
    uint16_t so1[1], so2[1];
    cfgpack_parse_error_t perr1, perr2;
    cfgpack_parse_opts_t opts1 = {&s1, e1, 4, val1, sp1, 0, so1, 0, &perr1};
    cfgpack_parse_opts_t opts2 = {&s2, e2, 4, val2, sp2, 0, so2, 0, &perr2};
    cfgpack_ctx_t c1, c2;

    CHECK(msgpack_init(mp1, mp1_len, &opts1, &c1) == CFGPACK_OK);
    CHECK(msgpack_init(mp2, mp2_len, &opts2, &c2) == CFGPACK_OK);

    CHECK(cfgpack_set_u8(&c1, 10, 42) == CFGPACK_OK);
    CHECK(cfgpack_set_u8(&c1, 11, 99) == CFGPACK_OK);
    LOG("Set v1: a@10=42, b@11=99");

    uint8_t buf[128];
    size_t len = 0;
    CHECK(cfgpack_pageout(&c1, buf, sizeof(buf), &len) == CFGPACK_OK);

    cfgpack_remap_entry_t remap[] = {{10, 20}, {11, 21}};
    CHECK(cfgpack_pagein_remap(&c2, buf, len, remap, 2) == CFGPACK_OK);

    cfgpack_value_t v;
    CHECK(cfgpack_get(&c2, 20, &v) == CFGPACK_OK && v.v.u64 == 42);
    CHECK(cfgpack_get(&c2, 21, &v) == CFGPACK_OK && v.v.u64 == 99);
    LOG("Remap verified: a@20=42, b@21=99");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 7. Remap widening: type widening across msgpack-parsed schemas
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_remap_widening) {
    LOG_SECTION("msgpack-parsed schemas: u8->u16 widening via remap");

    static const char *v1_json =
        "{"
        "  \"name\": \"w\","
        "  \"version\": 1,"
        "  \"entries\": ["
        "    {\"index\": 1, \"name\": \"val\", \"type\": \"u8\", \"value\": 0}"
        "  ]"
        "}";
    static const char *v2_json =
        "{"
        "  \"name\": \"w\","
        "  \"version\": 2,"
        "  \"entries\": ["
        "    {\"index\": 1, \"name\": \"val\", \"type\": \"u16\", \"value\": 0}"
        "  ]"
        "}";

    uint8_t mp1[512], mp2[512];
    size_t mp1_len = 0, mp2_len = 0;
    CHECK(json_to_msgpack(v1_json, mp1, sizeof(mp1), &mp1_len) == CFGPACK_OK);
    CHECK(json_to_msgpack(v2_json, mp2, sizeof(mp2), &mp2_len) == CFGPACK_OK);

    cfgpack_schema_t s1, s2;
    cfgpack_entry_t e1[2], e2[2];
    cfgpack_value_t val1[2], val2[2];
    char sp1[1], sp2[1];
    uint16_t so1[1], so2[1];
    cfgpack_parse_error_t perr1, perr2;
    cfgpack_parse_opts_t opts1 = {&s1, e1, 2, val1, sp1, 0, so1, 0, &perr1};
    cfgpack_parse_opts_t opts2 = {&s2, e2, 2, val2, sp2, 0, so2, 0, &perr2};
    cfgpack_ctx_t c1, c2;

    CHECK(msgpack_init(mp1, mp1_len, &opts1, &c1) == CFGPACK_OK);
    CHECK(msgpack_init(mp2, mp2_len, &opts2, &c2) == CFGPACK_OK);

    CHECK(cfgpack_set_u8(&c1, 1, 200) == CFGPACK_OK);

    uint8_t buf[128];
    size_t len = 0;
    CHECK(cfgpack_pageout(&c1, buf, sizeof(buf), &len) == CFGPACK_OK);

    cfgpack_remap_entry_t remap[] = {{1, 1}};
    CHECK(cfgpack_pagein_remap(&c2, buf, len, remap, 1) == CFGPACK_OK);

    cfgpack_value_t v;
    CHECK(cfgpack_get(&c2, 1, &v) == CFGPACK_OK && v.v.u64 == 200);
    LOG("u8->u16 widening: val@1 = %" PRIu64, v.v.u64);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 8. Remap defaults restored: new v2 entries have defaults after remap
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_remap_defaults_restored) {
    LOG_SECTION("msgpack: new v2 entries get defaults after remap");

    static const char *v1_json =
        "{"
        "  \"name\": \"def\","
        "  \"version\": 1,"
        "  \"entries\": ["
        "    {\"index\": 1, \"name\": \"a\", \"type\": \"u8\", \"value\": 0}"
        "  ]"
        "}";
    static const char *v2_json =
        "{"
        "  \"name\": \"def\","
        "  \"version\": 2,"
        "  \"entries\": ["
        "    {\"index\": 1, \"name\": \"a\", \"type\": \"u8\", \"value\": 0},"
        "    {\"index\": 2, \"name\": \"b\", \"type\": \"u16\", \"value\": "
        "5555}"
        "  ]"
        "}";

    uint8_t mp1[512], mp2[512];
    size_t mp1_len = 0, mp2_len = 0;
    CHECK(json_to_msgpack(v1_json, mp1, sizeof(mp1), &mp1_len) == CFGPACK_OK);
    CHECK(json_to_msgpack(v2_json, mp2, sizeof(mp2), &mp2_len) == CFGPACK_OK);

    cfgpack_schema_t s1, s2;
    cfgpack_entry_t e1[2], e2[4];
    cfgpack_value_t val1[2], val2[4];
    char sp1[1], sp2[1];
    uint16_t so1[1], so2[1];
    cfgpack_parse_error_t perr1, perr2;
    cfgpack_parse_opts_t opts1 = {&s1, e1, 2, val1, sp1, 0, so1, 0, &perr1};
    cfgpack_parse_opts_t opts2 = {&s2, e2, 4, val2, sp2, 0, so2, 0, &perr2};
    cfgpack_ctx_t c1, c2;

    CHECK(msgpack_init(mp1, mp1_len, &opts1, &c1) == CFGPACK_OK);
    CHECK(msgpack_init(mp2, mp2_len, &opts2, &c2) == CFGPACK_OK);

    CHECK(cfgpack_set_u8(&c1, 1, 77) == CFGPACK_OK);

    uint8_t buf[128];
    size_t len = 0;
    CHECK(cfgpack_pageout(&c1, buf, sizeof(buf), &len) == CFGPACK_OK);

    cfgpack_remap_entry_t remap[] = {{1, 1}};
    CHECK(cfgpack_pagein_remap(&c2, buf, len, remap, 1) == CFGPACK_OK);

    cfgpack_value_t v;
    CHECK(cfgpack_get(&c2, 1, &v) == CFGPACK_OK && v.v.u64 == 77);
    LOG("a@1 = %" PRIu64 " (remapped from v1)", v.v.u64);

    CHECK(cfgpack_get(&c2, 2, &v) == CFGPACK_OK && v.v.u64 == 5555);
    LOG("b@2 = %" PRIu64 " (default restored)", v.v.u64);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 9. Error: duplicate index -> CFGPACK_ERR_DUPLICATE
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_err_duplicate_index) {
    LOG_SECTION("Duplicate index returns ERR_DUPLICATE");

    /* Hand-craft msgpack: map(3) { 0:"d", 1:1,
     * 2: [map(4){0:1,1:"a",2:0,3:0},
     *      map(4){0:1,1:"b",2:0,3:0}] } */
    uint8_t mp[256];
    cfgpack_buf_t buf;
    cfgpack_buf_init(&buf, mp, sizeof(mp));

    cfgpack_msgpack_encode_map_header(&buf, 3);
    cfgpack_msgpack_encode_uint64(&buf, 0); /* key: name */
    cfgpack_msgpack_encode_str(&buf, "d", 1);
    cfgpack_msgpack_encode_uint64(&buf, 1); /* key: version */
    cfgpack_msgpack_encode_uint64(&buf, 1);
    cfgpack_msgpack_encode_uint64(&buf, 2); /* key: entries */
    /* array(2) */
    uint8_t arr = 0x92;
    cfgpack_buf_append(&buf, &arr, 1);
    /* entry 0 */
    cfgpack_msgpack_encode_map_header(&buf, 4);
    cfgpack_msgpack_encode_uint64(&buf, 0); /* key: index */
    cfgpack_msgpack_encode_uint64(&buf, 1);
    cfgpack_msgpack_encode_uint64(&buf, 1); /* key: name */
    cfgpack_msgpack_encode_str(&buf, "a", 1);
    cfgpack_msgpack_encode_uint64(&buf, 2); /* key: type */
    cfgpack_msgpack_encode_uint64(&buf, CFGPACK_TYPE_U8);
    cfgpack_msgpack_encode_uint64(&buf, 3); /* key: value */
    cfgpack_msgpack_encode_uint64(&buf, 0);
    /* entry 1: same index */
    cfgpack_msgpack_encode_map_header(&buf, 4);
    cfgpack_msgpack_encode_uint64(&buf, 0); /* key: index */
    cfgpack_msgpack_encode_uint64(&buf, 1);
    cfgpack_msgpack_encode_uint64(&buf, 1); /* key: name */
    cfgpack_msgpack_encode_str(&buf, "b", 1);
    cfgpack_msgpack_encode_uint64(&buf, 2); /* key: type */
    cfgpack_msgpack_encode_uint64(&buf, CFGPACK_TYPE_U8);
    cfgpack_msgpack_encode_uint64(&buf, 3); /* key: value */
    cfgpack_msgpack_encode_uint64(&buf, 0);

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[4];
    cfgpack_value_t values[4];
    char str_pool[1];
    uint16_t str_offsets[1];
    cfgpack_parse_error_t perr;

    cfgpack_parse_opts_t opts = {&schema, entries,     4, values, str_pool,
                                 0,       str_offsets, 0, &perr};
    cfgpack_err_t rc = cfgpack_schema_parse_msgpack(mp, buf.len, &opts);
    CHECK(rc == CFGPACK_ERR_DUPLICATE);
    LOG("Correctly returned ERR_DUPLICATE: %s", perr.message);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 10. Error: reserved index 0 -> CFGPACK_ERR_RESERVED_INDEX
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_err_reserved_index) {
    LOG_SECTION("Index 0 returns ERR_RESERVED_INDEX");

    uint8_t mp[256];
    cfgpack_buf_t buf;
    cfgpack_buf_init(&buf, mp, sizeof(mp));

    cfgpack_msgpack_encode_map_header(&buf, 3);
    cfgpack_msgpack_encode_uint64(&buf, 0); /* key: name */
    cfgpack_msgpack_encode_str(&buf, "r", 1);
    cfgpack_msgpack_encode_uint64(&buf, 1); /* key: version */
    cfgpack_msgpack_encode_uint64(&buf, 1);
    cfgpack_msgpack_encode_uint64(&buf, 2); /* key: entries */
    uint8_t arr = 0x91;                     /* array(1) */
    cfgpack_buf_append(&buf, &arr, 1);
    cfgpack_msgpack_encode_map_header(&buf, 4);
    cfgpack_msgpack_encode_uint64(&buf, 0); /* key: index */
    cfgpack_msgpack_encode_uint64(&buf, 0); /* reserved */
    cfgpack_msgpack_encode_uint64(&buf, 1); /* key: name */
    cfgpack_msgpack_encode_str(&buf, "a", 1);
    cfgpack_msgpack_encode_uint64(&buf, 2); /* key: type */
    cfgpack_msgpack_encode_uint64(&buf, CFGPACK_TYPE_U8);
    cfgpack_msgpack_encode_uint64(&buf, 3); /* key: value */
    cfgpack_msgpack_encode_uint64(&buf, 0);

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[4];
    cfgpack_value_t values[4];
    char str_pool[1];
    uint16_t str_offsets[1];
    cfgpack_parse_error_t perr;

    cfgpack_parse_opts_t opts = {&schema, entries,     4, values, str_pool,
                                 0,       str_offsets, 0, &perr};
    cfgpack_err_t rc = cfgpack_schema_parse_msgpack(mp, buf.len, &opts);
    CHECK(rc == CFGPACK_ERR_RESERVED_INDEX);
    LOG("Correctly returned ERR_RESERVED_INDEX: %s", perr.message);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 11. Error: name too long -> ERR_BOUNDS
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_err_name_too_long) {
    LOG_SECTION("Name > 5 chars returns ERR_BOUNDS");

    uint8_t mp[256];
    cfgpack_buf_t buf;
    cfgpack_buf_init(&buf, mp, sizeof(mp));

    cfgpack_msgpack_encode_map_header(&buf, 3);
    cfgpack_msgpack_encode_uint64(&buf, 0); /* key: name */
    cfgpack_msgpack_encode_str(&buf, "n", 1);
    cfgpack_msgpack_encode_uint64(&buf, 1); /* key: version */
    cfgpack_msgpack_encode_uint64(&buf, 1);
    cfgpack_msgpack_encode_uint64(&buf, 2); /* key: entries */
    uint8_t arr = 0x91;
    cfgpack_buf_append(&buf, &arr, 1);
    cfgpack_msgpack_encode_map_header(&buf, 4);
    cfgpack_msgpack_encode_uint64(&buf, 0); /* key: index */
    cfgpack_msgpack_encode_uint64(&buf, 1);
    cfgpack_msgpack_encode_uint64(&buf, 1);        /* key: name */
    cfgpack_msgpack_encode_str(&buf, "toolng", 6); /* 6 chars > 5 */
    cfgpack_msgpack_encode_uint64(&buf, 2);        /* key: type */
    cfgpack_msgpack_encode_uint64(&buf, CFGPACK_TYPE_U8);
    cfgpack_msgpack_encode_uint64(&buf, 3); /* key: value */
    cfgpack_msgpack_encode_uint64(&buf, 0);

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[4];
    cfgpack_value_t values[4];
    char str_pool[1];
    uint16_t str_offsets[1];
    cfgpack_parse_error_t perr;

    cfgpack_parse_opts_t opts = {&schema, entries,     4, values, str_pool,
                                 0,       str_offsets, 0, &perr};
    cfgpack_err_t rc = cfgpack_schema_parse_msgpack(mp, buf.len, &opts);
    CHECK(rc == CFGPACK_ERR_BOUNDS);
    LOG("Correctly returned ERR_BOUNDS: %s", perr.message);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 12. Error: bad type string -> ERR_INVALID_TYPE
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_err_bad_type) {
    LOG_SECTION("Unknown type returns ERR_INVALID_TYPE");

    uint8_t mp[256];
    cfgpack_buf_t buf;
    cfgpack_buf_init(&buf, mp, sizeof(mp));

    cfgpack_msgpack_encode_map_header(&buf, 3);
    cfgpack_msgpack_encode_uint64(&buf, 0); /* key: name */
    cfgpack_msgpack_encode_str(&buf, "t", 1);
    cfgpack_msgpack_encode_uint64(&buf, 1); /* key: version */
    cfgpack_msgpack_encode_uint64(&buf, 1);
    cfgpack_msgpack_encode_uint64(&buf, 2); /* key: entries */
    uint8_t arr = 0x91;
    cfgpack_buf_append(&buf, &arr, 1);
    cfgpack_msgpack_encode_map_header(&buf, 4);
    cfgpack_msgpack_encode_uint64(&buf, 0); /* key: index */
    cfgpack_msgpack_encode_uint64(&buf, 1);
    cfgpack_msgpack_encode_uint64(&buf, 1); /* key: name */
    cfgpack_msgpack_encode_str(&buf, "a", 1);
    cfgpack_msgpack_encode_uint64(&buf, 2);  /* key: type */
    cfgpack_msgpack_encode_uint64(&buf, 99); /* invalid type (out of range) */
    cfgpack_msgpack_encode_uint64(&buf, 3);  /* key: value */
    cfgpack_msgpack_encode_uint64(&buf, 0);

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[4];
    cfgpack_value_t values[4];
    char str_pool[1];
    uint16_t str_offsets[1];
    cfgpack_parse_error_t perr;

    cfgpack_parse_opts_t opts = {&schema, entries,     4, values, str_pool,
                                 0,       str_offsets, 0, &perr};
    cfgpack_err_t rc = cfgpack_schema_parse_msgpack(mp, buf.len, &opts);
    CHECK(rc == CFGPACK_ERR_INVALID_TYPE);
    LOG("Correctly returned ERR_INVALID_TYPE: %s", perr.message);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 13. Error: truncated input -> ERR_DECODE
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_err_truncated) {
    LOG_SECTION("Truncated input returns ERR_DECODE");

    /* A valid msgpack that we'll truncate */
    static const char *json =
        "{"
        "  \"name\": \"tr\","
        "  \"version\": 1,"
        "  \"entries\": ["
        "    {\"index\": 1, \"name\": \"a\", \"type\": \"u8\", \"value\": 0}"
        "  ]"
        "}";

    uint8_t mp[512];
    size_t mp_len = 0;
    CHECK(json_to_msgpack(json, mp, sizeof(mp), &mp_len) == CFGPACK_OK);

    /* Truncate to half */
    size_t trunc_len = mp_len / 2;
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[4];
    cfgpack_value_t values[4];
    char str_pool[1];
    uint16_t str_offsets[1];
    cfgpack_parse_error_t perr;

    cfgpack_parse_opts_t opts = {&schema, entries,     4, values, str_pool,
                                 0,       str_offsets, 0, &perr};
    cfgpack_err_t rc = cfgpack_schema_parse_msgpack(mp, trunc_len, &opts);
    CHECK(rc != CFGPACK_OK);
    LOG("Truncated (%zu of %zu bytes) returned error: %s", trunc_len, mp_len,
        perr.message);

    /* Also test measure on truncated data */
    cfgpack_schema_measure_t m;
    rc = cfgpack_schema_measure_msgpack(mp, trunc_len, &m, &perr);
    CHECK(rc != CFGPACK_OK);
    LOG("measure_msgpack on truncated data also returned error");

    /* Empty input */
    rc = cfgpack_schema_parse_msgpack(mp, 0, &opts);
    CHECK(rc != CFGPACK_OK);
    LOG("Empty input returned error");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 14. Error: string exceeding limit -> ERR_STR_TOO_LONG
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_err_str_too_long) {
    LOG_SECTION("String exceeding limit returns ERR_STR_TOO_LONG");

    /* Build a msgpack with an fstr default > 16 chars */
    uint8_t mp[256];
    cfgpack_buf_t buf;
    cfgpack_buf_init(&buf, mp, sizeof(mp));

    cfgpack_msgpack_encode_map_header(&buf, 3);
    cfgpack_msgpack_encode_uint64(&buf, 0); /* key: name */
    cfgpack_msgpack_encode_str(&buf, "sl", 2);
    cfgpack_msgpack_encode_uint64(&buf, 1); /* key: version */
    cfgpack_msgpack_encode_uint64(&buf, 1);
    cfgpack_msgpack_encode_uint64(&buf, 2); /* key: entries */
    uint8_t arr = 0x91;
    cfgpack_buf_append(&buf, &arr, 1);
    cfgpack_msgpack_encode_map_header(&buf, 4);
    cfgpack_msgpack_encode_uint64(&buf, 0); /* key: index */
    cfgpack_msgpack_encode_uint64(&buf, 1);
    cfgpack_msgpack_encode_uint64(&buf, 1); /* key: name */
    cfgpack_msgpack_encode_str(&buf, "a", 1);
    cfgpack_msgpack_encode_uint64(&buf, 2); /* key: type */
    cfgpack_msgpack_encode_uint64(&buf, CFGPACK_TYPE_FSTR);
    cfgpack_msgpack_encode_uint64(&buf, 3); /* key: value */
    /* fstr default with 17 chars (> CFGPACK_FSTR_MAX=16) */
    cfgpack_msgpack_encode_str(&buf, "12345678901234567", 17);

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[4];
    cfgpack_value_t values[4];
    size_t pool_size = (CFGPACK_FSTR_MAX + 1);
    char str_pool[CFGPACK_FSTR_MAX + 1];
    uint16_t str_offsets[1];
    cfgpack_parse_error_t perr;

    cfgpack_parse_opts_t opts = {&schema,   entries,     4, values, str_pool,
                                 pool_size, str_offsets, 1, &perr};
    cfgpack_err_t rc = cfgpack_schema_parse_msgpack(mp, buf.len, &opts);
    CHECK(rc == CFGPACK_ERR_STR_TOO_LONG);
    LOG("Correctly returned ERR_STR_TOO_LONG: %s", perr.message);

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════ */

int main(void) {
    test_result_t overall = TEST_OK;

    overall |= (test_case_result("roundtrip", test_roundtrip()) != TEST_OK);
    overall |= (test_case_result("measure_accuracy", test_measure_accuracy()) !=
                TEST_OK);
    overall |= (test_case_result("all_types", test_all_types()) != TEST_OK);
    overall |= (test_case_result("string_defaults", test_string_defaults()) !=
                TEST_OK);
    overall |= (test_case_result("nil_defaults", test_nil_defaults()) !=
                TEST_OK);
    overall |= (test_case_result("remap_basic", test_remap_basic()) != TEST_OK);
    overall |= (test_case_result("remap_widening", test_remap_widening()) !=
                TEST_OK);
    overall |= (test_case_result("remap_defaults_restored",
                                 test_remap_defaults_restored()) != TEST_OK);
    overall |= (test_case_result("err_duplicate_index",
                                 test_err_duplicate_index()) != TEST_OK);
    overall |= (test_case_result("err_reserved_index",
                                 test_err_reserved_index()) != TEST_OK);
    overall |= (test_case_result("err_name_too_long",
                                 test_err_name_too_long()) != TEST_OK);
    overall |= (test_case_result("err_bad_type", test_err_bad_type()) !=
                TEST_OK);
    overall |= (test_case_result("err_truncated", test_err_truncated()) !=
                TEST_OK);
    overall |= (test_case_result("err_str_too_long", test_err_str_too_long()) !=
                TEST_OK);

    if (overall == TEST_OK) {
        printf(COLOR_GREEN "ALL PASS" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "SOME FAIL" COLOR_RESET "\n");
    }
    return (overall);
}
