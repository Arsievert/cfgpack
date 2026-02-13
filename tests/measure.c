/* Schema measure function tests. */

#include "cfgpack/cfgpack.h"
#include "cfgpack/io_file.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static char scratch[16384]; /* Scratch buffer for file parsing */

static test_result_t write_file(const char *path, const char *body) {
    FILE *f = fopen(path, "w");
    if (!f) {
        return (TEST_FAIL);
    }
    fputs(body, f);
    fclose(f);
    return (TEST_OK);
}

/* ─── .map format measure tests ─────────────────────────────────────────── */

TEST_CASE(test_measure_map_sample) {
    LOG_SECTION("Measure sample.map and verify against actual parse");

    const char *path = "tests/data/sample.map";
    cfgpack_schema_measure_t m;
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Measuring sample.map");
    rc = cfgpack_schema_measure_file(path, &m, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_OK);

    LOG("Measure results:");
    LOG("  entry_count  = %zu", m.entry_count);
    LOG("  str_count    = %zu", m.str_count);
    LOG("  fstr_count   = %zu", m.fstr_count);
    LOG("  str_pool_size= %zu", m.str_pool_size);

    CHECK(m.entry_count == 15);
    CHECK(m.str_count == 3);
    CHECK(m.fstr_count == 2);

    /* Verify str_pool_size calculation */
    size_t expected_pool = 3 * (CFGPACK_STR_MAX + 1) +
                           2 * (CFGPACK_FSTR_MAX + 1);
    CHECK(m.str_pool_size == expected_pool);
    LOG("  expected str_pool_size = %zu (correct)", expected_pool);

    /* Now do a real parse and compare with schema_get_sizing */
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t values[128];
    char str_pool[2048];
    uint16_t str_offsets[128];

    rc = cfgpack_parse_schema_file(path, &schema, entries, 128, values,
                                   str_pool, sizeof(str_pool), str_offsets, 128,
                                   scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_OK);

    cfgpack_schema_sizing_t sizing;
    cfgpack_schema_get_sizing(&schema, &sizing);

    LOG("Comparing measure vs get_sizing:");
    LOG("  entry_count: measure=%zu, parse=%zu", m.entry_count,
        schema.entry_count);
    LOG("  str_count:   measure=%zu, sizing=%zu", m.str_count,
        sizing.str_count);
    LOG("  fstr_count:  measure=%zu, sizing=%zu", m.fstr_count,
        sizing.fstr_count);
    LOG("  str_pool:    measure=%zu, sizing=%zu", m.str_pool_size,
        sizing.str_pool_size);

    CHECK(m.entry_count == schema.entry_count);
    CHECK(m.str_count == sizing.str_count);
    CHECK(m.fstr_count == sizing.fstr_count);
    CHECK(m.str_pool_size == sizing.str_pool_size);

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_measure_map_no_strings) {
    LOG_SECTION("Measure .map with no string entries");

    const char *path = "tests/data/measure_nostr.map";
    cfgpack_schema_measure_t m;
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(
        write_file(path, "demo 1\n1 foo u8 0\n2 bar i32 -5\n3 baz f64 1.0\n") ==
        TEST_OK);

    rc = cfgpack_schema_measure_file(path, &m, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_OK);

    CHECK(m.entry_count == 3);
    CHECK(m.str_count == 0);
    CHECK(m.fstr_count == 0);
    CHECK(m.str_pool_size == 0);
    LOG("entry_count=%zu, str_count=%zu, fstr_count=%zu, str_pool_size=%zu",
        m.entry_count, m.str_count, m.fstr_count, m.str_pool_size);

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_measure_map_invalid_type) {
    LOG_SECTION("Measure rejects invalid type in .map");

    const char *path = "tests/data/measure_badtype.map";
    cfgpack_schema_measure_t m;
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(write_file(path, "demo 1\n1 foo nope 0\n") == TEST_OK);

    rc = cfgpack_schema_measure_file(path, &m, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_ERR_INVALID_TYPE);
    LOG("Correctly rejected: CFGPACK_ERR_INVALID_TYPE");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_measure_map_reserved_index) {
    LOG_SECTION("Measure rejects index 0 in .map");

    const char *path = "tests/data/measure_idx0.map";
    cfgpack_schema_measure_t m;
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(write_file(path, "demo 1\n0 foo u8 0\n") == TEST_OK);

    rc = cfgpack_schema_measure_file(path, &m, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_ERR_RESERVED_INDEX);
    LOG("Correctly rejected: CFGPACK_ERR_RESERVED_INDEX");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_measure_map_name_too_long) {
    LOG_SECTION("Measure rejects name > 5 chars in .map");

    const char *path = "tests/data/measure_longname.map";
    cfgpack_schema_measure_t m;
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(write_file(path, "demo 1\n1 toolong u8 0\n") == TEST_OK);

    rc = cfgpack_schema_measure_file(path, &m, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_ERR_BOUNDS);
    LOG("Correctly rejected: CFGPACK_ERR_BOUNDS");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_measure_map_missing_header) {
    LOG_SECTION("Measure rejects missing header in .map");

    const char *path = "tests/data/measure_nohdr.map";
    cfgpack_schema_measure_t m;
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(write_file(path, "") == TEST_OK);

    rc = cfgpack_schema_measure_file(path, &m, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_ERR_PARSE);
    LOG("Correctly rejected: CFGPACK_ERR_PARSE");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_measure_map_missing_default) {
    LOG_SECTION("Measure rejects missing default value in .map");

    const char *path = "tests/data/measure_nodef.map";
    cfgpack_schema_measure_t m;
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(write_file(path, "demo 1\n1 foo u8\n") == TEST_OK);

    rc = cfgpack_schema_measure_file(path, &m, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_ERR_PARSE);
    LOG("Correctly rejected: CFGPACK_ERR_PARSE");

    LOG("Test completed successfully");
    return (TEST_OK);
}

