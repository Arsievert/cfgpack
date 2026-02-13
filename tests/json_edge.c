/* JSON parser/writer error paths and edge cases. */

#include "cfgpack/cfgpack.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

/* Helper to call cfgpack_schema_parse_json with standard buffers. */
static cfgpack_err_t parse_json(const char *json,
                                cfgpack_schema_t *schema,
                                cfgpack_entry_t *entries,
                                size_t max_entries,
                                cfgpack_value_t *values,
                                char *str_pool,
                                size_t str_pool_cap,
                                uint16_t *str_offsets,
                                size_t str_offsets_count,
                                cfgpack_parse_error_t *err) {
    return cfgpack_schema_parse_json(json, strlen(json), schema, entries,
                                     max_entries, values, str_pool,
                                     str_pool_cap, str_offsets,
                                     str_offsets_count, err);
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

    const char *json = "{ this is not valid json at all";
    LOG("Input: '%s'", json);

    CHECK(parse_json(json, &schema, entries, 8, values, str_pool,
                     sizeof(str_pool), str_offsets, 4, &err) ==
          CFGPACK_ERR_PARSE);
    LOG("Correctly returned: CFGPACK_ERR_PARSE");

    /* Also test empty string */
    LOG("Testing empty string");
    CHECK(parse_json("", &schema, entries, 8, values, str_pool,
                     sizeof(str_pool), str_offsets, 4, &err) ==
          CFGPACK_ERR_PARSE);
    LOG("Empty string correctly returned: CFGPACK_ERR_PARSE");

    /* Truncated JSON */
    const char *truncated = "{\"name\": \"test\", \"version\":";
    LOG("Testing truncated JSON: '%s'", truncated);
    CHECK(parse_json(truncated, &schema, entries, 8, values, str_pool,
                     sizeof(str_pool), str_offsets, 4, &err) ==
          CFGPACK_ERR_PARSE);
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

    CHECK(parse_json(json, &schema, entries, 8, values, str_pool,
                     sizeof(str_pool), str_offsets, 4, &err) ==
          CFGPACK_ERR_PARSE);
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

    CHECK(parse_json(json, &schema, entries, 8, values, str_pool,
                     sizeof(str_pool), str_offsets, 4, &err) ==
          CFGPACK_ERR_PARSE);
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

    CHECK(parse_json(json, &schema, entries, 8, values, str_pool,
                     sizeof(str_pool), str_offsets, 4, &err) ==
          CFGPACK_ERR_PARSE);
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

    CHECK(parse_json(json, &schema, entries, 8, values, str_pool,
                     sizeof(str_pool), str_offsets, 4, &err) ==
          CFGPACK_ERR_INVALID_TYPE);
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

    CHECK(parse_json(json, &schema, entries, 128, values, str_pool,
                     sizeof(str_pool), str_offsets, 4, &err) ==
          CFGPACK_ERR_BOUNDS);
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

    CHECK(parse_json(json, &schema, entries, 8, values, str_pool,
                     sizeof(str_pool), str_offsets, 4, &err) ==
          CFGPACK_ERR_RESERVED_INDEX);
    LOG("Correctly returned: CFGPACK_ERR_RESERVED_INDEX");

    return TEST_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * 8. cfgpack_schema_write_json with tiny output buffer -> ERR_BOUNDS
 * ═══════════════════════════════════════════════════════════════════════════ */
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
                                 test_json_parse_too_many_entries()) != TEST_OK);
    overall |= (test_case_result("json_parse_reserved_index",
                                 test_json_parse_reserved_index()) != TEST_OK);
    overall |= (test_case_result("json_writer_small_buffer",
                                 test_json_writer_small_buffer()) != TEST_OK);

    if (overall == TEST_OK) {
        printf(COLOR_GREEN "ALL PASS" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "SOME FAIL" COLOR_RESET "\n");
    }
    return (overall);
}
