#include "cfgpack/cfgpack.h"
#include "cfgpack/io_file.h"
#include "cfgpack/schema.h"

#include "test.h"

#include <stdio.h>
#include <string.h>

static char scratch[8192];

TEST_CASE(test_parse_ok) {
    LOG_SECTION("Parse valid .map schema file");

    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t values[128];
    char str_pool[2048];
    uint16_t str_offsets[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Parsing tests/data/sample.map");
    cfgpack_parse_opts_t opts = {&schema,     entries,  128,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 128,      &err};
    rc = cfgpack_parse_schema_file("tests/data/sample.map", &opts, scratch,
                                   sizeof(scratch));
    CHECK(rc == CFGPACK_OK);
    LOG("Parse succeeded");

    LOG("Verifying schema metadata:");
    LOG("  name = '%s'", schema.map_name);
    LOG("  version = %u", schema.version);
    LOG("  entry_count = %zu", schema.entry_count);
    CHECK(schema.entry_count == 15);
    CHECK(schema.version == 1);

    LOG("Verifying first entry:");
    LOG("  index = %u (expected: 1)", schema.entries[0].index);
    LOG("  type = %d (expected: %d = U8)", schema.entries[0].type,
        CFGPACK_TYPE_U8);
    CHECK(schema.entries[0].index ==
          1); /* Index 0 is reserved for schema name */
    CHECK(schema.entries[0].type == CFGPACK_TYPE_U8);

    LOG("Verifying last entry (index 14):");
    LOG("  index = %u (expected: 15)", schema.entries[14].index);
    LOG("  type = %d (expected: %d = STR)", schema.entries[14].type,
        CFGPACK_TYPE_STR);
    CHECK(schema.entries[14].index == 15);
    CHECK(schema.entries[14].type == CFGPACK_TYPE_STR);

    LOG("Freeing schema resources");
    cfgpack_schema_free(&schema);

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_parse_bad_type) {
    LOG_SECTION("Parse schema with invalid type name");

    const char *path = "tests/data/bad_type.map";
    FILE *f;
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t values[128];
    char str_pool[2048];
    uint16_t str_offsets[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Creating test file: %s", path);
    f = fopen(path, "w");
    CHECK(f != NULL);
    fprintf(f, "demo 1\n1 foo nope NIL  # invalid type should fail\n");
    fclose(f);
    LOG("File contents: 'demo 1\\n1 foo nope NIL'");
    LOG("  'nope' is not a valid type name");

    LOG("Parsing file (expecting CFGPACK_ERR_INVALID_TYPE)");
    cfgpack_parse_opts_t opts = {&schema,     entries,  128,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 128,      &err};
    rc = cfgpack_parse_schema_file(path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_ERR_INVALID_TYPE);
    LOG("Correctly rejected: CFGPACK_ERR_INVALID_TYPE");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_parse_duplicate_index) {
    LOG_SECTION("Parse schema with duplicate index");

    const char *path = "tests/data/dup_index.map";
    FILE *f;
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t values[128];
    char str_pool[2048];
    uint16_t str_offsets[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Creating test file: %s", path);
    f = fopen(path, "w");
    CHECK(f != NULL);
    fprintf(
        f,
        "demo 1\n1 foo u8 0  # first entry\n1 bar u8 0  # duplicate index\n");
    fclose(f);
    LOG("File contents:");
    LOG("  demo 1");
    LOG("  1 foo u8 0  # first entry");
    LOG("  1 bar u8 0  # duplicate index (same index 1)");

    LOG("Parsing file (expecting CFGPACK_ERR_DUPLICATE)");
    cfgpack_parse_opts_t opts = {&schema,     entries,  128,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 128,      &err};
    rc = cfgpack_parse_schema_file(path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_ERR_DUPLICATE);
    LOG("Correctly rejected: CFGPACK_ERR_DUPLICATE");

    LOG("Test completed successfully");
    return (TEST_OK);
}

/* ─────────────────────────────────────────────────────────────────────────────
 * In-memory .map helpers for the tokenizer strictness tests
 * ───────────────────────────────────────────────────────────────────────────── */

static cfgpack_err_t parse_map_str(const char *map,
                                   cfgpack_schema_t *schema,
                                   cfgpack_value_t *values,
                                   cfgpack_parse_error_t *err) {
    static cfgpack_entry_t entries[8];
    static char str_pool[256];
    static uint16_t str_offsets[4];
    cfgpack_parse_opts_t opts = {schema,      entries,  8,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 4,        err};
    memset(err, 0, sizeof(*err));
    return (cfgpack_parse_schema(map, strlen(map), &opts));
}

/*
 * Regression: .map integers were parsed with base 0, so "010" meant 8 and
 * "08" was an error while JSON read "010" as 10.  The entry index had no
 * end-pointer check ("5x" parsed as 5), and anything after the default
 * token was silently ignored.
 */
TEST_CASE(test_map_integer_literals) {
    LOG_SECTION(".map integer literals: decimal and 0x only, no octal");
    cfgpack_parse_error_t err;
    cfgpack_schema_t schema;
    cfgpack_value_t values[8];

    CHECK(parse_map_str("t 1\n1 a u8 010\n2 b u8 08\n3 c u16 0x10\n"
                        "4 d i8 -0x7f\n",
                        &schema, values, &err) == CFGPACK_OK);
    CHECK(values[0].v.u64 == 10);
    CHECK(values[1].v.u64 == 8);
    CHECK(values[2].v.u64 == 16);
    CHECK(values[3].v.i64 == -127);
    LOG("010 -> 10, 08 -> 8, 0x10 -> 16, -0x7f -> -127 (ok)");

    return (TEST_OK);
}

TEST_CASE(test_map_index_strict) {
    LOG_SECTION(".map entry index must be a plain decimal integer");
    cfgpack_parse_error_t err;
    cfgpack_schema_t schema;
    cfgpack_value_t values[8];

    CHECK(parse_map_str("t 1\n5x a u8 0\n", &schema, values, &err) ==
          CFGPACK_ERR_PARSE);
    CHECK(err.code == CFGPACK_PARSE_INVALID_INDEX && err.line == 2);
    LOG("index 5x: ERR_PARSE line %zu (%s)", err.line,
        cfgpack_parse_error_str(err.code));

    CHECK(parse_map_str("t 1\nabc a u8 0\n", &schema, values, &err) ==
          CFGPACK_ERR_PARSE);
    CHECK(err.code == CFGPACK_PARSE_INVALID_INDEX);
    LOG("index abc: ERR_PARSE, not 'reserved index 0' (ok)");

    CHECK(parse_map_str("t 1\n-1 a u8 0\n", &schema, values, &err) ==
          CFGPACK_ERR_PARSE);
    CHECK(parse_map_str("t 1\n65536 a u8 0\n", &schema, values, &err) ==
          CFGPACK_ERR_BOUNDS);
    CHECK(parse_map_str("t 1\n65535 a u8 0\n", &schema, values, &err) ==
          CFGPACK_OK);
    LOG("index -1: PARSE, 65536: BOUNDS, 65535: OK");

    return (TEST_OK);
}

TEST_CASE(test_map_trailing_tokens) {
    LOG_SECTION(".map lines reject tokens after the default or version");
    cfgpack_parse_error_t err;
    cfgpack_schema_t schema;
    cfgpack_value_t values[8];

    CHECK(parse_map_str("t 1 junk\n1 a u8 0\n", &schema, values, &err) ==
          CFGPACK_ERR_PARSE);
    CHECK(err.code == CFGPACK_PARSE_UNEXPECTED_TOKEN && err.line == 1);
    LOG("header 't 1 junk': ERR_PARSE line 1");

    CHECK(parse_map_str("t 1\n1 a u8 5 junk\n", &schema, values, &err) ==
          CFGPACK_ERR_PARSE);
    CHECK(err.code == CFGPACK_PARSE_UNEXPECTED_TOKEN && err.line == 2);
    CHECK(parse_map_str("t 1\n1 s str \"x\" junk\n", &schema, values, &err) ==
          CFGPACK_ERR_PARSE);
    LOG("trailing token after numeric and quoted defaults: ERR_PARSE");

    CHECK(parse_map_str("t 1 # schema\n1 a u8 5 # five\n2 s str \"x\"# x\n",
                        &schema, values, &err) == CFGPACK_OK);
    LOG("inline # comments still accepted after header and defaults");

    return (TEST_OK);
}

TEST_CASE(test_map_version_range) {
    LOG_SECTION(".map version is a 32-bit unsigned decimal");
    cfgpack_parse_error_t err;
    cfgpack_schema_t schema;
    cfgpack_value_t values[8];

    CHECK(parse_map_str("t 4294967295\n1 a u8 0\n", &schema, values, &err) ==
          CFGPACK_OK);
    CHECK(schema.version == 4294967295u);
    CHECK(parse_map_str("t 4294967296\n1 a u8 0\n", &schema, values, &err) ==
          CFGPACK_ERR_BOUNDS);
    CHECK(err.code == CFGPACK_PARSE_VERSION_OUT_OF_RANGE);
    CHECK(parse_map_str("t 99999999999999999999\n1 a u8 0\n", &schema, values,
                        &err) == CFGPACK_ERR_BOUNDS);
    CHECK(parse_map_str("t -1\n1 a u8 0\n", &schema, values, &err) ==
          CFGPACK_ERR_PARSE);
    CHECK(parse_map_str("t +1\n1 a u8 0\n", &schema, values, &err) ==
          CFGPACK_ERR_PARSE);
    LOG("2^32-1 ok; 2^32, 10^20: BOUNDS; -1, +1: PARSE");

    return (TEST_OK);
}

TEST_CASE(test_map_float_subnormal) {
    LOG_SECTION(".map float defaults: subnormals accepted, overflow rejected");
    cfgpack_parse_error_t err;
    cfgpack_schema_t schema;
    cfgpack_value_t values[8];

    CHECK(parse_map_str("t 1\n1 a f64 1e-310\n2 b f32 1e-40\n", &schema, values,
                        &err) == CFGPACK_OK);
    CHECK(values[0].v.f64 > 0.0 && values[0].v.f64 < 1e-300);
    LOG("1e-310 (f64) and 1e-40 (f32) parse as subnormals");

    CHECK(parse_map_str("t 1\n1 a f64 1e999\n", &schema, values, &err) ==
          CFGPACK_ERR_PARSE);
    CHECK(parse_map_str("t 1\n1 a f32 1e39\n", &schema, values, &err) ==
          CFGPACK_ERR_BOUNDS);
    LOG("1e999 (f64): PARSE; 1e39 (f32): BOUNDS");

    return (TEST_OK);
}

int main(void) {
    test_result_t overall = TEST_OK;

    overall |= (test_case_result("parse_ok", test_parse_ok()) != TEST_OK);
    overall |= (test_case_result("bad_type", test_parse_bad_type()) != TEST_OK);
    overall |= (test_case_result("dup_index", test_parse_duplicate_index()) !=
                TEST_OK);
    overall |= (test_case_result("map_integer_literals",
                                 test_map_integer_literals()) != TEST_OK);
    overall |= (test_case_result("map_index_strict", test_map_index_strict()) !=
                TEST_OK);
    overall |= (test_case_result("map_trailing_tokens",
                                 test_map_trailing_tokens()) != TEST_OK);
    overall |= (test_case_result("map_version_range",
                                 test_map_version_range()) != TEST_OK);
    overall |= (test_case_result("map_float_subnormal",
                                 test_map_float_subnormal()) != TEST_OK);

    if (overall == TEST_OK) {
        printf(COLOR_GREEN "ALL PASS" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "SOME FAIL" COLOR_RESET "\n");
    }
    return (overall);
}