/* ─── JSON format measure tests ─────────────────────────────────────────── */

TEST_CASE(test_measure_json_valid) {
    LOG_SECTION("Measure valid JSON schema");

    cfgpack_schema_measure_t m;
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    const char *json = "{\n"
                       "  \"name\": \"test\",\n"
                       "  \"version\": 1,\n"
                       "  \"entries\": [\n"
                       "    {\"index\": 1, \"name\": \"foo\", \"type\": "
                       "\"u8\", \"value\": 0},\n"
                       "    {\"index\": 2, \"name\": \"bar\", \"type\": "
                       "\"str\", \"value\": \"hi\"},\n"
                       "    {\"index\": 3, \"name\": \"baz\", \"type\": "
                       "\"fstr\", \"value\": \"yo\"},\n"
                       "    {\"index\": 4, \"name\": \"qux\", \"type\": "
                       "\"i32\", \"value\": -5}\n"
                       "  ]\n"
                       "}\n";
    size_t json_len = strlen(json);

    LOG("Measuring JSON schema");
    rc = cfgpack_schema_measure_json(json, json_len, &m, &err);
    CHECK(rc == CFGPACK_OK);

    LOG("Measure results:");
    LOG("  entry_count  = %zu", m.entry_count);
    LOG("  str_count    = %zu", m.str_count);
    LOG("  fstr_count   = %zu", m.fstr_count);
    LOG("  str_pool_size= %zu", m.str_pool_size);

    CHECK(m.entry_count == 4);
    CHECK(m.str_count == 1);
    CHECK(m.fstr_count == 1);

    size_t expected_pool = 1 * (CFGPACK_STR_MAX + 1) +
                           1 * (CFGPACK_FSTR_MAX + 1);
    CHECK(m.str_pool_size == expected_pool);
    LOG("  expected str_pool_size = %zu (correct)", expected_pool);

    /* Parse the same JSON and compare */
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[16];
    cfgpack_value_t values[16];
    char str_pool[512];
    uint16_t str_offsets[16];

    rc = cfgpack_schema_parse_json(json, json_len, &schema, entries, 16, values,
                                   str_pool, sizeof(str_pool), str_offsets, 16,
                                   &err);
    CHECK(rc == CFGPACK_OK);

    cfgpack_schema_sizing_t sizing;
    cfgpack_schema_get_sizing(&schema, &sizing);

    CHECK(m.entry_count == schema.entry_count);
    CHECK(m.str_count == sizing.str_count);
    CHECK(m.fstr_count == sizing.fstr_count);
    CHECK(m.str_pool_size == sizing.str_pool_size);
    LOG("Measure matches parse results");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_measure_json_malformed) {
    LOG_SECTION("Measure rejects malformed JSON");

    cfgpack_schema_measure_t m;
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    const char *json = "not json at all";
    rc = cfgpack_schema_measure_json(json, strlen(json), &m, &err);
    CHECK(rc == CFGPACK_ERR_PARSE);
    LOG("Correctly rejected malformed JSON: CFGPACK_ERR_PARSE");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_measure_json_reserved_index) {
    LOG_SECTION("Measure rejects index 0 in JSON");

    cfgpack_schema_measure_t m;
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    const char *json = "{\n"
                       "  \"name\": \"test\",\n"
                       "  \"version\": 1,\n"
                       "  \"entries\": [\n"
                       "    {\"index\": 0, \"name\": \"foo\", \"type\": "
                       "\"u8\", \"value\": 0}\n"
                       "  ]\n"
                       "}\n";
    rc = cfgpack_schema_measure_json(json, strlen(json), &m, &err);
    CHECK(rc == CFGPACK_ERR_RESERVED_INDEX);
    LOG("Correctly rejected: CFGPACK_ERR_RESERVED_INDEX");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_measure_json_invalid_type) {
    LOG_SECTION("Measure rejects invalid type in JSON");

    cfgpack_schema_measure_t m;
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    const char *json = "{\n"
                       "  \"name\": \"test\",\n"
                       "  \"version\": 1,\n"
                       "  \"entries\": [\n"
                       "    {\"index\": 1, \"name\": \"foo\", \"type\": "
                       "\"nope\", \"value\": 0}\n"
                       "  ]\n"
                       "}\n";
    rc = cfgpack_schema_measure_json(json, strlen(json), &m, &err);
    CHECK(rc == CFGPACK_ERR_INVALID_TYPE);
    LOG("Correctly rejected: CFGPACK_ERR_INVALID_TYPE");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_measure_json_missing_field) {
    LOG_SECTION("Measure rejects missing required field in JSON");

    cfgpack_schema_measure_t m;
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    /* Missing "version" field */
    const char *json = "{\n"
                       "  \"name\": \"test\",\n"
                       "  \"entries\": [\n"
                       "    {\"index\": 1, \"name\": \"foo\", \"type\": "
                       "\"u8\", \"value\": 0}\n"
                       "  ]\n"
                       "}\n";
    rc = cfgpack_schema_measure_json(json, strlen(json), &m, &err);
    CHECK(rc == CFGPACK_ERR_PARSE);
    LOG("Correctly rejected missing version: CFGPACK_ERR_PARSE");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_measure_json_name_too_long) {
    LOG_SECTION("Measure rejects name > 5 chars in JSON");

    cfgpack_schema_measure_t m;
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    const char *json = "{\n"
                       "  \"name\": \"test\",\n"
                       "  \"version\": 1,\n"
                       "  \"entries\": [\n"
                       "    {\"index\": 1, \"name\": \"toolong\", \"type\": "
                       "\"u8\", \"value\": 0}\n"
                       "  ]\n"
                       "}\n";
    rc = cfgpack_schema_measure_json(json, strlen(json), &m, &err);
    CHECK(rc == CFGPACK_ERR_BOUNDS);
    LOG("Correctly rejected: CFGPACK_ERR_BOUNDS");

    LOG("Test completed successfully");
    return (TEST_OK);
}

