/* Schema parser edge-case tests. */

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

TEST_CASE(test_duplicate_name) {
    LOG_SECTION("Duplicate entry name detection");

    const char *path = "tests/data/dup_name.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t values[128];
    char str_pool[2048];
    uint16_t str_offsets[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Creating test file with duplicate name 'foo'");
    CHECK(write_file(path,
                     "demo 1\n1 foo u8 0  # first foo\n2 foo u8 0  # duplicate "
                     "name\n") == TEST_OK);
    LOG("File contents:");
    LOG("  demo 1");
    LOG("  1 foo u8 0  # first foo");
    LOG("  2 foo u8 0  # duplicate name");

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

TEST_CASE(test_name_too_long) {
    LOG_SECTION("Entry name exceeds max length (5 chars)");

    const char *path = "tests/data/name_long.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t values[128];
    char str_pool[2048];
    uint16_t str_offsets[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Creating test file with 7-char name 'toolong'");
    CHECK(
        write_file(path, "demo 1\n1 toolong u8 0  # name exceeds 5 chars\n") ==
        TEST_OK);
    LOG("File contents: '1 toolong u8 0' (name is 7 chars, max is 5)");

    LOG("Parsing file (expecting CFGPACK_ERR_BOUNDS)");
    cfgpack_parse_opts_t opts = {&schema,     entries,  128,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 128,      &err};
    rc = cfgpack_parse_schema_file(path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_ERR_BOUNDS);
    LOG("Correctly rejected: CFGPACK_ERR_BOUNDS");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_index_too_large) {
    LOG_SECTION("Index exceeds uint16 max (65535)");

    const char *path = "tests/data/index_large.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t values[128];
    char str_pool[2048];
    uint16_t str_offsets[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Creating test file with index 70000 (max is 65535)");
    CHECK(write_file(path, "demo 1\n70000 foo u8 0\n") == TEST_OK);
    LOG("File contents: '70000 foo u8 0'");

    LOG("Parsing file (expecting CFGPACK_ERR_BOUNDS)");
    cfgpack_parse_opts_t opts = {&schema,     entries,  128,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 128,      &err};
    rc = cfgpack_parse_schema_file(path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_ERR_BOUNDS);
    LOG("Correctly rejected: CFGPACK_ERR_BOUNDS");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_missing_header) {
    LOG_SECTION("Schema file missing header line");

    const char *path = "tests/data/missing_header.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t values[128];
    char str_pool[2048];
    uint16_t str_offsets[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Creating test file without header (just entry line)");
    CHECK(write_file(path, "1 foo u8 0\n") == TEST_OK);
    LOG("File contents: '1 foo u8 0' (missing 'name version' header)");

    LOG("Parsing file (expecting CFGPACK_ERR_PARSE)");
    cfgpack_parse_opts_t opts = {&schema,     entries,  128,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 128,      &err};
    rc = cfgpack_parse_schema_file(path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_ERR_PARSE);
    LOG("Correctly rejected: CFGPACK_ERR_PARSE");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_missing_fields) {
    LOG_SECTION("Entry line missing required fields");

    const char *path = "tests/data/missing_fields.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t values[128];
    char str_pool[2048];
    uint16_t str_offsets[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Creating test file with incomplete entry '1 foo' (missing type and "
        "default)");
    CHECK(write_file(path, "demo 1\n1 foo\n") == TEST_OK);
    LOG("File contents:");
    LOG("  demo 1");
    LOG("  1 foo  # missing type and default");

    LOG("Parsing file (expecting CFGPACK_ERR_PARSE)");
    cfgpack_parse_opts_t opts = {&schema,     entries,  128,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 128,      &err};
    rc = cfgpack_parse_schema_file(path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_ERR_PARSE);
    LOG("Correctly rejected: CFGPACK_ERR_PARSE");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_too_many_entries) {
    LOG_SECTION("Schema exceeds max entries (128)");

    const char *path = "tests/data/too_many.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t values[128];
    char str_pool[2048];
    uint16_t str_offsets[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Creating test file with 129 entries (max is 128)");
    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    fputs("demo 1\n", f);
    for (int i = 1; i <= 129; ++i) {
        fprintf(f, "%d e%d u8 0\n", i, i);
    }
    fclose(f);
    LOG("File has header + 129 entries");

    LOG("Parsing file (expecting CFGPACK_ERR_BOUNDS)");
    cfgpack_parse_opts_t opts = {&schema,     entries,  128,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 128,      &err};
    rc = cfgpack_parse_schema_file(path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_ERR_BOUNDS);
    LOG("Correctly rejected: CFGPACK_ERR_BOUNDS");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_accept_128_entries) {
    LOG_SECTION("Schema with exactly 128 entries (max allowed)");

    const char *path = "tests/data/accept_128.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t values[128];
    char str_pool[2048];
    uint16_t str_offsets[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Creating test file with exactly 128 entries");
    FILE *f = fopen(path, "w");
    CHECK(f != NULL);
    fputs("demo 1\n", f);
    for (int i = 1; i <= 128; ++i) {
        fprintf(f, "%d e%d u8 0\n", i, i);
    }
    fclose(f);
    LOG("File has header + 128 entries");

    LOG("Parsing file (expecting success)");
    cfgpack_parse_opts_t opts = {&schema,     entries,  128,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 128,      &err};
    rc = cfgpack_parse_schema_file(path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_OK);
    LOG("Parse succeeded");

    LOG("Verifying entry count");
    CHECK(schema.entry_count == 128);
    LOG("entry_count = %zu (correct)", schema.entry_count);

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_unknown_type) {
    LOG_SECTION("Unknown type name in schema");

    const char *path = "tests/data/unknown_type.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t values[128];
    char str_pool[2048];
    uint16_t str_offsets[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Creating test file with unknown type 'nope'");
    CHECK(write_file(path, "demo 1\n1 foo nope NIL\n") == TEST_OK);
    LOG("File contents: '1 foo nope NIL'");

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

TEST_CASE(test_header_non_numeric_version) {
    LOG_SECTION("Non-numeric version in header");

    const char *path = "tests/data/header_non_numeric.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t values[8];
    char str_pool[256];
    uint16_t str_offsets[8];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Creating test file with version 'x' instead of number");
    CHECK(write_file(path, "demo x\n1 a u8 0\n") == TEST_OK);
    LOG("File contents: 'demo x' (version should be numeric)");

    LOG("Parsing file (expecting CFGPACK_ERR_PARSE)");
    cfgpack_parse_opts_t opts = {&schema,     entries,  8,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 8,        &err};
    rc = cfgpack_parse_schema_file(path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_ERR_PARSE);
    LOG("Correctly rejected: CFGPACK_ERR_PARSE");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_name_length_edges) {
    LOG_SECTION("Name length boundary testing (5 chars max)");

    const char *ok_path = "tests/data/name_len_ok.map";
    const char *bad_path = "tests/data/name_len_bad.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t values[8];
    char str_pool[256];
    uint16_t str_offsets[8];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Testing exactly 5 chars: 'abcde'");
    CHECK(
        write_file(ok_path, "demo 1\n1 abcde u8 0  # exactly 5 chars - OK\n") ==
        TEST_OK);
    cfgpack_parse_opts_t opts = {&schema,     entries,  8,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 8,        &err};
    rc = cfgpack_parse_schema_file(ok_path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_OK);
    CHECK(schema.entry_count == 1);
    LOG("5-char name accepted (correct)");

    LOG("Testing 6 chars: 'abcdef'");
    CHECK(
        write_file(bad_path, "demo 1\n1 abcdef u8 0  # 6 chars - too long\n") ==
        TEST_OK);
    rc = cfgpack_parse_schema_file(bad_path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_ERR_BOUNDS);
    LOG("6-char name rejected: CFGPACK_ERR_BOUNDS (correct)");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_index_edges) {
    LOG_SECTION("Index boundary testing (1-65535 valid)");

    const char *ok_path = "tests/data/index_edge_ok.map";
    const char *bad_path = "tests/data/index_edge_bad.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t values[8];
    char str_pool[256];
    uint16_t str_offsets[8];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Testing max valid index: 65535");
    CHECK(write_file(ok_path, "demo 1\n65535 a u8 0  # max valid index\n") ==
          TEST_OK);
    cfgpack_parse_opts_t opts = {&schema,     entries,  8,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 8,        &err};
    rc = cfgpack_parse_schema_file(ok_path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_OK);
    CHECK(schema.entries[0].index == 65535);
    LOG("Index 65535 accepted, entries[0].index = %u (correct)",
        schema.entries[0].index);

    LOG("Testing out of range: 65536");
    CHECK(write_file(bad_path, "demo 1\n65536 a u8 0  # out of range\n") ==
          TEST_OK);
    rc = cfgpack_parse_schema_file(bad_path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_ERR_BOUNDS);
    LOG("Index 65536 rejected: CFGPACK_ERR_BOUNDS (correct)");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_unsorted_input_sorted_output) {
    LOG_SECTION("Unsorted input yields sorted output");

    const char *path = "tests/data/unsorted.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[4];
    cfgpack_value_t values[4];
    char str_pool[256];
    uint16_t str_offsets[4];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Creating test file with unsorted indices: 3, 1, 2");
    CHECK(write_file(path,
                     "demo 1\n3 c u8 0  # third by index\n1 a u8 0  # first by "
                     "index\n2 b u8 0  # second by index\n") == TEST_OK);
    LOG("File order: index 3, then 1, then 2");

    LOG("Parsing file");
    cfgpack_parse_opts_t opts = {&schema,     entries,  4,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 4,        &err};
    rc = cfgpack_parse_schema_file(path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_OK);
    LOG("Parse succeeded");

    LOG("Verifying entries are sorted by index:");
    CHECK(schema.entry_count == 3);
    CHECK(schema.entries[0].index == 1);
    LOG("  entries[0].index = %u (expected 1)", schema.entries[0].index);
    CHECK(schema.entries[1].index == 2);
    LOG("  entries[1].index = %u (expected 2)", schema.entries[1].index);
    CHECK(schema.entries[2].index == 3);
    LOG("  entries[2].index = %u (expected 3)", schema.entries[2].index);

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_default_u8_out_of_range) {
    LOG_SECTION("Default value out of range for u8");

    const char *path = "tests/data/default_u8_oob.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t values[8];
    char str_pool[256];
    uint16_t str_offsets[8];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Testing u8 default = 256 (max is 255)");
    CHECK(write_file(path, "demo 1\n1 foo u8 256\n") == TEST_OK);
    cfgpack_parse_opts_t opts = {&schema,     entries,  8,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 8,        &err};
    rc = cfgpack_parse_schema_file(path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_ERR_BOUNDS);
    LOG("256 rejected: CFGPACK_ERR_BOUNDS (correct)");

    LOG("Testing u8 default = 255 (max valid)");
    CHECK(write_file(path, "demo 1\n1 foo u8 255\n") == TEST_OK);
    rc = cfgpack_parse_schema_file(path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_OK);
    CHECK(values[0].v.u64 == 255);
    LOG("255 accepted, values[0].v.u64 = %" PRIu64 " (correct)",
        values[0].v.u64);

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_default_i8_out_of_range) {
    LOG_SECTION("Default value out of range for i8");

    const char *path = "tests/data/default_i8_oob.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t values[8];
    char str_pool[256];
    uint16_t str_offsets[8];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Testing i8 default = 128 (max is 127)");
    CHECK(write_file(path, "demo 1\n1 foo i8 128\n") == TEST_OK);
    cfgpack_parse_opts_t opts = {&schema,     entries,  8,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 8,        &err};
    rc = cfgpack_parse_schema_file(path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_ERR_BOUNDS);
    LOG("128 rejected: CFGPACK_ERR_BOUNDS (correct)");

    LOG("Testing i8 default = -129 (min is -128)");
    CHECK(write_file(path, "demo 1\n1 foo i8 -129\n") == TEST_OK);
    rc = cfgpack_parse_schema_file(path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_ERR_BOUNDS);
    LOG("-129 rejected: CFGPACK_ERR_BOUNDS (correct)");

    LOG("Testing valid edge values: -128 and 127");
    CHECK(write_file(path, "demo 1\n1 foo i8 -128\n2 bar i8 127\n") == TEST_OK);
    rc = cfgpack_parse_schema_file(path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_OK);
    CHECK(values[0].v.i64 == -128);
    CHECK(values[1].v.i64 == 127);
    LOG("-128 and 127 accepted:");
    LOG("  values[0].v.i64 = %" PRId64, values[0].v.i64);
    LOG("  values[1].v.i64 = %" PRId64, values[1].v.i64);

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_default_fstr_too_long) {
    LOG_SECTION("Default fstr value exceeds max length (16 chars)");

    const char *path = "tests/data/default_fstr_long.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t values[8];
    char str_pool[256];
    uint16_t str_offsets[8];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Testing fstr default with 17 chars (max is 16)");
    CHECK(write_file(path, "demo 1\n1 foo fstr \"12345678901234567\"\n") ==
          TEST_OK);
    cfgpack_parse_opts_t opts = {&schema,     entries,  8,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 8,        &err};
    rc = cfgpack_parse_schema_file(path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_ERR_STR_TOO_LONG);
    LOG("17-char string rejected: CFGPACK_ERR_STR_TOO_LONG (correct)");

    LOG("Testing fstr default with exactly 16 chars");
    CHECK(write_file(path, "demo 1\n1 foo fstr \"1234567890123456\"\n") ==
          TEST_OK);
    rc = cfgpack_parse_schema_file(path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_OK);
    CHECK(values[0].v.fstr.len == 16);
    LOG("16-char string accepted, len = %u (correct)", values[0].v.fstr.len);

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_default_hex_binary_literals) {
    LOG_SECTION("Hex literal default values");

    const char *path = "tests/data/default_hex.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t values[8];
    char str_pool[256];
    uint16_t str_offsets[8];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Testing hex defaults: u8=0xFF, u16=0xABCD");
    CHECK(write_file(path, "demo 1\n1 foo u8 0xFF\n2 bar u16 0xABCD\n") ==
          TEST_OK);
    cfgpack_parse_opts_t opts = {&schema,     entries,  8,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 8,        &err};
    rc = cfgpack_parse_schema_file(path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_OK);
    CHECK(values[0].v.u64 == 0xFF);
    CHECK(values[1].v.u64 == 0xABCD);
    LOG("values[0].v.u64 = 0x%02" PRIX64 " (0xFF)", values[0].v.u64);
    LOG("values[1].v.u64 = 0x%04" PRIX64 " (0xABCD)", values[1].v.u64);

    LOG("Testing hex out of range: u8=0x100 (max 0xFF)");
    CHECK(write_file(path, "demo 1\n1 foo u8 0x100\n") == TEST_OK);
    rc = cfgpack_parse_schema_file(path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_ERR_BOUNDS);
    LOG("0x100 rejected for u8: CFGPACK_ERR_BOUNDS (correct)");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_default_invalid_format) {
    LOG_SECTION("Invalid default value formats");

    const char *path = "tests/data/default_invalid.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t values[8];
    char str_pool[256];
    uint16_t str_offsets[8];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Testing non-numeric value 'abc' for integer type");
    CHECK(write_file(path, "demo 1\n1 foo u8 abc\n") == TEST_OK);
    cfgpack_parse_opts_t opts = {&schema,     entries,  8,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 8,        &err};
    rc = cfgpack_parse_schema_file(path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_ERR_PARSE);
    LOG("'abc' rejected for u8: CFGPACK_ERR_PARSE (correct)");

    LOG("Testing non-quoted string 'hello' for str type");
    CHECK(write_file(path, "demo 1\n1 foo str hello\n") == TEST_OK);
    rc = cfgpack_parse_schema_file(path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_ERR_PARSE);
    LOG("Unquoted string rejected: CFGPACK_ERR_PARSE (correct)");

    LOG("Testing unterminated string");
    CHECK(write_file(path, "demo 1\n1 foo str \"hello\n") == TEST_OK);
    rc = cfgpack_parse_schema_file(path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_ERR_PARSE);
    LOG("Unterminated string rejected: CFGPACK_ERR_PARSE (correct)");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_default_escape_sequences) {
    LOG_SECTION("Escape sequences in string defaults");

    const char *path = "tests/data/default_escape.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t values[8];
    char str_pool[256];
    uint16_t str_offsets[8];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Testing escape sequences: \\n, \\t in string");
    CHECK(write_file(path, "demo 1\n1 foo fstr \"a\\nb\\tc\"\n") == TEST_OK);
    cfgpack_parse_opts_t opts = {&schema,     entries,  8,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 8,        &err};
    rc = cfgpack_parse_schema_file(path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_OK);
    LOG("Parse succeeded");

    LOG("Verifying parsed string content:");
    CHECK(values[0].v.fstr.len == 5);
    LOG("  len = %u (expected 5: 'a' + LF + 'b' + TAB + 'c')",
        values[0].v.fstr.len);
    const char *fstr_data = str_pool + values[0].v.fstr.offset;
    CHECK(fstr_data[0] == 'a');
    LOG("  [0] = '%c' (expected 'a')", fstr_data[0]);
    CHECK(fstr_data[1] == '\n');
    LOG("  [1] = 0x%02x (expected 0x0a = LF)", fstr_data[1]);
    CHECK(fstr_data[2] == 'b');
    LOG("  [2] = '%c' (expected 'b')", fstr_data[2]);
    CHECK(fstr_data[3] == '\t');
    LOG("  [3] = 0x%02x (expected 0x09 = TAB)", fstr_data[3]);
    CHECK(fstr_data[4] == 'c');
    LOG("  [4] = '%c' (expected 'c')", fstr_data[4]);

    LOG("Test completed successfully");
    return (TEST_OK);
}

/**
 * @brief Helper to print file contents with a label.
 */
static void print_file(const char *label, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        LOG("[%s: unable to open %s]", label, path);
        return;
    }
    LOG("--- %s (%s) ---", label, path);
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        /* Remove trailing newline for cleaner log output */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }
        LOG("  %s", line);
    }
    fclose(f);
}

TEST_CASE(test_json_writer_basic) {
    LOG_SECTION("JSON schema writer");

    const char *in_path = "tests/data/sample.map";
    const char *out_path = "build/schema_tmp.json";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t values[128];
    char str_pool[2048];
    uint16_t str_offsets[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;
    FILE *f;
    char buf[64];

    LOG("Parsing input schema: %s", in_path);
    cfgpack_parse_opts_t opts = {&schema,     entries,  128,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 128,      &err};
    rc = cfgpack_parse_schema_file(in_path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_OK);
    LOG("Parse succeeded");

    cfgpack_ctx_t ctx;
    cfgpack_init(&ctx, &schema, values, 128, str_pool, sizeof(str_pool),
                 str_offsets, 128);

    LOG("Writing JSON to: %s", out_path);
    rc = cfgpack_schema_write_json_file(&ctx, out_path, scratch,
                                        sizeof(scratch), &err);
    CHECK(rc == CFGPACK_OK);
    LOG("JSON write succeeded");

    print_file("Generated JSON", out_path);

    LOG("Verifying JSON starts with '{'");
    f = fopen(out_path, "r");
    CHECK(f != NULL);
    CHECK(fgets(buf, sizeof(buf), f) != NULL);
    CHECK(buf[0] == '{');
    fclose(f);
    LOG("First char is '{' (correct)");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_json_roundtrip) {
    LOG_SECTION("JSON roundtrip: .map -> JSON -> parse -> JSON");

    const char *map_path = "tests/data/sample.map";
    const char *json_path = "build/roundtrip.json";
    const char *json_path2 = "build/roundtrip2.json";
    cfgpack_schema_t schema1, schema2;
    cfgpack_entry_t entries1[128], entries2[128];
    cfgpack_value_t values1[128], values2[128];
    char str_pool1[2048], str_pool2[2048];
    uint16_t str_offsets1[128], str_offsets2[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Step 1: Parse .map file");
    cfgpack_parse_opts_t opts1 = {&schema1,     entries1,  128,
                                  values1,      str_pool1, sizeof(str_pool1),
                                  str_offsets1, 128,       &err};
    rc = cfgpack_parse_schema_file(map_path, &opts1, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_OK);
    LOG("Parsed: name='%s', version=%u, entries=%zu", schema1.map_name,
        schema1.version, schema1.entry_count);

    cfgpack_ctx_t ctx1;
    cfgpack_init(&ctx1, &schema1, values1, 128, str_pool1, sizeof(str_pool1),
                 str_offsets1, 128);

    LOG("Step 2: Write to JSON");
    rc = cfgpack_schema_write_json_file(&ctx1, json_path, scratch,
                                        sizeof(scratch), &err);
    CHECK(rc == CFGPACK_OK);
    LOG("JSON written to: %s", json_path);
    print_file("Generated JSON (from .map)", json_path);

    LOG("Step 3: Parse JSON back");
    cfgpack_parse_opts_t opts2 = {&schema2,     entries2,  128,
                                  values2,      str_pool2, sizeof(str_pool2),
                                  str_offsets2, 128,       &err};
    rc = cfgpack_schema_parse_json_file(json_path, &opts2, scratch,
                                        sizeof(scratch));
    if (rc != CFGPACK_OK) {
        LOG("JSON parse error: %s (line %zu)", err.message, err.line);
    }
    CHECK(rc == CFGPACK_OK);
    LOG("JSON parsed successfully");

    LOG("Step 4: Verify schemas match");
    CHECK(strcmp(schema1.map_name, schema2.map_name) == 0);
    LOG("  name: '%s' == '%s'", schema1.map_name, schema2.map_name);
    CHECK(schema1.version == schema2.version);
    LOG("  version: %u == %u", schema1.version, schema2.version);
    CHECK(schema1.entry_count == schema2.entry_count);
    LOG("  entry_count: %zu == %zu", schema1.entry_count, schema2.entry_count);

    LOG("Verifying all entries match...");
    for (size_t i = 0; i < schema1.entry_count; ++i) {
        CHECK(entries1[i].index == entries2[i].index);
        CHECK(strcmp(entries1[i].name, entries2[i].name) == 0);
        CHECK(entries1[i].type == entries2[i].type);
        CHECK(entries1[i].has_default == entries2[i].has_default);

        if (entries1[i].has_default) {
            switch (entries1[i].type) {
            case CFGPACK_TYPE_U8:
            case CFGPACK_TYPE_U16:
            case CFGPACK_TYPE_U32:
            case CFGPACK_TYPE_U64:
                CHECK(values1[i].v.u64 == values2[i].v.u64);
                break;
            case CFGPACK_TYPE_I8:
            case CFGPACK_TYPE_I16:
            case CFGPACK_TYPE_I32:
            case CFGPACK_TYPE_I64:
                CHECK(values1[i].v.i64 == values2[i].v.i64);
                break;
            case CFGPACK_TYPE_F32:
                CHECK(values1[i].v.f32 == values2[i].v.f32);
                break;
            case CFGPACK_TYPE_F64:
                CHECK(values1[i].v.f64 == values2[i].v.f64);
                break;
            case CFGPACK_TYPE_STR:
                CHECK(values1[i].v.str.len == values2[i].v.str.len);
                CHECK(strncmp(str_pool1 + values1[i].v.str.offset,
                              str_pool2 + values2[i].v.str.offset,
                              values1[i].v.str.len) == 0);
                break;
            case CFGPACK_TYPE_FSTR:
                CHECK(values1[i].v.fstr.len == values2[i].v.fstr.len);
                CHECK(strncmp(str_pool1 + values1[i].v.fstr.offset,
                              str_pool2 + values2[i].v.fstr.offset,
                              values1[i].v.fstr.len) == 0);
                break;
            }
        }
    }
    LOG("All %zu entries verified", schema1.entry_count);

    cfgpack_ctx_t ctx2;
    cfgpack_init(&ctx2, &schema2, values2, 128, str_pool2, sizeof(str_pool2),
                 str_offsets2, 128);

    LOG("Step 5: Write JSON again from parsed schema");
    rc = cfgpack_schema_write_json_file(&ctx2, json_path2, scratch,
                                        sizeof(scratch), &err);
    CHECK(rc == CFGPACK_OK);
    LOG("Re-generated JSON written to: %s", json_path2);
    print_file("Re-generated JSON (from parsed JSON)", json_path2);

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_json_parse_direct) {
    LOG_SECTION("Parse JSON schema directly");

    const char *json_path = "tests/data/test_schema.json";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t values[128];
    char str_pool[2048];
    uint16_t str_offsets[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Creating JSON schema file");
    const char *json_content = "{\n"
                               "  \"name\": \"test\",\n"
                               "  \"version\": 42,\n"
                               "  \"entries\": [\n"
                               "    {\"index\": 1, \"name\": \"speed\", "
                               "\"type\": \"u16\", \"value\": 100},\n"
                               "    {\"index\": 2, \"name\": \"name\", "
                               "\"type\": \"fstr\", \"value\": \"hello\"},\n"
                               "    {\"index\": 3, \"name\": \"temp\", "
                               "\"type\": \"i8\", \"value\": -10},\n"
                               "    {\"index\": 4, \"name\": \"ratio\", "
                               "\"type\": \"f32\", \"value\": 3.14},\n"
                               "    {\"index\": 5, \"name\": \"desc\", "
                               "\"type\": \"str\", \"value\": null}\n"
                               "  ]\n"
                               "}\n";

    CHECK(write_file(json_path, json_content) == TEST_OK);
    print_file("Input JSON", json_path);

    LOG("Parsing JSON");
    cfgpack_parse_opts_t opts = {&schema,     entries,  128,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 128,      &err};
    rc = cfgpack_schema_parse_json_file(json_path, &opts, scratch,
                                        sizeof(scratch));
    if (rc != CFGPACK_OK) {
        LOG("JSON parse error: %s (line %zu)", err.message, err.line);
    }
    CHECK(rc == CFGPACK_OK);
    LOG("Parse succeeded");

    LOG("Verifying schema metadata:");
    CHECK(strcmp(schema.map_name, "test") == 0);
    LOG("  name = '%s' (expected 'test')", schema.map_name);
    CHECK(schema.version == 42);
    LOG("  version = %u (expected 42)", schema.version);
    CHECK(schema.entry_count == 5);
    LOG("  entry_count = %zu (expected 5)", schema.entry_count);

    LOG("Verifying entry 0: speed u16 100");
    CHECK(entries[0].index == 1);
    CHECK(strcmp(entries[0].name, "speed") == 0);
    CHECK(entries[0].type == CFGPACK_TYPE_U16);
    CHECK(entries[0].has_default == 1);
    CHECK(values[0].v.u64 == 100);
    LOG("  index=%u, name='%s', type=u16, default=%" PRIu64, entries[0].index,
        entries[0].name, values[0].v.u64);

    LOG("Verifying entry 1: name fstr \"hello\"");
    CHECK(entries[1].index == 2);
    CHECK(strcmp(entries[1].name, "name") == 0);
    CHECK(entries[1].type == CFGPACK_TYPE_FSTR);
    CHECK(entries[1].has_default == 1);
    CHECK(strncmp(str_pool + values[1].v.fstr.offset, "hello",
                  values[1].v.fstr.len) == 0);
    LOG("  index=%u, name='%s', type=fstr, default='%.*s'", entries[1].index,
        entries[1].name, (int)values[1].v.fstr.len,
        str_pool + values[1].v.fstr.offset);

    LOG("Verifying entry 2: temp i8 -10");
    CHECK(entries[2].index == 3);
    CHECK(strcmp(entries[2].name, "temp") == 0);
    CHECK(entries[2].type == CFGPACK_TYPE_I8);
    CHECK(entries[2].has_default == 1);
    CHECK(values[2].v.i64 == -10);
    LOG("  index=%u, name='%s', type=i8, default=%" PRId64, entries[2].index,
        entries[2].name, values[2].v.i64);

    LOG("Verifying entry 3: ratio f32 3.14");
    CHECK(entries[3].index == 4);
    CHECK(strcmp(entries[3].name, "ratio") == 0);
    CHECK(entries[3].type == CFGPACK_TYPE_F32);
    CHECK(entries[3].has_default == 1);
    CHECK(values[3].v.f32 > 3.13f && values[3].v.f32 < 3.15f);
    LOG("  index=%u, name='%s', type=f32, default=%f", entries[3].index,
        entries[3].name, (double)values[3].v.f32);

    LOG("Verifying entry 4: desc str null (no default)");
    CHECK(entries[4].index == 5);
    CHECK(strcmp(entries[4].name, "desc") == 0);
    CHECK(entries[4].type == CFGPACK_TYPE_STR);
    CHECK(entries[4].has_default == 0);
    LOG("  index=%u, name='%s', type=str, has_default=%d", entries[4].index,
        entries[4].name, entries[4].has_default);

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_reserved_index_zero_map) {
    LOG_SECTION("Reserved index 0 in .map file");

    const char *path = "tests/data/reserved_index.map";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t values[8];
    char str_pool[256];
    uint16_t str_offsets[8];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Creating .map file with index 0 (reserved for schema name)");
    CHECK(write_file(path, "test 1\n0 foo u8 0\n") == TEST_OK);
    LOG("File contents: '0 foo u8 0' (index 0 is reserved)");

    LOG("Parsing file (expecting CFGPACK_ERR_RESERVED_INDEX)");
    cfgpack_parse_opts_t opts = {&schema,     entries,  8,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 8,        &err};
    rc = cfgpack_parse_schema_file(path, &opts, scratch, sizeof(scratch));
    CHECK(rc == CFGPACK_ERR_RESERVED_INDEX);
    LOG("Correctly rejected: CFGPACK_ERR_RESERVED_INDEX");

    LOG("Test completed successfully");
    return (TEST_OK);
}

TEST_CASE(test_reserved_index_zero_json) {
    LOG_SECTION("Reserved index 0 in JSON file");

    const char *json_path = "tests/data/reserved_index.json";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[8];
    cfgpack_value_t values[8];
    char str_pool[256];
    uint16_t str_offsets[8];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    LOG("Creating JSON file with index 0 (reserved)");
    const char *json_content = "{\n"
                               "  \"name\": \"test\",\n"
                               "  \"version\": 1,\n"
                               "  \"entries\": [\n"
                               "    {\"index\": 0, \"name\": \"foo\", "
                               "\"type\": \"u8\", \"value\": 0}\n"
                               "  ]\n"
                               "}\n";

    CHECK(write_file(json_path, json_content) == TEST_OK);
    print_file("Input JSON", json_path);

    LOG("Parsing JSON (expecting CFGPACK_ERR_RESERVED_INDEX)");
    cfgpack_parse_opts_t opts = {&schema,     entries,  8,
                                 values,      str_pool, sizeof(str_pool),
                                 str_offsets, 8,        &err};
    rc = cfgpack_schema_parse_json_file(json_path, &opts, scratch,
                                        sizeof(scratch));
    CHECK(rc == CFGPACK_ERR_RESERVED_INDEX);
    LOG("Correctly rejected: CFGPACK_ERR_RESERVED_INDEX");

    LOG("Test completed successfully");
    return (TEST_OK);
}

int main(void) {
    test_result_t overall = TEST_OK;

    overall |= (test_case_result("dup_name", test_duplicate_name()) != TEST_OK);
    overall |= (test_case_result("name_too_long", test_name_too_long()) !=
                TEST_OK);
    overall |= (test_case_result("index_too_large", test_index_too_large()) !=
                TEST_OK);
    overall |= (test_case_result("missing_header", test_missing_header()) !=
                TEST_OK);
    overall |= (test_case_result("missing_fields", test_missing_fields()) !=
                TEST_OK);
    overall |= (test_case_result("too_many_entries", test_too_many_entries()) !=
                TEST_OK);
    overall |= (test_case_result("accept_128_entries",
                                 test_accept_128_entries()) != TEST_OK);
    overall |= (test_case_result("unknown_type", test_unknown_type()) !=
                TEST_OK);
    overall |= (test_case_result("header_non_numeric_version",
                                 test_header_non_numeric_version()) != TEST_OK);
    overall |= (test_case_result("name_length_edges",
                                 test_name_length_edges()) != TEST_OK);
    overall |= (test_case_result("index_edges", test_index_edges()) != TEST_OK);
    overall |= (test_case_result("unsorted_input_sorted_output",
                                 test_unsorted_input_sorted_output()) !=
                TEST_OK);
    overall |= (test_case_result("default_u8_out_of_range",
                                 test_default_u8_out_of_range()) != TEST_OK);
    overall |= (test_case_result("default_i8_out_of_range",
                                 test_default_i8_out_of_range()) != TEST_OK);
    overall |= (test_case_result("default_fstr_too_long",
                                 test_default_fstr_too_long()) != TEST_OK);
    overall |= (test_case_result("default_hex_binary_literals",
                                 test_default_hex_binary_literals()) !=
                TEST_OK);
    overall |= (test_case_result("default_invalid_format",
                                 test_default_invalid_format()) != TEST_OK);
    overall |= (test_case_result("default_escape_sequences",
                                 test_default_escape_sequences()) != TEST_OK);
    overall |= (test_case_result("json_writer_basic",
                                 test_json_writer_basic()) != TEST_OK);
    overall |= (test_case_result("json_roundtrip", test_json_roundtrip()) !=
                TEST_OK);
    overall |= (test_case_result("json_parse_direct",
                                 test_json_parse_direct()) != TEST_OK);
    overall |= (test_case_result("reserved_index_zero_map",
                                 test_reserved_index_zero_map()) != TEST_OK);
    overall |= (test_case_result("reserved_index_zero_json",
                                 test_reserved_index_zero_json()) != TEST_OK);

    if (overall == TEST_OK) {
        printf(COLOR_GREEN "ALL PASS" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "SOME FAIL" COLOR_RESET "\n");
    }
    return (overall);
}
