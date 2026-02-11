#include "cfgpack/schema.h"
#include "cfgpack/cfgpack.h"
#include "cfgpack/io_file.h"
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
    rc = cfgpack_parse_schema_file("tests/data/sample.map", &schema, entries,
                                   128, values, str_pool, sizeof(str_pool),
                                   str_offsets, 128, scratch, sizeof(scratch),
                                   &err);
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
    rc = cfgpack_parse_schema_file(path, &schema, entries, 128, values,
                                   str_pool, sizeof(str_pool), str_offsets, 128,
                                   scratch, sizeof(scratch), &err);
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
    rc = cfgpack_parse_schema_file(path, &schema, entries, 128, values,
                                   str_pool, sizeof(str_pool), str_offsets, 128,
                                   scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_ERR_DUPLICATE);
    LOG("Correctly rejected: CFGPACK_ERR_DUPLICATE");

    LOG("Test completed successfully");
    return (TEST_OK);
}

int main(void) {
    test_result_t overall = TEST_OK;

    overall |= (test_case_result("parse_ok", test_parse_ok()) != TEST_OK);
    overall |= (test_case_result("bad_type", test_parse_bad_type()) != TEST_OK);
    overall |= (test_case_result("dup_index", test_parse_duplicate_index()) !=
                TEST_OK);

    if (overall == TEST_OK) {
        printf(COLOR_GREEN "ALL PASS" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "SOME FAIL" COLOR_RESET "\n");
    }
    return (overall);
}
