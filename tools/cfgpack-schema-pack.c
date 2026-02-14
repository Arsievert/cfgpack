/**
 * @file cfgpack-schema-pack.c
 * @brief CLI tool for converting .map or JSON schemas to MessagePack binary.
 *
 * Usage:
 *   cfgpack-schema-pack <input> <output>
 *
 * The input file format is auto-detected:
 *   - Files ending in ".json" are parsed as JSON schemas.
 *   - All other files are parsed as .map schemas.
 *
 * The output file contains raw MessagePack binary data suitable for
 * on-device parsing with cfgpack_schema_parse_msgpack().
 *
 * Exit codes:
 *   0 - Success
 *   1 - Usage error
 *   2 - File I/O error
 *   3 - Parse/encode error
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "cfgpack/cfgpack.h"
#include "cfgpack/msgpack.h"
#include "cfgpack/io_file.h"

#define MAX_INPUT_SIZE (64 * 1024)  /* 64 KB max input */
#define MAX_OUTPUT_SIZE (64 * 1024) /* 64 KB max output */
#define MAX_ENTRIES 256
#define MAX_STR_OFFSETS 256

static char scratch[MAX_INPUT_SIZE];
static uint8_t output_buf[MAX_OUTPUT_SIZE];

static cfgpack_entry_t entries[MAX_ENTRIES];
static cfgpack_value_t values[MAX_ENTRIES];
static char str_pool[MAX_STR_OFFSETS * (CFGPACK_STR_MAX + 1)];
static uint16_t str_offsets[MAX_STR_OFFSETS];

static void print_usage(const char *prog) {
    fprintf(stderr, "Usage: %s <input> <output>\n", prog);
    fprintf(stderr, "\n");
    fprintf(stderr, "Converts a .map or JSON schema to MessagePack binary.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Input format:\n");
    fprintf(stderr, "  .json files  - Parsed as JSON schema\n");
    fprintf(stderr, "  Other files  - Parsed as .map schema\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Output: raw msgpack binary for on-device parsing.\n");
}

static int has_suffix(const char *str, const char *suffix) {
    size_t str_len = strlen(str);
    size_t suf_len = strlen(suffix);
    if (suf_len > str_len) {
        return 0;
    }
    return strcmp(str + str_len - suf_len, suffix) == 0;
}

int main(int argc, char *argv[]) {
    FILE *fout = NULL;
    cfgpack_schema_t schema;
    cfgpack_schema_measure_t m;
    cfgpack_parse_error_t perr;
    cfgpack_ctx_t ctx;
    cfgpack_err_t rc;
    int is_json;
    size_t out_len = 0;

    if (argc != 3) {
        print_usage(argv[0]);
        return 1;
    }

    const char *input_path = argv[1];
    const char *output_path = argv[2];

    is_json = has_suffix(input_path, ".json");

    /* Phase 1: measure */
    if (is_json) {
        rc = cfgpack_schema_measure_json_file(input_path, &m, scratch,
                                              sizeof(scratch), &perr);
    } else {
        rc = cfgpack_schema_measure_file(input_path, &m, scratch,
                                         sizeof(scratch), &perr);
    }
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Measure failed: %s\n", perr.message);
        return 3;
    }

    if (m.entry_count > MAX_ENTRIES) {
        fprintf(stderr, "Too many entries: %zu (max %d)\n", m.entry_count,
                MAX_ENTRIES);
        return 3;
    }
    if (m.str_count + m.fstr_count > MAX_STR_OFFSETS) {
        fprintf(stderr, "Too many string entries: %zu (max %d)\n",
                m.str_count + m.fstr_count, MAX_STR_OFFSETS);
        return 3;
    }

    /* Phase 2: parse */
    if (is_json) {
        rc = cfgpack_schema_parse_json_file(input_path, &schema, entries,
                                            m.entry_count, values, str_pool,
                                            m.str_pool_size, str_offsets,
                                            m.str_count + m.fstr_count, scratch,
                                            sizeof(scratch), &perr);
    } else {
        rc = cfgpack_parse_schema_file(input_path, &schema, entries,
                                       m.entry_count, values, str_pool,
                                       m.str_pool_size, str_offsets,
                                       m.str_count + m.fstr_count, scratch,
                                       sizeof(scratch), &perr);
    }
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Parse failed: %s\n", perr.message);
        return 3;
    }

    /* Phase 3: init context */
    rc = cfgpack_init(&ctx, &schema, values, schema.entry_count, str_pool,
                      m.str_pool_size, str_offsets, m.str_count + m.fstr_count);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Init failed (error %d)\n", rc);
        return 3;
    }

    /* Phase 4: write msgpack */
    rc = cfgpack_schema_write_msgpack(&ctx, output_buf, sizeof(output_buf),
                                      &out_len, &perr);
    if (rc != CFGPACK_OK) {
        fprintf(stderr, "Encode failed: %s\n", perr.message);
        return 3;
    }

    /* Phase 5: write output file */
    fout = fopen(output_path, "wb");
    if (!fout) {
        fprintf(stderr, "Cannot open output file: %s\n", output_path);
        return 2;
    }

    if (fwrite(output_buf, 1, out_len, fout) != out_len) {
        fprintf(stderr, "Error writing output file\n");
        fclose(fout);
        return 2;
    }
    fclose(fout);

    /* Print stats */
    printf("Schema: \"%s\" v%u (%zu entries)\n", schema.map_name,
           schema.version, schema.entry_count);
    printf("Output: %zu bytes -> %s\n", out_len, output_path);

    return 0;
}
