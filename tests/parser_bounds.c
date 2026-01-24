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
    cfgpack_value_t defaults[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(write_file(path, "demo 1\n0 foo u8 0  # first foo\n1 foo u8 0  # duplicate name\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 128, defaults, &err);
    CHECK(rc == CFGPACK_ERR_DUPLICATE);
    return (TEST_OK);
}

TEST_CASE(test_name_too_long) {
    const char *path = "tests/data/name_long.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t defaults[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(write_file(path, "demo 1\n0 toolong u8 0  # name exceeds 5 chars\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 128, defaults, &err);
    CHECK(rc == CFGPACK_ERR_BOUNDS);
    return (TEST_OK);
}

TEST_CASE(test_index_too_large) {
    const char *path = "tests/data/index_large.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t defaults[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(write_file(path, "demo 1\n70000 foo u8 0\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 128, defaults, &err);
    CHECK(rc == CFGPACK_ERR_BOUNDS);
    return (TEST_OK);
}

TEST_CASE(test_missing_header) {
    const char *path = "tests/data/missing_header.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t defaults[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(write_file(path, "0 foo u8 0\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 128, defaults, &err);
    CHECK(rc == CFGPACK_ERR_PARSE);
    return (TEST_OK);
}

TEST_CASE(test_missing_fields) {
    const char *path = "tests/data/missing_fields.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t defaults[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(write_file(path, "demo 1\n0 foo\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 128, defaults, &err);
    CHECK(rc == CFGPACK_ERR_PARSE);
    return (TEST_OK);
}

TEST_CASE(test_too_many_entries) {
    const char *path = "tests/data/too_many.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t defaults[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    fputs("demo 1\n", f);
    for (int i = 0; i < 129; ++i) {
        fprintf(f, "%d e%d u8 0\n", i, i);
    }
    fclose(f);

    rc = cfgpack_parse_schema(path, &schema, entries, 128, defaults, &err);
    CHECK(rc == CFGPACK_ERR_BOUNDS);
    return (TEST_OK);
}

TEST_CASE(test_accept_128_entries) {
    const char *path = "tests/data/accept_128.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t defaults[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    fputs("demo 1\n", f);
    for (int i = 0; i < 128; ++i) {
        fprintf(f, "%d e%d u8 0\n", i, i);
    }
    fclose(f);

    rc = cfgpack_parse_schema(path, &schema, entries, 128, defaults, &err);
    CHECK(rc == CFGPACK_OK);
    CHECK(schema.entry_count == 128);
    return (TEST_OK);
}

TEST_CASE(test_unknown_type) {
    const char *path = "tests/data/unknown_type.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t defaults[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(write_file(path, "demo 1\n0 foo nope NIL\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 128, defaults, &err);
    CHECK(rc == CFGPACK_ERR_INVALID_TYPE);
    return (TEST_OK);
}

TEST_CASE(test_header_non_numeric_version) {
    const char *path = "tests/data/header_non_numeric.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t defaults[8];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(write_file(path, "demo x\n0 a u8 0\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 8, defaults, &err);
    CHECK(rc == CFGPACK_ERR_PARSE);
    return (TEST_OK);
}

TEST_CASE(test_name_length_edges) {
    const char *ok_path = "tests/data/name_len_ok.map";
    const char *bad_path = "tests/data/name_len_bad.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t defaults[8];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(write_file(ok_path, "demo 1\n0 abcde u8 0  # exactly 5 chars - OK\n") == TEST_OK);
    rc = cfgpack_parse_schema(ok_path, &schema, entries, 8, defaults, &err);
    CHECK(rc == CFGPACK_OK);
    CHECK(schema.entry_count == 1);

    CHECK(write_file(bad_path, "demo 1\n0 abcdef u8 0  # 6 chars - too long\n") == TEST_OK);
    rc = cfgpack_parse_schema(bad_path, &schema, entries, 8, defaults, &err);
    CHECK(rc == CFGPACK_ERR_BOUNDS);
    return (TEST_OK);
}