/* ─── Measure-then-parse integration test ───────────────────────────────── */

TEST_CASE(test_measure_then_parse_map) {
    LOG_SECTION("Measure then parse: .map format end-to-end");

    const char *map = "demo 1\n"
                      "1 foo u8 255\n"
                      "2 bar str \"hello\"\n"
                      "3 baz fstr \"world\"\n"
                      "4 qux i32 -42\n";
    size_t map_len = strlen(map);

    cfgpack_schema_measure_t m;
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    /* Measure */
    rc = cfgpack_schema_measure(map, map_len, &m, &err);
    CHECK(rc == CFGPACK_OK);
    CHECK(m.entry_count == 4);
    CHECK(m.str_count == 1);
    CHECK(m.fstr_count == 1);
    LOG("Measure: %zu entries, %zu str, %zu fstr, pool=%zu", m.entry_count,
        m.str_count, m.fstr_count, m.str_pool_size);

    /* Parse with exact-sized buffers */
    cfgpack_entry_t entries[4];
    cfgpack_value_t values[4];
    char str_pool[82];       /* 1*(64+1) + 1*(16+1) = 82 */
    uint16_t str_offsets[2]; /* 1 str + 1 fstr */

    CHECK(m.str_pool_size == sizeof(str_pool));

    cfgpack_schema_t schema;
    rc = cfgpack_parse_schema(map, map_len, &schema, entries, m.entry_count,
                              values, str_pool, m.str_pool_size, str_offsets,
                              m.str_count + m.fstr_count, &err);
    CHECK(rc == CFGPACK_OK);
    CHECK(schema.entry_count == 4);
    LOG("Parse succeeded with measure-sized buffers");

    /* Verify a value to be sure */
    CHECK(entries[0].index == 1);
    CHECK(values[0].v.u64 == 255);
    CHECK(entries[3].index == 4);
    CHECK(values[3].v.i64 == -42);
    LOG("Values verified: foo=255, qux=-42");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_measure_then_parse_json) {
    LOG_SECTION("Measure then parse: JSON format end-to-end");

    const char *json = "{\n"
                       "  \"name\": \"demo\",\n"
                       "  \"version\": 1,\n"
                       "  \"entries\": [\n"
                       "    {\"index\": 1, \"name\": \"foo\", \"type\": "
                       "\"u8\", \"value\": 255},\n"
                       "    {\"index\": 2, \"name\": \"bar\", \"type\": "
                       "\"str\", \"value\": \"hello\"},\n"
                       "    {\"index\": 3, \"name\": \"baz\", \"type\": "
                       "\"fstr\", \"value\": \"world\"},\n"
                       "    {\"index\": 4, \"name\": \"qux\", \"type\": "
                       "\"i32\", \"value\": -42}\n"
                       "  ]\n"
                       "}\n";
    size_t json_len = strlen(json);

    cfgpack_schema_measure_t m;
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    /* Measure */
    rc = cfgpack_schema_measure_json(json, json_len, &m, &err);
    CHECK(rc == CFGPACK_OK);
    CHECK(m.entry_count == 4);
    CHECK(m.str_count == 1);
    CHECK(m.fstr_count == 1);
    LOG("Measure: %zu entries, %zu str, %zu fstr, pool=%zu", m.entry_count,
        m.str_count, m.fstr_count, m.str_pool_size);

    /* Parse with exact-sized buffers */
    cfgpack_entry_t entries[4];
    cfgpack_value_t values[4];
    char str_pool[82];       /* 1*(64+1) + 1*(16+1) = 82 */
    uint16_t str_offsets[2]; /* 1 str + 1 fstr */

    CHECK(m.str_pool_size == sizeof(str_pool));

    cfgpack_schema_t schema;
    rc = cfgpack_schema_parse_json(json, json_len, &schema, entries,
                                   m.entry_count, values, str_pool,
                                   m.str_pool_size, str_offsets,
                                   m.str_count + m.fstr_count, &err);
    CHECK(rc == CFGPACK_OK);
    CHECK(schema.entry_count == 4);
    LOG("Parse succeeded with measure-sized buffers");

    /* Verify a value to be sure */
    CHECK(entries[0].index == 1);
    CHECK(values[0].v.u64 == 255);
    CHECK(entries[3].index == 4);
    CHECK(values[3].v.i64 == -42);
    LOG("Values verified: foo=255, qux=-42");

    LOG("Test completed successfully");
    return (TEST_OK);
}

