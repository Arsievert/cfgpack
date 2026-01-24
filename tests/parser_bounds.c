/* Schema parser edge-case tests. */

#include "cfgpack/cfgpack.h"
#include "cfgpack/io_file.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static char scratch[16384];  /* Scratch buffer for file parsing */

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
    rc = cfgpack_parse_schema_file(path, &schema, entries, 128, defaults, scratch, sizeof(scratch), &err);
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
    rc = cfgpack_parse_schema_file(path, &schema, entries, 128, defaults, scratch, sizeof(scratch), &err);
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
    rc = cfgpack_parse_schema_file(path, &schema, entries, 128, defaults, scratch, sizeof(scratch), &err);
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
    rc = cfgpack_parse_schema_file(path, &schema, entries, 128, defaults, scratch, sizeof(scratch), &err);
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
    rc = cfgpack_parse_schema_file(path, &schema, entries, 128, defaults, scratch, sizeof(scratch), &err);
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

    rc = cfgpack_parse_schema_file(path, &schema, entries, 128, defaults, scratch, sizeof(scratch), &err);
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

    rc = cfgpack_parse_schema_file(path, &schema, entries, 128, defaults, scratch, sizeof(scratch), &err);
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
    rc = cfgpack_parse_schema_file(path, &schema, entries, 128, defaults, scratch, sizeof(scratch), &err);
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
    rc = cfgpack_parse_schema_file(path, &schema, entries, 8, defaults, scratch, sizeof(scratch), &err);
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
    rc = cfgpack_parse_schema_file(ok_path, &schema, entries, 8, defaults, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_OK);
    CHECK(schema.entry_count == 1);

    CHECK(write_file(bad_path, "demo 1\n0 abcdef u8 0  # 6 chars - too long\n") == TEST_OK);
    rc = cfgpack_parse_schema_file(bad_path, &schema, entries, 8, defaults, scratch, sizeof(scratch), &err);
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
    rc = cfgpack_parse_schema_file(ok_path, &schema, entries, 8, defaults, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_OK);
    CHECK(schema.entries[0].index == 65535);

    CHECK(write_file(bad_path, "demo 1\n65536 a u8 0  # out of range\n") == TEST_OK);
    rc = cfgpack_parse_schema_file(bad_path, &schema, entries, 8, defaults, scratch, sizeof(scratch), &err);
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
    rc = cfgpack_parse_schema_file(path, &schema, entries, 4, defaults, scratch, sizeof(scratch), &err);
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

    rc = cfgpack_parse_schema_file(in_path, &schema, entries, 128, defaults, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_OK);
    rc = cfgpack_schema_write_markdown_file(&schema, defaults, out_path, scratch, sizeof(scratch), &err);
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
    rc = cfgpack_parse_schema_file(path, &schema, entries, 8, defaults, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_ERR_BOUNDS);

    /* 255 is valid max u8 */
    CHECK(write_file(path, "demo 1\n0 foo u8 255\n") == TEST_OK);
    rc = cfgpack_parse_schema_file(path, &schema, entries, 8, defaults, scratch, sizeof(scratch), &err);
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
    rc = cfgpack_parse_schema_file(path, &schema, entries, 8, defaults, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_ERR_BOUNDS);

    /* -129 is below i8 min (-128) */
    CHECK(write_file(path, "demo 1\n0 foo i8 -129\n") == TEST_OK);
    rc = cfgpack_parse_schema_file(path, &schema, entries, 8, defaults, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_ERR_BOUNDS);

    /* -128 and 127 are valid */
    CHECK(write_file(path, "demo 1\n0 foo i8 -128\n1 bar i8 127\n") == TEST_OK);
    rc = cfgpack_parse_schema_file(path, &schema, entries, 8, defaults, scratch, sizeof(scratch), &err);
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
    rc = cfgpack_parse_schema_file(path, &schema, entries, 8, defaults, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_ERR_STR_TOO_LONG);

    /* 16 chars should be OK */
    CHECK(write_file(path, "demo 1\n0 foo fstr \"1234567890123456\"\n") == TEST_OK);
    rc = cfgpack_parse_schema_file(path, &schema, entries, 8, defaults, scratch, sizeof(scratch), &err);
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
    rc = cfgpack_parse_schema_file(path, &schema, entries, 8, defaults, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_OK);
    CHECK(defaults[0].v.u64 == 0xFF);
    CHECK(defaults[1].v.u64 == 0xABCD);

    /* Hex value out of range for u8 */
    CHECK(write_file(path, "demo 1\n0 foo u8 0x100\n") == TEST_OK);
    rc = cfgpack_parse_schema_file(path, &schema, entries, 8, defaults, scratch, sizeof(scratch), &err);
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
    rc = cfgpack_parse_schema_file(path, &schema, entries, 8, defaults, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_ERR_PARSE);

    /* Non-quoted string for string type */
    CHECK(write_file(path, "demo 1\n0 foo str hello\n") == TEST_OK);
    rc = cfgpack_parse_schema_file(path, &schema, entries, 8, defaults, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_ERR_PARSE);

    /* Unterminated string */
    CHECK(write_file(path, "demo 1\n0 foo str \"hello\n") == TEST_OK);
    rc = cfgpack_parse_schema_file(path, &schema, entries, 8, defaults, scratch, sizeof(scratch), &err);
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
    rc = cfgpack_parse_schema_file(path, &schema, entries, 8, defaults, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_OK);
    CHECK(defaults[0].v.fstr.len == 5);
    CHECK(defaults[0].v.fstr.data[0] == 'a');
    CHECK(defaults[0].v.fstr.data[1] == '\n');
    CHECK(defaults[0].v.fstr.data[2] == 'b');
    CHECK(defaults[0].v.fstr.data[3] == '\t');
    CHECK(defaults[0].v.fstr.data[4] == 'c');
    return (TEST_OK);
}

