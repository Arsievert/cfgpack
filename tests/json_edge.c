/* JSON parser/writer error paths and edge cases. */

#include "cfgpack/cfgpack.h"

#include "test.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

/* Helper to call cfgpack_schema_parse_json with standard buffers. */
static cfgpack_err_t parse_json(const char *json,
                                const cfgpack_parse_opts_t *opts) {
    return cfgpack_schema_parse_json(json, strlen(json), opts);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 1. Truncated/malformed JSON -> ERR_PARSE
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_json_parse_malformed) {
    LOG_SECTION("Malformed JSON returns ERR_PARSE");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t values[8];
    char str_pool[256];
    uint16_t str_offsets[4];
    cfgpack_parse_error_t err;
    cfgpack_parse_opts_t opts = {&schema,     entries,  8,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 4,        &err};

    const char *json = "{ this is not valid json at all";
    LOG("Input: '%s'", json);

    CHECK(parse_json(json, &opts) == CFGPACK_ERR_PARSE);
    LOG("Correctly returned: CFGPACK_ERR_PARSE");

    /* Also test empty string */
    LOG("Testing empty string");
    CHECK(parse_json("", &opts) == CFGPACK_ERR_PARSE);
    LOG("Empty string correctly returned: CFGPACK_ERR_PARSE");

    /* Truncated JSON */
    const char *truncated = "{\"name\": \"test\", \"version\":";
    LOG("Testing truncated JSON: '%s'", truncated);
    CHECK(parse_json(truncated, &opts) == CFGPACK_ERR_PARSE);
    LOG("Truncated JSON correctly returned: CFGPACK_ERR_PARSE");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 2. JSON missing "name" field -> ERR_PARSE
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_json_parse_missing_name) {
    LOG_SECTION("JSON missing 'name' field returns ERR_PARSE");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t values[8];
    char str_pool[256];
    uint16_t str_offsets[4];
    cfgpack_parse_error_t err;

    const char *json = "{\n"
                       "  \"version\": 1,\n"
                       "  \"entries\": [\n"
                       "    {\"index\": 1, \"name\": \"foo\", "
                       "\"type\": \"u8\", \"value\": 0}\n"
                       "  ]\n"
                       "}\n";
    LOG("Input JSON has 'version' and 'entries' but no 'name'");

    cfgpack_parse_opts_t opts = {&schema,     entries,  8,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 4,        &err};
    CHECK(parse_json(json, &opts) == CFGPACK_ERR_PARSE);
    LOG("Correctly returned: CFGPACK_ERR_PARSE");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 3. JSON missing "version" field -> ERR_PARSE
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_json_parse_missing_version) {
    LOG_SECTION("JSON missing 'version' field returns ERR_PARSE");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t values[8];
    char str_pool[256];
    uint16_t str_offsets[4];
    cfgpack_parse_error_t err;

    const char *json = "{\n"
                       "  \"name\": \"test\",\n"
                       "  \"entries\": [\n"
                       "    {\"index\": 1, \"name\": \"foo\", "
                       "\"type\": \"u8\", \"value\": 0}\n"
                       "  ]\n"
                       "}\n";
    LOG("Input JSON has 'name' and 'entries' but no 'version'");

    cfgpack_parse_opts_t opts = {&schema,     entries,  8,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 4,        &err};
    CHECK(parse_json(json, &opts) == CFGPACK_ERR_PARSE);
    LOG("Correctly returned: CFGPACK_ERR_PARSE");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 4. JSON missing "entries" array -> ERR_PARSE
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_json_parse_missing_entries) {
    LOG_SECTION("JSON missing 'entries' array returns ERR_PARSE");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t values[8];
    char str_pool[256];
    uint16_t str_offsets[4];
    cfgpack_parse_error_t err;

    const char *json = "{\n"
                       "  \"name\": \"test\",\n"
                       "  \"version\": 1\n"
                       "}\n";
    LOG("Input JSON has 'name' and 'version' but no 'entries'");

    cfgpack_parse_opts_t opts = {&schema,     entries,  8,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 4,        &err};
    CHECK(parse_json(json, &opts) == CFGPACK_ERR_PARSE);
    LOG("Correctly returned: CFGPACK_ERR_PARSE");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 5. JSON entry with invalid type string -> ERR_INVALID_TYPE
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_json_parse_bad_entry_type) {
    LOG_SECTION("JSON entry with invalid type string returns ERR_INVALID_TYPE");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t values[8];
    char str_pool[256];
    uint16_t str_offsets[4];
    cfgpack_parse_error_t err;

    const char *json = "{\n"
                       "  \"name\": \"test\",\n"
                       "  \"version\": 1,\n"
                       "  \"entries\": [\n"
                       "    {\"index\": 1, \"name\": \"foo\", "
                       "\"type\": \"bogus\", \"value\": 0}\n"
                       "  ]\n"
                       "}\n";
    LOG("Input JSON has entry with type 'bogus'");

    cfgpack_parse_opts_t opts = {&schema,     entries,  8,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 4,        &err};
    CHECK(parse_json(json, &opts) == CFGPACK_ERR_INVALID_TYPE);
    LOG("Correctly returned: CFGPACK_ERR_INVALID_TYPE");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 6. JSON with 129 entries -> ERR_BOUNDS
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_json_parse_too_many_entries) {
    LOG_SECTION("JSON with 129 entries returns ERR_BOUNDS");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t values[128];
    char str_pool[256];
    uint16_t str_offsets[4];
    cfgpack_parse_error_t err;

    /* Build a JSON string with 129 entries (exceeds max_entries=128) */
    static char json[32768];
    int pos = 0;
    pos += snprintf(json + pos, sizeof(json) - (size_t)pos,
                    "{\"name\":\"test\",\"version\":1,\"entries\":[");
    for (int i = 0; i < 129; ++i) {
        if (i > 0) {
            json[pos++] = ',';
        }
        pos += snprintf(json + pos, sizeof(json) - (size_t)pos,
                        "{\"index\":%d,\"name\":\"e%03d\","
                        "\"type\":\"u8\",\"value\":0}",
                        i + 1, i);
    }
    pos += snprintf(json + pos, sizeof(json) - (size_t)pos, "]}");
    LOG("Generated JSON with 129 entries (%d bytes)", pos);

    cfgpack_parse_opts_t opts = {&schema,     entries,  128,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 4,        &err};
    CHECK(parse_json(json, &opts) == CFGPACK_ERR_BOUNDS);
    LOG("Correctly returned: CFGPACK_ERR_BOUNDS");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 7. JSON entry with index 0 -> ERR_RESERVED_INDEX
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_json_parse_reserved_index) {
    LOG_SECTION("JSON entry with index 0 returns ERR_RESERVED_INDEX");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t values[8];
    char str_pool[256];
    uint16_t str_offsets[4];
    cfgpack_parse_error_t err;

    const char *json = "{\n"
                       "  \"name\": \"test\",\n"
                       "  \"version\": 1,\n"
                       "  \"entries\": [\n"
                       "    {\"index\": 0, \"name\": \"foo\", "
                       "\"type\": \"u8\", \"value\": 0}\n"
                       "  ]\n"
                       "}\n";
    LOG("Input JSON has entry with index 0 (reserved)");

    cfgpack_parse_opts_t opts = {&schema,     entries,  8,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 4,        &err};
    CHECK(parse_json(json, &opts) == CFGPACK_ERR_RESERVED_INDEX);
    LOG("Correctly returned: CFGPACK_ERR_RESERVED_INDEX");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 7b. JSON numeric defaults must respect the declared type's range
 *
 * Regression: JSON defaults were stored with a raw cast, so u8 accepted
 * 9999 (truncated), unsigned types accepted negative values, and a float
 * literal for an integer type silently stored garbage.  The JSON parser
 * must enforce the same per-type validation as the .map parser.
 * ═══════════════════════════════════════════════════════════════════════════ */
TEST_CASE(test_json_parse_default_range) {
    LOG_SECTION("JSON default range/type validation");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t values[8];
    char str_pool[256];
    uint16_t str_offsets[4];
    cfgpack_parse_error_t err;
    cfgpack_parse_opts_t opts = {&schema,     entries,  8,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 4,        &err};

    /* u8 default above 255 */
    const char *too_big =
        "{\"name\":\"t\",\"version\":1,\"entries\":["
        "{\"index\":1,\"name\":\"a\",\"type\":\"u8\",\"value\":9999}]}";
    CHECK(parse_json(too_big, &opts) == CFGPACK_ERR_BOUNDS);
    LOG("u8 default 9999: ERR_BOUNDS (ok)");

    /* negative default for unsigned type */
    const char *negative =
        "{\"name\":\"t\",\"version\":1,\"entries\":["
        "{\"index\":1,\"name\":\"a\",\"type\":\"u16\",\"value\":-1}]}";
    CHECK(parse_json(negative, &opts) == CFGPACK_ERR_BOUNDS);
    LOG("u16 default -1: ERR_BOUNDS (ok)");

    /* i8 default below -128 */
    const char *too_small =
        "{\"name\":\"t\",\"version\":1,\"entries\":["
        "{\"index\":1,\"name\":\"a\",\"type\":\"i8\",\"value\":-200}]}";
    CHECK(parse_json(too_small, &opts) == CFGPACK_ERR_BOUNDS);
    LOG("i8 default -200: ERR_BOUNDS (ok)");

    /* float literal for an integer type */
    const char *float_for_int =
        "{\"name\":\"t\",\"version\":1,\"entries\":["
        "{\"index\":1,\"name\":\"a\",\"type\":\"u32\",\"value\":3.5}]}";
    CHECK(parse_json(float_for_int, &opts) == CFGPACK_ERR_PARSE);
    LOG("u32 default 3.5: ERR_PARSE (ok)");

    /* boundary values still accepted */
    const char *at_bounds =
        "{\"name\":\"t\",\"version\":1,\"entries\":["
        "{\"index\":1,\"name\":\"a\",\"type\":\"u8\",\"value\":255},"
        "{\"index\":2,\"name\":\"b\",\"type\":\"i8\",\"value\":-128}]}";
    CHECK(parse_json(at_bounds, &opts) == CFGPACK_OK);
    CHECK(values[0].v.u64 == 255);
    CHECK(values[1].v.i64 == -128);
    LOG("u8 255 / i8 -128 accepted and stored exactly (ok)");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 8. cfgpack_schema_write_json with tiny output buffer -> ERR_BOUNDS
 * ═══════════════════════════════════════════════════════════════════════════ */
/*
 * Regression: a string default on a numeric entry, or a number on a str
 * entry, was accepted with has_default = 1.  The str case left the value
 * pointing at pool offset 0, another entry's string.
 */
TEST_CASE(test_json_parse_default_family_mismatch) {
    LOG_SECTION("JSON default must match the entry's type family");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t values[8];
    char str_pool[256];
    uint16_t str_offsets[4];
    cfgpack_parse_error_t err;
    cfgpack_parse_opts_t opts = {&schema,     entries,  8,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 4,        &err};

    const char *str_for_u8 =
        "{\"name\":\"t\",\"version\":1,\"entries\":["
        "{\"index\":1,\"name\":\"a\",\"type\":\"u8\",\"value\":\"hi\"}]}";
    CHECK(parse_json(str_for_u8, &opts) == CFGPACK_ERR_TYPE_MISMATCH);
    LOG("u8 with string default: ERR_TYPE_MISMATCH (%s)",
        cfgpack_parse_error_str(err.code));

    const char *num_for_str =
        "{\"name\":\"t\",\"version\":1,\"entries\":["
        "{\"index\":1,\"name\":\"s\",\"type\":\"str\",\"value\":5}]}";
    CHECK(parse_json(num_for_str, &opts) == CFGPACK_ERR_TYPE_MISMATCH);
    LOG("str with numeric default: ERR_TYPE_MISMATCH (%s)",
        cfgpack_parse_error_str(err.code));

    const char *num_for_fstr =
        "{\"name\":\"t\",\"version\":1,\"entries\":["
        "{\"index\":1,\"name\":\"f\",\"type\":\"fstr\",\"value\":1.5}]}";
    CHECK(parse_json(num_for_fstr, &opts) == CFGPACK_ERR_TYPE_MISMATCH);
    LOG("fstr with float default: ERR_TYPE_MISMATCH (%s)",
        cfgpack_parse_error_str(err.code));

    /* null is still fine for either family */
    const char *nulls =
        "{\"name\":\"t\",\"version\":1,\"entries\":["
        "{\"index\":1,\"name\":\"a\",\"type\":\"u8\",\"value\":null},"
        "{\"index\":2,\"name\":\"s\",\"type\":\"str\",\"value\":null}]}";
    CHECK(parse_json(nulls, &opts) == CFGPACK_OK);
    CHECK(entries[0].has_default == 0 && entries[1].has_default == 0);
    LOG("null defaults accepted for both families");

    return (TEST_OK);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * Helpers for the grammar tests: parse into static buffers, return rc.
 * ───────────────────────────────────────────────────────────────────────────── */

static cfgpack_schema_t g_schema;
static cfgpack_entry_t g_entries[8];
static cfgpack_value_t g_values[8];
static char g_str_pool[256];
static uint16_t g_str_offsets[4];
static cfgpack_parse_error_t g_err;

static cfgpack_err_t parse_json_g(const char *json) {
    cfgpack_parse_opts_t opts = {&g_schema,     g_entries,  8,
                                 g_values,      g_str_pool, sizeof(g_str_pool),
                                 g_str_offsets, 4,          &g_err};
    memset(&g_err, 0, sizeof(g_err));
    return (cfgpack_schema_parse_json(json, strlen(json), &opts));
}

/** @brief Wrap one entry's "value" literal in a minimal schema. */
static cfgpack_err_t parse_value(const char *type, const char *literal) {
    char json[256];
    snprintf(json, sizeof(json),
             "{\"name\":\"t\",\"version\":1,\"entries\":["
             "{\"index\":1,\"name\":\"a\",\"type\":\"%s\",\"value\":%s}]}",
             type, literal);
    return (parse_json_g(json));
}

/*
 * Regression: the number scanner accepted "-", "e5", "1." and "1e+" as 0,
 * and strtoll clamped anything above INT64_MAX, so a u64 default written
 * by cfgpack_schema_write_json did not round-trip.
 */
TEST_CASE(test_json_number_grammar) {
    LOG_SECTION("JSON numbers follow the RFC 8259 grammar");

    static const char *bad[] = {"-",  "e5",  "E5", ".",   ".5",  "1.",
                                "1e", "1e+", "+1", "01x", "0x10"};
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        CHECK(parse_value("u8", bad[i]) == CFGPACK_ERR_PARSE);
    }
    LOG("11 malformed literals rejected with ERR_PARSE");

    CHECK(parse_value("u64", "18446744073709551615") == CFGPACK_OK);
    CHECK(g_values[0].v.u64 == UINT64_MAX);
    LOG("u64 18446744073709551615 parses exactly");

    CHECK(parse_value("u64", "18446744073709551616") == CFGPACK_ERR_PARSE);
    CHECK(parse_value("i64", "9223372036854775808") == CFGPACK_ERR_BOUNDS);
    CHECK(parse_value("i64", "-9223372036854775808") == CFGPACK_OK);
    CHECK(g_values[0].v.i64 == INT64_MIN);
    CHECK(parse_value("i64", "-9223372036854775809") == CFGPACK_ERR_PARSE);
    LOG("2^64: PARSE; 2^63 on i64: BOUNDS; INT64_MIN ok; below: PARSE");

    CHECK(parse_value("f64", "1e-310") == CFGPACK_OK);
    CHECK(g_values[0].v.f64 > 0.0);
    CHECK(parse_value("f64", "1e999") == CFGPACK_ERR_PARSE);
    CHECK(parse_value("f32", "1e39") == CFGPACK_ERR_BOUNDS);
    CHECK(parse_value("f32", "-2.5e1") == CFGPACK_OK);
    CHECK(g_values[0].v.f32 == -25.0f);
    LOG("subnormal ok, 1e999 PARSE, 1e39 on f32 BOUNDS, -2.5e1 == -25");

    return (TEST_OK);
}

TEST_CASE(test_json_u64_roundtrip) {
    LOG_SECTION("u64 default above INT64_MAX survives write_json -> parse");
    char out[512];
    size_t out_len = 0;
    cfgpack_ctx_t ctx;

    CHECK(parse_value("u64", "18446744073709551615") == CFGPACK_OK);
    CHECK(cfgpack_init(&ctx, &g_schema, g_values, 8, g_str_pool,
                       sizeof(g_str_pool), g_str_offsets, 4) == CFGPACK_OK);
    CHECK(cfgpack_schema_write_json(&ctx, out, sizeof(out), &out_len, NULL) ==
          CFGPACK_OK);
    out[out_len] = '\0';
    CHECK(parse_json_g(out) == CFGPACK_OK);
    CHECK(g_values[0].v.u64 == UINT64_MAX);
    LOG("round-tripped value = %" PRIu64, g_values[0].v.u64);

    return (TEST_OK);
}

TEST_CASE(test_json_version_range) {
    LOG_SECTION("JSON version must be an integer in 0..2^32-1");
    char json[160];

    snprintf(json, sizeof(json),
             "{\"name\":\"t\",\"version\":4294967295,\"entries\":[]}");
    CHECK(parse_json_g(json) == CFGPACK_OK && g_schema.version == 4294967295u);
    snprintf(json, sizeof(json),
             "{\"name\":\"t\",\"version\":4294967296,\"entries\":[]}");
    CHECK(parse_json_g(json) == CFGPACK_ERR_BOUNDS);
    CHECK(g_err.code == CFGPACK_PARSE_VERSION_OUT_OF_RANGE);
    snprintf(json, sizeof(json),
             "{\"name\":\"t\",\"version\":-1,\"entries\":[]}");
    CHECK(parse_json_g(json) == CFGPACK_ERR_PARSE);
    snprintf(json, sizeof(json),
             "{\"name\":\"t\",\"version\":1.5,\"entries\":[]}");
    CHECK(parse_json_g(json) == CFGPACK_ERR_PARSE);
    LOG("2^32-1 ok; 2^32 BOUNDS; -1 and 1.5 PARSE");

    return (TEST_OK);
}

/*
 * Regression: \b and \f decoded to the letters b and f, unknown escapes
 * were accepted, and \u silently dropped or mangled most code points.
 */
TEST_CASE(test_json_string_escapes) {
    LOG_SECTION("JSON string escapes decode per RFC 8259, \\u as UTF-8");
    const char *sp;
    uint16_t sl;
    cfgpack_ctx_t ctx;

    CHECK(parse_value("str", "\"\\b\\f\\/\\n\\t\\r\\\\\\\"\"") == CFGPACK_OK);
    CHECK(cfgpack_init(&ctx, &g_schema, g_values, 8, g_str_pool,
                       sizeof(g_str_pool), g_str_offsets, 4) == CFGPACK_OK);
    CHECK(cfgpack_get_str(&ctx, 1, &sp, &sl) == CFGPACK_OK);
    CHECK(sl == 8 && memcmp(sp, "\b\f/\n\t\r\\\"", 8) == 0);
    LOG("all eight simple escapes decode to their control characters");

    CHECK(parse_value("str", "\"\\q\"") == CFGPACK_ERR_PARSE);
    CHECK(parse_value("str", "\"\\u0041\"") == CFGPACK_ERR_PARSE);
    CHECK(parse_value("str", "\"\\u00e9\"") == CFGPACK_ERR_PARSE);
    LOG("\\q and any \\u escape: ERR_PARSE (not supported)");

    /* A bad \u used to swallow the closing quote and run into the next
     * tokens; it must now fail cleanly instead of accepting the entry. */
    CHECK(parse_json_g("{\"name\":\"t\",\"version\":1,\"entries\":["
                       "{\"index\":1,\"name\":\"a\",\"type\":\"u8\\u\"},{"
                       "\"value\":0}]}") == CFGPACK_ERR_PARSE);
    LOG("runaway \\u no longer consumes following tokens");

    return (TEST_OK);
}

/*
 * Regression: the writer emitted map_name and entry names with wbuf_puts,
 * so a name containing a quote or backslash produced invalid JSON.
 */
TEST_CASE(test_json_writer_escapes_names) {
    LOG_SECTION("write_json escapes schema and entry names");
    char out[512];
    size_t out_len = 0;
    cfgpack_ctx_t ctx;

    CHECK(parse_json_g("{\"name\":\"a\\\"b\\\\c\",\"version\":1,\"entries\":["
                       "{\"index\":1,\"name\":\"x\\\"y\",\"type\":\"u8\","
                       "\"value\":1}]}") == CFGPACK_OK);
    CHECK(strcmp(g_schema.map_name, "a\"b\\c") == 0);
    CHECK(strcmp(g_entries[0].name, "x\"y") == 0);
    CHECK(cfgpack_init(&ctx, &g_schema, g_values, 8, g_str_pool,
                       sizeof(g_str_pool), g_str_offsets, 4) == CFGPACK_OK);
    CHECK(cfgpack_schema_write_json(&ctx, out, sizeof(out), &out_len, NULL) ==
          CFGPACK_OK);
    out[out_len] = '\0';
    CHECK(strstr(out, "\"name\": \"a\\\"b\\\\c\"") != NULL);
    LOG("written: %s", strstr(out, "\"name\""));

    CHECK(parse_json_g(out) == CFGPACK_OK);
    CHECK(strcmp(g_schema.map_name, "a\"b\\c") == 0);
    CHECK(strcmp(g_entries[0].name, "x\"y") == 0);
    LOG("re-parsed names match");

    return (TEST_OK);
}

/*
 * Regression: commas between members were optional, trailing commas were
 * accepted, and anything after the root object was ignored.
 */
TEST_CASE(test_json_structure_strict) {
    LOG_SECTION("JSON separators and end of input are enforced");

    CHECK(parse_json_g("{\"name\":\"t\" \"version\":1,\"entries\":[]}") ==
          CFGPACK_ERR_PARSE);
    CHECK(g_err.code == CFGPACK_PARSE_UNEXPECTED_TOKEN);
    LOG("missing comma between members: ERR_PARSE");

    CHECK(parse_json_g("{\"name\":\"t\",\"version\":1,\"entries\":[],}") ==
          CFGPACK_ERR_PARSE);
    CHECK(
        parse_json_g("{\"name\":\"t\",\"version\":1,\"entries\":["
                     "{\"index\":1,\"name\":\"a\",\"type\":\"u8\",\"value\":0},"
                     "]}") == CFGPACK_ERR_PARSE);
    CHECK(
        parse_json_g("{\"name\":\"t\",\"version\":1,\"entries\":["
                     "{\"index\":1,\"name\":\"a\",\"type\":\"u8\",\"value\":0,}"
                     "]}") == CFGPACK_ERR_PARSE);
    LOG("trailing commas in object, array and entry: ERR_PARSE");

    CHECK(
        parse_json_g("{\"name\":\"t\",\"version\":1,\"entries\":["
                     "{\"index\":1,\"name\":\"a\",\"type\":\"u8\",\"value\":0}"
                     "{\"index\":2,\"name\":\"b\",\"type\":\"u8\",\"value\":0}"
                     "]}") == CFGPACK_ERR_PARSE);
    LOG("missing comma between entries: ERR_PARSE");

    CHECK(parse_json_g("{\"name\":\"t\",\"version\":1,\"entries\":[]} x") ==
          CFGPACK_ERR_PARSE);
    CHECK(g_err.code == CFGPACK_PARSE_UNEXPECTED_TOKEN);
    CHECK(parse_json_g("{\"name\":\"t\",\"version\":1,\"entries\":[]}}") ==
          CFGPACK_ERR_PARSE);
    LOG("trailing garbage after root: ERR_PARSE");

    CHECK(parse_json_g("{\"name\":\"t\",\"version\":1,\"entries\":[]}\n\n ") ==
          CFGPACK_OK);
    {
        const char *padded = "{\"name\":\"t\",\"version\":1,\"entries\":[]}";
        cfgpack_parse_opts_t opts = {
            &g_schema,          g_entries,     8, g_values, g_str_pool,
            sizeof(g_str_pool), g_str_offsets, 4, &g_err};
        CHECK(cfgpack_schema_parse_json(padded, strlen(padded) + 1, &opts) ==
              CFGPACK_OK);
    }
    LOG("trailing whitespace and a trailing NUL are accepted");

    /* Line numbers now count newlines inside strings */
    CHECK(parse_json_g("{\"name\":\"a\nb\",\n\"version\":x}") ==
          CFGPACK_ERR_PARSE);
    CHECK(g_err.line == 3);
    LOG("error after a string containing a newline reports line %zu",
        g_err.line);

    return (TEST_OK);
}

TEST_CASE(test_json_writer_small_buffer) {
    LOG_SECTION("schema_write_json with tiny buffer returns ERR_BOUNDS");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[1];
    cfgpack_ctx_t ctx;
    cfgpack_value_t values[1];
    char str_pool[1];
    uint16_t str_offsets[1];

    snprintf(schema.map_name, sizeof(schema.map_name), "test");
    schema.version = 1;
    schema.entry_count = 1;
    schema.entries = entries;
    entries[0].index = 1;
    snprintf(entries[0].name, sizeof(entries[0].name), "foo");
    entries[0].type = CFGPACK_TYPE_U8;
    entries[0].has_default = 1;

    CHECK(cfgpack_init(&ctx, &schema, values, 1, str_pool, sizeof(str_pool),
                       str_offsets, 0) == CFGPACK_OK);
    LOG("Context initialized with 1 u8 entry");

    CHECK(cfgpack_set_u8(&ctx, 1, 42) == CFGPACK_OK);
    LOG("Set u8@1 = 42");

    /* Try writing to a tiny buffer (4 bytes -- way too small) */
    char out[4];
    size_t out_len = 0;
    cfgpack_parse_error_t err;
    LOG("Attempting cfgpack_schema_write_json with 4-byte buffer");
    CHECK(cfgpack_schema_write_json(&ctx, out, sizeof(out), &out_len, &err) ==
          CFGPACK_ERR_BOUNDS);
    LOG("Correctly returned: CFGPACK_ERR_BOUNDS");

    /* Verify out_len indicates the needed size */
    LOG("Reported needed size: %zu bytes", out_len);
    CHECK(out_len > sizeof(out));
    LOG("Needed size > buffer capacity (correct)");

    return TEST_OK;
}

int main(void) {
    test_result_t overall = TEST_OK;

    overall |= (test_case_result("json_parse_malformed",
                                 test_json_parse_malformed()) != TEST_OK);
    overall |= (test_case_result("json_parse_missing_name",
                                 test_json_parse_missing_name()) != TEST_OK);
    overall |= (test_case_result("json_parse_missing_version",
                                 test_json_parse_missing_version()) != TEST_OK);
    overall |= (test_case_result("json_parse_missing_entries",
                                 test_json_parse_missing_entries()) != TEST_OK);
    overall |= (test_case_result("json_parse_bad_entry_type",
                                 test_json_parse_bad_entry_type()) != TEST_OK);
    overall |= (test_case_result("json_parse_too_many_entries",
                                 test_json_parse_too_many_entries()) !=
                TEST_OK);
    overall |= (test_case_result("json_parse_reserved_index",
                                 test_json_parse_reserved_index()) != TEST_OK);
    overall |= (test_case_result("json_parse_default_family_mismatch",
                                 test_json_parse_default_family_mismatch()) !=
                TEST_OK);
    overall |= (test_case_result("json_parse_default_range",
                                 test_json_parse_default_range()) != TEST_OK);
    overall |= (test_case_result("json_number_grammar",
                                 test_json_number_grammar()) != TEST_OK);
    overall |= (test_case_result("json_u64_roundtrip",
                                 test_json_u64_roundtrip()) != TEST_OK);
    overall |= (test_case_result("json_version_range",
                                 test_json_version_range()) != TEST_OK);
    overall |= (test_case_result("json_string_escapes",
                                 test_json_string_escapes()) != TEST_OK);
    overall |= (test_case_result("json_writer_escapes_names",
                                 test_json_writer_escapes_names()) != TEST_OK);
    overall |= (test_case_result("json_structure_strict",
                                 test_json_structure_strict()) != TEST_OK);
    overall |= (test_case_result("json_writer_small_buffer",
                                 test_json_writer_small_buffer()) != TEST_OK);

    if (overall == TEST_OK) {
        printf(COLOR_GREEN "ALL PASS" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "SOME FAIL" COLOR_RESET "\n");
    }
    return (overall);
}
