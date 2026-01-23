/* Schema parser edge-case tests. */

#include "cfgpack/cfgpack.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static test_result_t write_file(const char *path, const char *body) {
    FILE *f = fopen(path, "w");
    if (!f) {
        return (TEST_FAIL);
    }
    fputs(body, f);
    fclose(f);
    return (TEST_OK);
}

TEST_CASE(test_duplicate_name) {
    const char *path = "tests/data/dup_name.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(write_file(path, "demo 1\n0 foo u8\n1 foo u8\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 128, &err);
    CHECK(rc == CFGPACK_ERR_DUPLICATE);
    return (TEST_OK);
}

TEST_CASE(test_name_too_long) {
    const char *path = "tests/data/name_long.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(write_file(path, "demo 1\n0 toolong u8\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 128, &err);
    CHECK(rc == CFGPACK_ERR_BOUNDS);
    return (TEST_OK);
}

TEST_CASE(test_index_too_large) {
    const char *path = "tests/data/index_large.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(write_file(path, "demo 1\n70000 foo u8\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 128, &err);
    CHECK(rc == CFGPACK_ERR_BOUNDS);
    return (TEST_OK);
}

TEST_CASE(test_missing_header) {
    const char *path = "tests/data/missing_header.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(write_file(path, "0 foo u8\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 128, &err);
    CHECK(rc == CFGPACK_ERR_PARSE);
    return (TEST_OK);
}

TEST_CASE(test_missing_fields) {
    const char *path = "tests/data/missing_fields.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(write_file(path, "demo 1\n0 foo\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 128, &err);
    CHECK(rc == CFGPACK_ERR_PARSE);
    return (TEST_OK);
}

TEST_CASE(test_too_many_entries) {
    const char *path = "tests/data/too_many.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    fputs("demo 1\n", f);
    for (int i = 0; i < 129; ++i) {
        fprintf(f, "%d e%d u8\n", i, i);
    }
    fclose(f);

    rc = cfgpack_parse_schema(path, &schema, entries, 128, &err);
    CHECK(rc == CFGPACK_ERR_BOUNDS);
    return (TEST_OK);
}

TEST_CASE(test_accept_128_entries) {
    const char *path = "tests/data/accept_128.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    fputs("demo 1\n", f);
    for (int i = 0; i < 128; ++i) {
        fprintf(f, "%d e%d u8\n", i, i);
    }
    fclose(f);

    rc = cfgpack_parse_schema(path, &schema, entries, 128, &err);
    CHECK(rc == CFGPACK_OK);
    CHECK(schema.entry_count == 128);
    return (TEST_OK);
}

TEST_CASE(test_unknown_type) {
    const char *path = "tests/data/unknown_type.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(write_file(path, "demo 1\n0 foo nope\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 128, &err);
    CHECK(rc == CFGPACK_ERR_INVALID_TYPE);
    return (TEST_OK);
}

TEST_CASE(test_header_non_numeric_version) {
    const char *path = "tests/data/header_non_numeric.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(write_file(path, "demo x\n0 a u8\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 8, &err);
    CHECK(rc == CFGPACK_ERR_PARSE);
    return (TEST_OK);
}

TEST_CASE(test_name_length_edges) {
    const char *ok_path = "tests/data/name_len_ok.map";
    const char *bad_path = "tests/data/name_len_bad.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(write_file(ok_path, "demo 1\n0 abcde u8\n") == TEST_OK);
    rc = cfgpack_parse_schema(ok_path, &schema, entries, 8, &err);
    CHECK(rc == CFGPACK_OK);
    CHECK(schema.entry_count == 1);

    CHECK(write_file(bad_path, "demo 1\n0 abcdef u8\n") == TEST_OK);
    rc = cfgpack_parse_schema(bad_path, &schema, entries, 8, &err);
    CHECK(rc == CFGPACK_ERR_BOUNDS);
    return (TEST_OK);
}

TEST_CASE(test_index_edges) {
    const char *ok_path = "tests/data/index_edge_ok.map";
    const char *bad_path = "tests/data/index_edge_bad.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(write_file(ok_path, "demo 1\n65535 a u8\n") == TEST_OK);
    rc = cfgpack_parse_schema(ok_path, &schema, entries, 8, &err);
    CHECK(rc == CFGPACK_OK);
    CHECK(schema.entries[0].index == 65535);

    CHECK(write_file(bad_path, "demo 1\n65536 a u8\n") == TEST_OK);
    rc = cfgpack_parse_schema(bad_path, &schema, entries, 8, &err);
    CHECK(rc == CFGPACK_ERR_BOUNDS);
    return (TEST_OK);
}

TEST_CASE(test_unsorted_input_sorted_output) {
    const char *path = "tests/data/unsorted.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[4];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(write_file(path, "demo 1\n2 c u8\n0 a u8\n1 b u8\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 4, &err);
    CHECK(rc == CFGPACK_OK);
    CHECK(schema.entry_count == 3);
    CHECK(schema.entries[0].index == 0);
    CHECK(schema.entries[1].index == 1);
    CHECK(schema.entries[2].index == 2);
    return (TEST_OK);
}

TEST_CASE(test_markdown_writer_basic) {
    const char *in_path = "tests/data/sample.map";
    const char *out_path = "build/markdown_tmp.md";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;
    FILE *f;
    char buf[64];

    rc = cfgpack_parse_schema(in_path, &schema, entries, 128, &err);
    CHECK(rc == CFGPACK_OK);
    rc = cfgpack_schema_write_markdown(&schema, out_path, &err);
    CHECK(rc == CFGPACK_OK);
    f = fopen(out_path, "r");
    CHECK(f != NULL);
    CHECK(fgets(buf, sizeof(buf), f) != NULL);
    CHECK(strncmp(buf, "# ", 2) == 0);
    fclose(f);
    return (TEST_OK);
}

int main(void) {
    test_result_t overall = TEST_OK;

    overall |= (test_case_result("dup_name", test_duplicate_name()) != TEST_OK);
    overall |= (test_case_result("name_too_long", test_name_too_long()) != TEST_OK);
    overall |= (test_case_result("index_too_large", test_index_too_large()) != TEST_OK);
    overall |= (test_case_result("missing_header", test_missing_header()) != TEST_OK);
    overall |= (test_case_result("missing_fields", test_missing_fields()) != TEST_OK);
    overall |= (test_case_result("too_many_entries", test_too_many_entries()) != TEST_OK);
    overall |= (test_case_result("accept_128_entries", test_accept_128_entries()) != TEST_OK);
    overall |= (test_case_result("unknown_type", test_unknown_type()) != TEST_OK);

    if (overall == TEST_OK) {
        printf("ALL PASS\n");
    } else {
        printf("SOME FAIL\n");
    }
    return (overall);
}