/**
 * @brief Helper to print file contents with a label.
 */
static void print_file(const char *label, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        printf("  [%s: unable to open %s]\n", label, path);
        return;
    }
    printf("  --- %s (%s) ---\n", label, path);
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        printf("  %s", line);
    }
    fclose(f);
    printf("\n");
}

TEST_CASE(test_json_writer_basic) {
    const char *in_path = "tests/data/sample.map";
    const char *out_path = "build/schema_tmp.json";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t defaults[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;
    FILE *f;
    char buf[64];

    /* Parse .map file */
    rc = cfgpack_parse_schema_file(in_path, &schema, entries, 128, defaults, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_OK);

    /* Write JSON */
    rc = cfgpack_schema_write_json_file(&schema, defaults, out_path, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_OK);

    /* Print generated JSON */
    print_file("Generated JSON", out_path);

    /* Verify JSON structure */
    f = fopen(out_path, "r");
    CHECK(f != NULL);
    CHECK(fgets(buf, sizeof(buf), f) != NULL);
    CHECK(buf[0] == '{'); /* starts with { */
    fclose(f);

    return (TEST_OK);
}

TEST_CASE(test_json_roundtrip) {
    const char *map_path = "tests/data/sample.map";
    const char *json_path = "build/roundtrip.json";
    const char *json_path2 = "build/roundtrip2.json";
    cfgpack_schema_t schema1, schema2;
    cfgpack_entry_t entries1[128], entries2[128];
    cfgpack_value_t defaults1[128], defaults2[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    /* Step 1: Parse .map file */
    rc = cfgpack_parse_schema_file(map_path, &schema1, entries1, 128, defaults1, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_OK);

    /* Step 2: Write to JSON */
    rc = cfgpack_schema_write_json_file(&schema1, defaults1, json_path, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_OK);

    /* Print the generated JSON */
    print_file("Generated JSON (from .map)", json_path);

    /* Step 3: Parse JSON back */
    rc = cfgpack_schema_parse_json_file(json_path, &schema2, entries2, 128, defaults2, scratch, sizeof(scratch), &err);
    if (rc != CFGPACK_OK) {
        printf("  JSON parse error: %s (line %zu)\n", err.message, err.line);
    }
    CHECK(rc == CFGPACK_OK);

    /* Step 4: Verify schemas match */
    CHECK(strcmp(schema1.map_name, schema2.map_name) == 0);
    CHECK(schema1.version == schema2.version);
    CHECK(schema1.entry_count == schema2.entry_count);

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
                    CHECK(defaults1[i].v.u64 == defaults2[i].v.u64);
                    break;
                case CFGPACK_TYPE_I8:
                case CFGPACK_TYPE_I16:
                case CFGPACK_TYPE_I32:
                case CFGPACK_TYPE_I64:
                    CHECK(defaults1[i].v.i64 == defaults2[i].v.i64);
                    break;
                case CFGPACK_TYPE_F32:
                    CHECK(defaults1[i].v.f32 == defaults2[i].v.f32);
                    break;
                case CFGPACK_TYPE_F64:
                    CHECK(defaults1[i].v.f64 == defaults2[i].v.f64);
                    break;
                case CFGPACK_TYPE_STR:
                    CHECK(defaults1[i].v.str.len == defaults2[i].v.str.len);
                    CHECK(strcmp(defaults1[i].v.str.data, defaults2[i].v.str.data) == 0);
                    break;
                case CFGPACK_TYPE_FSTR:
                    CHECK(defaults1[i].v.fstr.len == defaults2[i].v.fstr.len);
                    CHECK(strcmp(defaults1[i].v.fstr.data, defaults2[i].v.fstr.data) == 0);
                    break;
            }
        }
    }

    /* Step 5: Write JSON again from parsed schema */
    rc = cfgpack_schema_write_json_file(&schema2, defaults2, json_path2, scratch, sizeof(scratch), &err);
    CHECK(rc == CFGPACK_OK);

    /* Print the re-generated JSON */
    print_file("Re-generated JSON (from parsed JSON)", json_path2);

    return (TEST_OK);
}