TEST_CASE(test_index_edges) {
    const char *ok_path = "tests/data/index_edge_ok.map";
    const char *bad_path = "tests/data/index_edge_bad.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t defaults[8];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(write_file(ok_path, "demo 1\n65535 a u8 0  # max valid index\n") == TEST_OK);
    rc = cfgpack_parse_schema(ok_path, &schema, entries, 8, defaults, &err);
    CHECK(rc == CFGPACK_OK);
    CHECK(schema.entries[0].index == 65535);

    CHECK(write_file(bad_path, "demo 1\n65536 a u8 0  # out of range\n") == TEST_OK);
    rc = cfgpack_parse_schema(bad_path, &schema, entries, 8, defaults, &err);
    CHECK(rc == CFGPACK_ERR_BOUNDS);
    return (TEST_OK);
}

TEST_CASE(test_unsorted_input_sorted_output) {
    const char *path = "tests/data/unsorted.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[4];
    cfgpack_value_t defaults[4];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    CHECK(write_file(path, "demo 1\n2 c u8 0  # third by index\n0 a u8 0  # first by index\n1 b u8 0  # second by index\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 4, defaults, &err);
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
    cfgpack_value_t defaults[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;
    FILE *f;
    char buf[64];

    rc = cfgpack_parse_schema(in_path, &schema, entries, 128, defaults, &err);
    CHECK(rc == CFGPACK_OK);
    rc = cfgpack_schema_write_markdown(&schema, defaults, out_path, &err);
    CHECK(rc == CFGPACK_OK);
    f = fopen(out_path, "r");
    CHECK(f != NULL);
    CHECK(fgets(buf, sizeof(buf), f) != NULL);
    CHECK(strncmp(buf, "# ", 2) == 0);
    fclose(f);
    return (TEST_OK);
}

TEST_CASE(test_default_u8_out_of_range) {
    const char *path = "tests/data/default_u8_oob.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t defaults[8];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    /* 256 exceeds u8 max (255) */
    CHECK(write_file(path, "demo 1\n0 foo u8 256\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 8, defaults, &err);
    CHECK(rc == CFGPACK_ERR_BOUNDS);

    /* 255 is valid max u8 */
    CHECK(write_file(path, "demo 1\n0 foo u8 255\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 8, defaults, &err);
    CHECK(rc == CFGPACK_OK);
    CHECK(defaults[0].v.u64 == 255);
    return (TEST_OK);
}

TEST_CASE(test_default_i8_out_of_range) {
    const char *path = "tests/data/default_i8_oob.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t defaults[8];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    /* 128 exceeds i8 max (127) */
    CHECK(write_file(path, "demo 1\n0 foo i8 128\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 8, defaults, &err);
    CHECK(rc == CFGPACK_ERR_BOUNDS);

    /* -129 is below i8 min (-128) */
    CHECK(write_file(path, "demo 1\n0 foo i8 -129\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 8, defaults, &err);
    CHECK(rc == CFGPACK_ERR_BOUNDS);

    /* -128 and 127 are valid */
    CHECK(write_file(path, "demo 1\n0 foo i8 -128\n1 bar i8 127\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 8, defaults, &err);
    CHECK(rc == CFGPACK_OK);
    CHECK(defaults[0].v.i64 == -128);
    CHECK(defaults[1].v.i64 == 127);
    return (TEST_OK);
}

TEST_CASE(test_default_fstr_too_long) {
    const char *path = "tests/data/default_fstr_long.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t defaults[8];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    /* fstr max is 16 chars; 17 chars should fail */
    CHECK(write_file(path, "demo 1\n0 foo fstr \"12345678901234567\"\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 8, defaults, &err);
    CHECK(rc == CFGPACK_ERR_STR_TOO_LONG);

    /* 16 chars should be OK */
    CHECK(write_file(path, "demo 1\n0 foo fstr \"1234567890123456\"\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 8, defaults, &err);
    CHECK(rc == CFGPACK_OK);
    CHECK(defaults[0].v.fstr.len == 16);
    return (TEST_OK);
}

TEST_CASE(test_default_hex_binary_literals) {
    const char *path = "tests/data/default_hex.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t defaults[8];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    /* Test hex and binary literals */
    CHECK(write_file(path, "demo 1\n0 foo u8 0xFF\n1 bar u16 0xABCD\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 8, defaults, &err);
    CHECK(rc == CFGPACK_OK);
    CHECK(defaults[0].v.u64 == 0xFF);
    CHECK(defaults[1].v.u64 == 0xABCD);

    /* Hex value out of range for u8 */
    CHECK(write_file(path, "demo 1\n0 foo u8 0x100\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 8, defaults, &err);
    CHECK(rc == CFGPACK_ERR_BOUNDS);
    return (TEST_OK);
}

TEST_CASE(test_default_invalid_format) {
    const char *path = "tests/data/default_invalid.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t defaults[8];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    /* Non-numeric value for integer type */
    CHECK(write_file(path, "demo 1\n0 foo u8 abc\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 8, defaults, &err);
    CHECK(rc == CFGPACK_ERR_PARSE);

    /* Non-quoted string for string type */
    CHECK(write_file(path, "demo 1\n0 foo str hello\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 8, defaults, &err);
    CHECK(rc == CFGPACK_ERR_PARSE);

    /* Unterminated string */
    CHECK(write_file(path, "demo 1\n0 foo str \"hello\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 8, defaults, &err);
    CHECK(rc == CFGPACK_ERR_PARSE);
    return (TEST_OK);
}

TEST_CASE(test_default_escape_sequences) {
    const char *path = "tests/data/default_escape.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t defaults[8];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    /* Test escape sequences in strings */
    CHECK(write_file(path, "demo 1\n0 foo fstr \"a\\nb\\tc\"\n") == TEST_OK);
    rc = cfgpack_parse_schema(path, &schema, entries, 8, defaults, &err);
    CHECK(rc == CFGPACK_OK);
    CHECK(defaults[0].v.fstr.len == 5);
    CHECK(defaults[0].v.fstr.data[0] == 'a');
    CHECK(defaults[0].v.fstr.data[1] == '\n');
    CHECK(defaults[0].v.fstr.data[2] == 'b');
    CHECK(defaults[0].v.fstr.data[3] == '\t');
    CHECK(defaults[0].v.fstr.data[4] == 'c');
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
    overall |= (test_case_result("header_non_numeric_version", test_header_non_numeric_version()) != TEST_OK);
    overall |= (test_case_result("name_length_edges", test_name_length_edges()) != TEST_OK);
    overall |= (test_case_result("index_edges", test_index_edges()) != TEST_OK);
    overall |= (test_case_result("unsorted_input_sorted_output", test_unsorted_input_sorted_output()) != TEST_OK);
    overall |= (test_case_result("markdown_writer_basic", test_markdown_writer_basic()) != TEST_OK);
    overall |= (test_case_result("default_u8_out_of_range", test_default_u8_out_of_range()) != TEST_OK);
    overall |= (test_case_result("default_i8_out_of_range", test_default_i8_out_of_range()) != TEST_OK);
    overall |= (test_case_result("default_fstr_too_long", test_default_fstr_too_long()) != TEST_OK);
    overall |= (test_case_result("default_hex_binary_literals", test_default_hex_binary_literals()) != TEST_OK);
    overall |= (test_case_result("default_invalid_format", test_default_invalid_format()) != TEST_OK);
    overall |= (test_case_result("default_escape_sequences", test_default_escape_sequences()) != TEST_OK);

    if (overall == TEST_OK) {
        printf(COLOR_GREEN "ALL PASS" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "SOME FAIL" COLOR_RESET "\n");
    }
    return (overall);
}
