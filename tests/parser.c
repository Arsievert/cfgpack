#include "cfgpack/schema.h"
#include "cfgpack/cfgpack.h"
#include "cfgpack/io_file.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static char scratch[8192];

TEST_CASE(test_parse_ok) {
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t defaults[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    rc = cfgpack_parse_schema_file("tests/data/sample.map", &schema, entries, 128, defaults, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_OK);
    CHECK(schema.entry_count == 15);
    CHECK(schema.version == 1);
    CHECK(schema.entries[0].index == 1);  /* Index 0 is reserved for schema name */
    CHECK(schema.entries[0].type == CFGPACK_TYPE_U8);
    CHECK(schema.entries[14].index == 15);
    CHECK(schema.entries[14].type == CFGPACK_TYPE_STR);
    printf("parsed: name=%s version=%u entries=%zu\n", schema.map_name, schema.version, schema.entry_count);
    cfgpack_schema_free(&schema);
    return (TEST_OK);
}

TEST_CASE(test_parse_bad_type) {
    const char *path = "tests/data/bad_type.map";
    FILE *f;
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t defaults[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    f = fopen(path, "w");
    CHECK(f != NULL);
    fprintf(f, "demo 1\n1 foo nope NIL  # invalid type should fail\n");
    fclose(f);

    rc = cfgpack_parse_schema_file(path, &schema, entries, 128, defaults, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_ERR_INVALID_TYPE);
    return (TEST_OK);
}

TEST_CASE(test_parse_duplicate_index) {
    const char *path = "tests/data/dup_index.map";
    FILE *f;
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t defaults[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    f = fopen(path, "w");
    CHECK(f != NULL);
    fprintf(f, "demo 1\n1 foo u8 0  # first entry\n1 bar u8 0  # duplicate index\n");
    fclose(f);

    rc = cfgpack_parse_schema_file(path, &schema, entries, 128, defaults, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_ERR_DUPLICATE);
    return (TEST_OK);
}

int main(void) {
    test_result_t overall = TEST_OK;

    overall |= (test_case_result("parse_ok", test_parse_ok()) != TEST_OK);
    overall |= (test_case_result("bad_type", test_parse_bad_type()) != TEST_OK);
    overall |= (test_case_result("dup_index", test_parse_duplicate_index()) != TEST_OK);

    if (overall == TEST_OK) {
        printf(COLOR_GREEN "ALL PASS" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "SOME FAIL" COLOR_RESET "\n");
    }
    return (overall);
}