TEST_CASE(test_json_parse_direct) {
    const char *json_path = "tests/data/test_schema.json";
    cfgpack_schema_t schema;
    cfgpack_entry_t entries[128];
    cfgpack_value_t defaults[128];
    cfgpack_parse_error_t err;
    cfgpack_err_t rc;

    /* Create a JSON schema file directly */
    const char *json_content =
        "{\n"
        "  \"name\": \"test\",\n"
        "  \"version\": 42,\n"
        "  \"entries\": [\n"
        "    {\"index\": 0, \"name\": \"speed\", \"type\": \"u16\", \"default\": 100},\n"
        "    {\"index\": 1, \"name\": \"name\", \"type\": \"fstr\", \"default\": \"hello\"},\n"
        "    {\"index\": 2, \"name\": \"temp\", \"type\": \"i8\", \"default\": -10},\n"
        "    {\"index\": 3, \"name\": \"ratio\", \"type\": \"f32\", \"default\": 3.14},\n"
        "    {\"index\": 4, \"name\": \"desc\", \"type\": \"str\", \"default\": null}\n"
        "  ]\n"
        "}\n";

    CHECK(write_file(json_path, json_content) == TEST_OK);

    /* Print the input JSON */
    print_file("Input JSON", json_path);

    /* Parse JSON */
    rc = cfgpack_schema_parse_json_file(json_path, &schema, entries, 128, defaults, scratch, sizeof(scratch), &err);
    if (rc != CFGPACK_OK) {
        printf("  JSON parse error: %s (line %zu)\n", err.message, err.line);
    }
    CHECK(rc == CFGPACK_OK);

    /* Verify parsed values */
    CHECK(strcmp(schema.map_name, "test") == 0);
    CHECK(schema.version == 42);
    CHECK(schema.entry_count == 5);

    /* Entry 0: speed u16 100 */
    CHECK(entries[0].index == 0);
    CHECK(strcmp(entries[0].name, "speed") == 0);
    CHECK(entries[0].type == CFGPACK_TYPE_U16);
    CHECK(entries[0].has_default == 1);
    CHECK(defaults[0].v.u64 == 100);

    /* Entry 1: name fstr "hello" */
    CHECK(entries[1].index == 1);
    CHECK(strcmp(entries[1].name, "name") == 0);
    CHECK(entries[1].type == CFGPACK_TYPE_FSTR);
    CHECK(entries[1].has_default == 1);
    CHECK(strcmp(defaults[1].v.fstr.data, "hello") == 0);

    /* Entry 2: temp i8 -10 */
    CHECK(entries[2].index == 2);
    CHECK(strcmp(entries[2].name, "temp") == 0);
    CHECK(entries[2].type == CFGPACK_TYPE_I8);
    CHECK(entries[2].has_default == 1);
    CHECK(defaults[2].v.i64 == -10);

    /* Entry 3: ratio f32 3.14 */
    CHECK(entries[3].index == 3);
    CHECK(strcmp(entries[3].name, "ratio") == 0);
    CHECK(entries[3].type == CFGPACK_TYPE_F32);
    CHECK(entries[3].has_default == 1);
    CHECK(defaults[3].v.f32 > 3.13f && defaults[3].v.f32 < 3.15f);

    /* Entry 4: desc str null */
    CHECK(entries[4].index == 4);
    CHECK(strcmp(entries[4].name, "desc") == 0);
    CHECK(entries[4].type == CFGPACK_TYPE_STR);
    CHECK(entries[4].has_default == 0);

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
    overall |= (test_case_result("json_writer_basic", test_json_writer_basic()) != TEST_OK);
    overall |= (test_case_result("json_roundtrip", test_json_roundtrip()) != TEST_OK);
    overall |= (test_case_result("json_parse_direct", test_json_parse_direct()) != TEST_OK);

    if (overall == TEST_OK) {
        printf(COLOR_GREEN "ALL PASS" COLOR_RESET "\n");
    } else {
        printf(COLOR_RED "SOME FAIL" COLOR_RESET "\n");
    }
    return (overall);
}