int main(void) {
    test_result_t overall = TEST_OK;

    /* .map measure tests */
    overall |= (test_case_result("measure_map_sample",
                                 test_measure_map_sample()) != TEST_OK);
    overall |= (test_case_result("measure_map_no_strings",
                                 test_measure_map_no_strings()) != TEST_OK);
    overall |= (test_case_result("measure_map_invalid_type",
                                 test_measure_map_invalid_type()) != TEST_OK);
    overall |= (test_case_result("measure_map_reserved_index",
                                 test_measure_map_reserved_index()) != TEST_OK);
    overall |= (test_case_result("measure_map_name_too_long",
                                 test_measure_map_name_too_long()) != TEST_OK);
    overall |= (test_case_result("measure_map_missing_header",
                                 test_measure_map_missing_header()) != TEST_OK);
    overall |= (test_case_result("measure_map_missing_default",
                                 test_measure_map_missing_default()) !=
                TEST_OK);

    /* JSON measure tests */
    overall |= (test_case_result("measure_json_valid",
                                 test_measure_json_valid()) != TEST_OK);
    overall |= (test_case_result("measure_json_malformed",
                                 test_measure_json_malformed()) != TEST_OK);
    overall |= (test_case_result("measure_json_reserved_index",
                                 test_measure_json_reserved_index()) !=
                TEST_OK);
    overall |= (test_case_result("measure_json_invalid_type",
                                 test_measure_json_invalid_type()) != TEST_OK);
    overall |= (test_case_result("measure_json_missing_field",
                                 test_measure_json_missing_field()) != TEST_OK);
    overall |= (test_case_result("measure_json_name_too_long",
                                 test_measure_json_name_too_long()) != TEST_OK);

    /* Integration: measure then parse */
    overall |= (test_case_result("measure_then_parse_map",
                                 test_measure_then_parse_map()) != TEST_OK);
    overall |= (test_case_result("measure_then_parse_json",
                                 test_measure_then_parse_json()) != TEST_OK);

    if (overall == TEST_OK) {
        printf(COLOR_GREEN "ALL PASS" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "SOME FAIL" COLOR_RESET "\n");
    }
    return (overall);
}
